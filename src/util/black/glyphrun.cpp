#include "glyphrun.h"
#include <cctype>

namespace glyphrun {

static constexpr uint32_t RI_FIRST = 0x1F1E6, RI_LAST = 0x1F1FF, ZWJ = 0x200D;

uint32_t flagKey(uint32_t ri1, uint32_t ri2) {
    return 0x110000 + (ri1 - RI_FIRST) * 26 + (ri2 - RI_FIRST);
}

static bool isRegional(uint32_t c) { return c >= RI_FIRST && c <= RI_LAST; }

// Parts of an emoji sequence that never draw on their own: variation selectors, keycap mark,
// skin tones, tags.
static bool isModifier(uint32_t c) {
    return c == 0xFE0E || c == 0xFE0F || c == 0x20E3 || (c >= 0x1F3FB && c <= 0x1F3FF) ||
           (c >= 0xE0020 && c <= 0xE007F);
}

// Length of the multi-byte UTF-8 character at s (2..4), or 0 for a stray/invalid/cut byte.
static int seqLen(const char* s) {
    unsigned char c = (unsigned char)s[0];
    int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    for (int k = 1; k < len; ++k)
        if (((unsigned char)s[k] & 0xC0) != 0x80) return 0;   // also stops at the NUL
    return len;
}

static uint32_t codepoint(const char* s, int len) {
    uint32_t cp = (unsigned char)s[0] & (len == 3 ? 0x0F : 0x07);
    for (int k = 1; k < len; ++k) cp = (cp << 6) | ((unsigned char)s[k] & 0x3F);
    return cp;
}

// The joined sequence that starts with `first`, its next part at s: fills cps (first + each
// codepoint after a ZWJ, modifiers skipped) and returns how many, with *end just past it.
// 0 when no ZWJ follows or the sequence is longer than JOIN_MAX.
static constexpr int JOIN_MAX = 4;   // emojifont::SEQ_MAX
static int joined(uint32_t first, const char* s, uint32_t* cps, size_t* end) {
    int m = 0;
    size_t j = 0;
    cps[m++] = first;
    for (;;) {
        size_t k = j;
        int l;
        while ((l = seqLen(s + k)) != 0 && isModifier(codepoint(s + k, l))) k += l;
        if (l == 0 || codepoint(s + k, l) != ZWJ) break;
        k += l;
        if ((l = seqLen(s + k)) == 0) break;              // ZWJ before ASCII: not joined
        if (m == JOIN_MAX) return 0;
        cps[m++] = codepoint(s + k, l);
        j = k + l;
    }
    *end = j;
    return m > 1 ? m : 0;
}

size_t decode(const char* s, Item* out, size_t cap, AsciiWidthFn aw, WideWidthFn ww, void* ctx,
              bool upper, EmojiWidthFn ew, EmojiSeqFn sq) {
    size_t n = 0;
    if (!s) return 0;
    for (size_t i = 0; s[i] && n < cap;) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {                                   // ASCII
            char ch = upper ? (char)toupper(c) : (char)c;
            out[n++] = {Kind::Ascii, ch, txt::Mark::None, c, (uint8_t)aw(ch, ctx)};
            ++i;
            continue;
        }
        int len = seqLen(s + i);
        if (len == 0) { ++i; continue; }                  // stray/invalid byte
        if (len == 2) {                                   // Latin-1 letters -> base + mark
            char seq[3] = {s[i], s[i + 1], 0}, base[4];
            txt::Mark marks[4];
            size_t m = txt::foldMarks(seq, base, marks, sizeof(base));
            for (size_t k = 0; k < m && n < cap; ++k) {
                char ch = upper ? (char)toupper((unsigned char)base[k]) : base[k];
                out[n++] = {Kind::Ascii, ch, marks[k], (uint32_t)(unsigned char)base[k],
                            (uint8_t)aw(ch, ctx)};
            }
            i += len;
            continue;
        }
        const uint32_t cp = codepoint(s + i, len);
        i += len;
        if (isModifier(cp)) continue;
        if (cp == ZWJ) {                                  // joined emoji: keep only the first
            if ((unsigned char)s[i] >= 0x80) { int l = seqLen(s + i); i += l ? l : 1; }
            continue;
        }
        if (sq && ew) {                                   // a joined emoji the font has
            uint32_t cps[JOIN_MAX];
            size_t end = 0;
            const int m = joined(cp, s + i, cps, &end);
            const uint32_t k = m ? sq(cps, m, ctx) : 0;
            const int w = k ? ew(k, ctx) : 0;
            if (w > 0) {
                out[n++] = {Kind::Emoji, 0, txt::Mark::None, k, (uint8_t)w};
                i += end;
                continue;
            }
        }
        uint32_t key = cp;
        if (isRegional(cp)) {                             // flag = a pair; half of one is dropped
            int l = seqLen(s + i);
            if (l != 4 || !isRegional(codepoint(s + i, l))) continue;
            key = flagKey(cp, codepoint(s + i, l));
            i += l;
        }
        int w = ew ? ew(key, ctx) : 0;
        if (w > 0) { out[n++] = {Kind::Emoji, 0, txt::Mark::None, key, (uint8_t)w}; continue; }
        w = len == 3 ? ww(cp, ctx) : 0;
        if (w > 0) out[n++] = {Kind::Wide, 0, txt::Mark::None, cp, (uint8_t)w};
    }
    return n;
}

int width(const Item* it, size_t n) {
    int w = 0;
    for (size_t i = 0; i < n; ++i) w += it[i].w;
    return w;
}

size_t fit(Item* it, size_t n, size_t cap, int maxW, AsciiWidthFn aw, void* ctx) {
    if (width(it, n) <= maxW) return n;
    int dot = aw('.', ctx);
    while (n > 0 && width(it, n) + 3 * dot > maxW) --n;
    for (int k = 0; k < 3 && n < cap; ++k)
        it[n++] = {Kind::Ascii, '.', txt::Mark::None, '.', (uint8_t)dot};
    return n;
}

// Number of items from `start` that fit in maxW.
static size_t fitCount(const Item* it, size_t start, size_t n, int maxW) {
    int w = 0;
    size_t k = start;
    while (k < n && w + it[k].w <= maxW) w += it[k++].w;
    return k - start;
}

static bool isSpace(const Item& x) { return x.kind == Kind::Ascii && x.ch == ' '; }
static bool isPicture(const Item& x) { return x.kind == Kind::Wide || x.kind == Kind::Emoji; }

Wrap wrapTwo(const Item* it, size_t n, int maxW, int ellipsisW) {
    size_t fitA = fitCount(it, 0, n, maxW);
    if (fitA >= n) return {n, n, n, false};
    size_t cut = fitA, next = fitA;                       // hard split by default
    for (size_t k = fitA; k > 0; --k) {
        if (isSpace(it[k])) { cut = k; next = k + 1; break; }          // break at a space
        if (isPicture(it[k]) || isPicture(it[k - 1])) { cut = next = k; break; }
    }
    if (cut == 0) cut = next = fitA;
    Wrap w{cut, next, n, false};
    if (width(it + next, n - next) > maxW) {
        w.bEllipsis = true;
        w.bEnd = next + fitCount(it, next, n, maxW - ellipsisW);
    }
    return w;
}

}
