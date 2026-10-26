#include "karaoke.h"
#include <cstring>

namespace karaoke {

int progressMs(const stateparse::Deck& d, uint32_t sincePollMs) {
    uint64_t p = d.progressMs;
    if (d.playing) p += sincePollMs;
    if (d.durationMs > 0 && p > d.durationMs) p = d.durationMs;
    if (p > 0x7FFFFFFF) p = 0x7FFFFFFF;
    return static_cast<int>(p);
}

int cells(uint32_t pos, uint32_t dur, int n) {
    if (dur == 0 || n <= 0) return 0;
    if (pos >= dur) return n;
    return static_cast<int>(static_cast<uint64_t>(pos) * static_cast<uint64_t>(n) / dur);
}

bool available(bool answered, bool playing, const stateparse::Deck& d) {
    return answered && playing && strcmp(d.lyricKind, "synced") == 0 && (d.line[0] != 0 || d.nextStartMs > 0);
}

const char* state(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d) {
    if (wrongKey) return ">WRONG KEY";
    if (!answered) return ">LYRICS: NO SIGNAL";
    if (!playing || !d.track[0]) return ">NOTHING PLAYING";
    if (!available(answered, playing, d)) return ">NO SYNCED LYRICS";
    return "";
}

const char* whyNot(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d) {
    if (wrongKey) return ">WRONG KEY";
    if (!answered) return ">LYRICS: NO SIGNAL";
    if (!playing || !d.track[0]) return ">NOTHING PLAYING";
    return ">NO SYNCED LYRICS";
}

bool isAlert(const char* msg) { return msg && strcmp(msg, ">WRONG KEY") == 0; }

const char* lyric(const stateparse::Deck& d, uint32_t posMs) {
    if (strcmp(d.lyricKind, "synced") != 0) return "";
    if (d.nextStartMs > 0 && posMs >= d.nextStartMs) return d.next;
    if (posMs >= d.lineStartMs) return d.line;
    return "";
}

const char* nextLine(const stateparse::Deck& d, uint32_t posMs) {
    if (strcmp(d.lyricKind, "synced") != 0) return "";
    if (d.nextStartMs > 0 && posMs >= d.nextStartMs) return "";
    if (posMs < d.lineStartMs) return d.line;
    return d.nextStartMs > 0 ? d.next : "";
}

namespace {

constexpr size_t LINE = sizeof(Line::s);

// s[0..len) into out (len < LINE).
void put(char* out, const char* s, size_t len) {
    memcpy(out, s, len);
    out[len] = 0;
}

// Whether s[0..len) fits maxPx; `tmp` (LINE bytes) holds it to measure.
bool fits(const char* s, size_t len, int font, Width width, int maxPx, char* tmp) {
    if (len >= LINE) return false;
    put(tmp, s, len);
    return width(tmp, font) <= maxPx;
}

// The rest of the text on one line, cut with ".." when it doesn't fit (a space before the ".."
// goes). True when cut.
bool fitLast(const char* s, int font, Width width, int maxPx, char* out) {
    size_t len = strlen(s);
    while (len > 0 && s[len - 1] == ' ') --len;
    if (len == 0 || fits(s, len, font, width, maxPx, out)) {
        if (len == 0) out[0] = 0;
        return false;
    }
    size_t keep = len - 1 < LINE - 3 ? len - 1 : LINE - 3;
    for (;;) {
        while (keep > 0 && s[keep - 1] == ' ') --keep;
        memcpy(out, s, keep);
        out[keep] = '.';
        out[keep + 1] = '.';
        out[keep + 2] = 0;
        if (keep == 0 || width(out, font) <= maxPx) return true;  // ".." even when it doesn't fit
        --keep;
    }
}

}  // namespace

int wrap(const char* s, int font, Width width, int maxPx, int maxLines, Line out[], bool* cut) {
    if (cut) *cut = false;
    if (!s || maxLines <= 0) return 0;
    int n = 0;
    const char* p = s;
    while (n < maxLines) {
        while (*p == ' ') ++p;
        if (!*p) break;
        char* o = out[n].s;
        if (n == maxLines - 1) {
            const bool c = fitLast(p, font, width, maxPx, o);
            if (cut) *cut = c;
            return n + 1;
        }
        size_t best = 0, e = 0;  // p[0..best): the whole words that fit so far
        for (;;) {
            while (p[e] == ' ') ++e;
            if (!p[e]) break;
            while (p[e] && p[e] != ' ') ++e;
            if (!fits(p, e, font, width, maxPx, o)) break;
            best = e;
        }
        if (best == 0) {  // the first word is wider than a line: as many characters as fit, at least one
            best = 1;
            while (p[best] && p[best] != ' ' && fits(p, best + 1, font, width, maxPx, o)) ++best;
        }
        put(o, p, best);
        p += best;
        ++n;
    }
    return n;
}

}
