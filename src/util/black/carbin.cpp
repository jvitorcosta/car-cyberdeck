#include "carbin.h"
#include <cstring>

namespace carbin {

static constexpr size_t HEAD = 20;

static int u16(const uint8_t* p) { return p[0] | p[1] << 8; }

bool parse(const uint8_t* blob, size_t len, Car* out) {
    if (!blob || len < HEAD || memcmp(blob, "CAR1", 4) != 0) return false;
    Car c;
    c.w = u16(blob + 4); c.h = u16(blob + 6);
    c.rearX = u16(blob + 8); c.frontX = u16(blob + 10); c.wheelY = u16(blob + 12);
    c.headX = u16(blob + 14); c.headY = u16(blob + 16); c.tailY = u16(blob + 18);
    if (c.w <= 0 || c.h <= 0 || c.w > 320 || c.h > 240) return false;
    const size_t n = (size_t)c.w * c.h, kinds = (n + 1) / 2;
    if (len != HEAD + kinds + 2 * n) return false;
    c.kinds = blob + HEAD;
    c.px = blob + HEAD + kinds;
    *out = c;
    return true;
}

bool pixel(const Car& c, int x, int y, uint16_t* rgb565, uint8_t* kind) {
    uint8_t k = 0;
    if (x >= 0 && y >= 0 && x < c.w && y < c.h) {
        const int i = y * c.w + x;
        k = (i & 1) ? (c.kinds[i >> 1] & 0x0F) : (c.kinds[i >> 1] >> 4);
        if (k && rgb565) *rgb565 = (uint16_t)(c.px[2 * i] | c.px[2 * i + 1] << 8);
    }
    if (kind) *kind = k;
    return k != 0;
}

}
