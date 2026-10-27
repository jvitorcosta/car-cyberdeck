#include "emojifont.h"
#include <cstring>

namespace emojifont {

static constexpr size_t SEQ_BYTES = 4 * SEQ_MAX;

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t Font::key(int i) const { return rd32(blob_ + 4 * i); }

int Font::compareSeq(int i, const uint32_t* cps, int n) const {
    const uint8_t* s = seqs_ + SEQ_BYTES * i;
    for (int k = 0; k < SEQ_MAX; ++k) {
        uint32_t a = rd32(s + 4 * k), b = k < n ? cps[k] : 0;
        if (a != b) return a < b ? -1 : 1;
    }
    return 0;
}

bool Font::open(const uint8_t* blob, size_t len) {
    blob_ = seqs_ = glyphs_ = nullptr; n_ = nseq_ = 0;
    if (!blob || len < 6) return false;
    const bool v2 = memcmp(blob, "EMJ2", 4) == 0;
    if (!v2 && memcmp(blob, "EMJ1", 4) != 0) return false;
    const size_t head = v2 ? 8 : 6;
    if (len < head) return false;
    const int n = blob[4] | (blob[5] << 8), nseq = v2 ? blob[6] | (blob[7] << 8) : 0;
    // counts <= 65535, so this fits in 32 bits on the ESP32
    if (len < head + (size_t)n * (4 + GLYPH_BYTES) + (size_t)nseq * SEQ_BYTES) return false;
    blob_ = blob + head; n_ = n;
    seqs_ = blob_ + 4 * n; nseq_ = nseq;
    glyphs_ = seqs_ + SEQ_BYTES * nseq;
    bool ok = true;
    for (int i = 1; i < n && ok; ++i) ok = key(i) > key(i - 1);
    for (int i = 1; i < nseq && ok; ++i) {
        uint32_t prev[SEQ_MAX];
        for (int k = 0; k < SEQ_MAX; ++k) prev[k] = rd32(seqs_ + SEQ_BYTES * (i - 1) + 4 * k);
        ok = compareSeq(i, prev, SEQ_MAX) > 0;
    }
    if (!ok) { blob_ = seqs_ = glyphs_ = nullptr; n_ = nseq_ = 0; }
    return ok;
}

const uint8_t* Font::glyph(uint32_t k) const {
    int lo = 0, hi = n_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t m = key(mid);
        if (k < m) hi = mid - 1;
        else if (k > m) lo = mid + 1;
        else return glyphs_ + GLYPH_BYTES * mid;
    }
    return nullptr;
}

uint32_t Font::sequence(const uint32_t* cps, int n) const {
    if (n < 1 || n > SEQ_MAX) return 0;
    int lo = 0, hi = nseq_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = compareSeq(mid, cps, n);
        if (c > 0) hi = mid - 1;
        else if (c < 0) lo = mid + 1;
        else return SEQ_BASE + (uint32_t)mid;
    }
    return 0;
}

uint16_t paletteAt(const uint8_t* g, int i) {
    return (uint16_t)(g[2 * i] | (g[2 * i + 1] << 8));
}

int pixelAt(const uint8_t* g, int x, int y) {
    uint8_t b = g[32 + y * (SIZE / 2) + x / 2];
    return (x & 1) ? (b & 0x0F) : (b >> 4);
}

}
