#include "jpfont.h"
#include <cstring>

namespace jpfont {

namespace {
constexpr size_t HEADER = 6, GLYPH = 32;

uint16_t cpAt(const uint8_t* blob, int i) { return (uint16_t)(blob[HEADER + 2 * i] | (blob[HEADER + 2 * i + 1] << 8)); }
// The width table and the bitmaps follow the codepoints.
const uint8_t* widths(const uint8_t* blob, int n) { return blob + HEADER + 2 * n; }
const uint8_t* bitmaps(const uint8_t* blob, int n) { return blob + HEADER + 3 * n; }
}  // namespace

bool Font::open(const uint8_t* blob, size_t len) {
    blob_ = nullptr;
    n_ = 0;
    if (!blob || len < HEADER || memcmp(blob, "JFNT", 4) != 0) return false;
    const int n = blob[4] | (blob[5] << 8);
    if (len != HEADER + (size_t)n * (3 + GLYPH)) return false;
    for (int i = 0; i < n; ++i) {
        const uint8_t w = widths(blob, n)[i];
        if ((w != 8 && w != 16) || (i > 0 && cpAt(blob, i) <= cpAt(blob, i - 1))) return false;
    }
    blob_ = blob;
    n_ = n;
    return true;
}

int Font::find(uint32_t cp) const {
    int lo = 0, hi = n_ - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const uint32_t c = cpAt(blob_, mid);
        if (cp < c) hi = mid - 1;
        else if (cp > c) lo = mid + 1;
        else return mid;
    }
    return -1;
}

const uint8_t* Font::glyph(uint32_t cp, int* width) const {
    const int i = find(cp);
    if (i < 0) return nullptr;
    if (width) *width = widths(blob_, n_)[i];
    return bitmaps(blob_, n_) + (size_t)i * GLYPH;
}

int Font::width(uint32_t cp) const {
    const int i = find(cp);
    return i < 0 ? 0 : widths(blob_, n_)[i];
}

}  // namespace jpfont
