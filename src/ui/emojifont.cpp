#include "emojifont.h"
#include <cstring>

namespace emojifont {

static constexpr size_t HEAD = 6;

uint32_t Font::key(int i) const {
    const uint8_t* p = blob_ + HEAD + 4 * i;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool Font::open(const uint8_t* blob, size_t len) {
    blob_ = nullptr; n_ = 0;
    if (!blob || len < HEAD || memcmp(blob, "EMJ1", 4) != 0) return false;
    int n = blob[4] | (blob[5] << 8);
    // count <= 65535, so this fits in 32 bits on the ESP32
    if (len < HEAD + (size_t)n * (4 + GLYPH_BYTES)) return false;
    blob_ = blob; n_ = n;
    for (int i = 1; i < n; ++i)
        if (key(i) <= key(i - 1)) { blob_ = nullptr; n_ = 0; return false; }
    return true;
}

const uint8_t* Font::glyph(uint32_t k) const {
    int lo = 0, hi = n_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t m = key(mid);
        if (k < m) hi = mid - 1;
        else if (k > m) lo = mid + 1;
        else return blob_ + HEAD + 4 * n_ + GLYPH_BYTES * mid;
    }
    return nullptr;
}

uint16_t paletteAt(const uint8_t* g, int i) {
    return (uint16_t)(g[2 * i] | (g[2 * i + 1] << 8));
}

int pixelAt(const uint8_t* g, int x, int y) {
    uint8_t b = g[32 + y * (SIZE / 2) + x / 2];
    return (x & 1) ? (b & 0x0F) : (b >> 4);
}

}
