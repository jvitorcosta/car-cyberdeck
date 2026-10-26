#include "lyricrun.h"

#include "../util/text.h"

namespace lyricrun {

bool isCjk(uint32_t cp) {
    static constexpr uint32_t RANGES[][2] = {
        {0x1100, 0x11FF},    // Hangul jamo
        {0x2E80, 0x2FDF},    // CJK and Kangxi radicals
        {0x3000, 0x4DBF},    // CJK punctuation, kana, bopomofo, Hangul compatibility jamo, enclosed, extension A
        {0x4E00, 0x9FFF},    // CJK unified ideographs
        {0xA960, 0xA97F},    // Hangul jamo extended-A
        {0xAC00, 0xD7FF},    // Hangul syllables, jamo extended-B
        {0xF900, 0xFAFF},    // CJK compatibility ideographs
        {0xFE30, 0xFE4F},    // CJK compatibility forms
        {0xFF00, 0xFFEF},    // halfwidth and fullwidth forms
        {0x20000, 0x3FFFF},  // ideographs, extension B on
    };
    for (const auto& r : RANGES)
        if (cp >= r[0] && cp <= r[1]) return true;
    return false;
}

size_t decode(const char* s, Item* out, size_t cap, GlyphWidth gw, const void* ctx, bool upper) {
    size_t n = 0;
    if (!s) return 0;
    for (size_t i = 0; s[i] && n < cap;) {
        // One character, parsed as txt::latin1Fold does: a lead byte and its continuation bytes;
        // anything malformed skips just the bytes it has.
        const size_t at = i;
        const unsigned char c = (unsigned char)s[i];
        const size_t more = c < 0x80 ? 0 : c >= 0xF5 ? 0 : c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : 0;
        uint32_t cp = c < 0x80 ? c : more == 3 ? (c & 0x07u) : more == 2 ? (c & 0x0Fu) : (c & 0x1Fu);
        size_t k = 1;
        for (; k <= more && (((unsigned char)s[i + k]) & 0xC0) == 0x80; ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3Fu);
        i += k;
        if (c >= 0x80 && (more == 0 || k <= more)) continue;  // malformed: dropped

        // what latin1Fold makes of it, if anything
        char one[5], folded[8];
        for (size_t j = 0; j < k; ++j) one[j] = s[at + j];
        one[k] = 0;
        const size_t m = txt::latin1Fold(one, folded, sizeof(folded));
        if (m > 0) {
            if (n + m > cap) break;  // whole or not at all
            if (upper) txt::latin1Upper(folded);
            for (size_t j = 0; j < m; ++j) out[n++] = {(unsigned char)folded[j], Kind::Latin1, 0};
            continue;
        }
        const int w = gw && cp <= 0xFFFF ? gw(cp, ctx) : 0;
        if (w > 0) out[n++] = {static_cast<uint16_t>(cp), Kind::Wide, static_cast<uint8_t>(w)};
        else if (isCjk(cp)) out[n++] = {0, Kind::Box, BOX_W};
    }
    return n;
}

int scale(termfont::Face f) { return f == termfont::Face::BIG ? 2 : 1; }

namespace {
// One Latin-1 character's advance, or its ink's right edge (termfont::width of it alone).
int latin(termfont::Face f, uint32_t c, bool ink) {
    const char s[2] = {static_cast<char>(c), 0};
    return ink ? termfont::width(f, s) : termfont::advance(f, s);
}
}  // namespace

int advance(termfont::Face f, const Item& it) {
    return it.kind == Kind::Latin1 ? latin(f, it.cp, false) : it.w * scale(f);
}

int width(termfont::Face f, const Item* it, size_t n, bool dots) {
    int w = 0;
    for (size_t i = 0; i < n; ++i) w += advance(f, it[i]);
    if (dots) return w + termfont::width(f, "..");
    if (n > 0 && it[n - 1].kind == Kind::Latin1) w += latin(f, it[n - 1].cp, true) - latin(f, it[n - 1].cp, false);
    return w;
}

int glyphTop(termfont::Face f, int baseline) { return baseline - (termfont::capHeight(f) + GLYPH_H * scale(f)) / 2; }

namespace {

bool isSpace(const Item& x) { return x.kind == Kind::Latin1 && x.cp == ' '; }
bool isWide(const Item& x) { return x.kind != Kind::Latin1; }

bool fits(const Item* it, size_t from, size_t to, termfont::Face f, int maxPx) { return width(f, it + from, to - from) <= maxPx; }

// The rest of the text from p on one line, cut with ".." when it doesn't fit (a space before the
// ".." goes). True when cut.
bool fitLast(const Item* it, size_t p, size_t n, termfont::Face f, int maxPx, Span& out) {
    size_t end = n;
    while (end > p && isSpace(it[end - 1])) --end;
    out = {static_cast<uint16_t>(p), static_cast<uint16_t>(end), false};
    if (fits(it, p, end, f, maxPx)) return false;
    for (size_t keep = end - 1;; --keep) {
        while (keep > p && isSpace(it[keep - 1])) --keep;
        out = {static_cast<uint16_t>(p), static_cast<uint16_t>(keep), true};
        if (keep == p || width(f, it + p, keep - p, true) <= maxPx) return true;  // ".." even when it doesn't fit
    }
}

}  // namespace

int wrap(const Item* it, size_t n, termfont::Face f, int maxPx, int maxLines, Span out[], bool* cut) {
    if (cut) *cut = false;
    if (!it || maxLines <= 0) return 0;
    if (n > 0xFFFF) n = 0xFFFF;
    int lines = 0;
    size_t p = 0;
    while (lines < maxLines) {
        while (p < n && isSpace(it[p])) ++p;
        if (p == n) break;
        if (lines == maxLines - 1) {
            const bool c = fitLast(it, p, n, f, maxPx, out[lines]);
            if (cut) *cut = c;
            return lines + 1;
        }
        size_t best = p, e = p;  // it[p..best): the whole words that fit so far
        for (;;) {
            while (e < n && isSpace(it[e])) ++e;
            if (e == n) break;
            if (isWide(it[e])) ++e;  // a word of its own
            else
                while (e < n && !isSpace(it[e]) && !isWide(it[e])) ++e;
            if (!fits(it, p, e, f, maxPx)) break;
            best = e;
        }
        if (best == p) {  // the first word is wider than a line: as many characters as fit, at least one
            best = p + 1;
            while (best < n && !isSpace(it[best]) && !isWide(it[best]) && fits(it, p, best + 1, f, maxPx)) ++best;
        }
        out[lines++] = {static_cast<uint16_t>(p), static_cast<uint16_t>(best), false};
        p = best;
    }
    return lines;
}

bool hasWide(const Item* it, const Span& s) {
    for (size_t i = s.start; i < s.end; ++i)
        if (isWide(it[i])) return true;
    return false;
}

}  // namespace lyricrun
