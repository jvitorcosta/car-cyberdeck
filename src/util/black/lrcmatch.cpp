#include "lrcmatch.h"
#include "../common/text.h"
#include <cstdint>
#include <cstring>

namespace lrcmatch {

using lrcstream::Kind;
using lyricstatus::Result;

static constexpr size_t FOLDED = 96;   // AppState titles are shorter; a longer entry can't be equal

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

static bool startsWith(const char* s, const char* word) {
    for (; *word; ++s, ++word)
        if (lower(*s) != *word) return false;
    return true;
}

static size_t baseLength(const char* t) {
    static const char* const CUTS[] = {" - ", " \xE2\x80\x93 ", " \xE2\x80\x94 ", " (", " [", " feat. ", " ft. "};
    size_t i = 0;
    for (; t[i]; ++i)
        if (i > 0)
            for (const char* cut : CUTS)
                if (startsWith(t + i, cut)) return i;
    return i;
}

size_t baseTitle(const char* title, char* out, size_t n) {
    if (n == 0) return 0;
    if (!title) title = "";
    size_t len = baseLength(title);
    if (len >= n) len = n - 1;
    memcpy(out, title, len);
    out[len] = '\0';
    return len;
}

// Lower-case words split by single spaces; Latin accents folded, other scripts kept.
static size_t foldWords(const char* s, size_t len, char* out, size_t n) {
    size_t j = 0;
    bool gap = false;
    auto put = [&](const char* b, size_t m) {
        if (j + m + (gap && j ? 1 : 0) >= n) return;
        if (gap && j) out[j++] = ' ';
        gap = false;
        for (size_t k = 0; k < m; ++k) out[j++] = lower(b[k]);
    };
    for (size_t i = 0; i < len;) {
        const uint8_t c = (uint8_t)s[i];
        if (c < 0x80) {
            ++i;
            if (c == '\'') continue;
            const char l = lower((char)c);
            if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9')) put(&l, 1);
            else gap = true;
            continue;
        }
        size_t m = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (i + m > len) m = len - i;
        if (m == 3 && memcmp(s + i, "\xE2\x80\x99", 3) == 0) { i += m; continue; }   // curly '
        char one[5], folded[4];
        memcpy(one, s + i, m);
        one[m] = '\0';
        txt::asciiFold(one, folded, sizeof folded);
        if (folded[0]) put(folded, strlen(folded));
        else put(s + i, m);
        i += m;
    }
    out[j] = '\0';
    return j;
}

bool sameTitle(const char* a, const char* b) {
    if (!a || !b) return true;
    char fa[FOLDED], fb[FOLDED];
    const size_t na = foldWords(a, baseLength(a), fa, sizeof fa);
    const size_t nb = foldWords(b, baseLength(b), fb, sizeof fb);
    return na == 0 || nb == 0 || strcmp(fa, fb) == 0;
}

// 5xx and 429 are the server's problem; negative codes are connect/TLS/timeout failures.
static bool failed(const Answer& a) { return a.stalled || a.rc < 0 || a.rc == 429 || a.rc >= 500; }

bool needSearch(const Answer& get) {
    if (get.stalled) return false;
    if (get.rc == 200) return get.kind == Kind::Plain || get.kind == Kind::None;
    return get.rc == 404 || get.rc >= 500;
}

static Result fromKind(Kind k) {
    switch (k) {
        case Kind::Synced:       return Result::Synced;
        case Kind::Plain:        return Result::Plain;
        case Kind::Instrumental: return Result::Instrumental;
        case Kind::None:         break;
    }
    return Result::NotFound;
}

Result settle(const Answer& get, const Answer* search, bool lastAttempt) {
    if (!search) {
        if (failed(get)) return Result::TempError;
        return get.rc == 200 ? fromKind(get.kind) : Result::NotFound;
    }
    if (!failed(*search) && search->kind == Kind::Synced) return Result::Synced;
    if (failed(get) || failed(*search))
        return lastAttempt && search->kind == Kind::Plain ? Result::Plain : Result::TempError;
    if (get.kind == Kind::Plain) return Result::Plain;   // an exact entry beats a search's instrumental
    return fromKind(search->kind);
}

}
