#include "idlesprite.h"
#include <cstring>

namespace idlesprite {
namespace {
constexpr size_t HEAD = 8, PAL = 32;
const uint8_t* s_px = nullptr;   // the frames' 4-bit pixels
uint16_t s_pal[16];
int s_w = 0, s_h = 0, s_n = 0;
}

bool parse(const uint8_t* blob, size_t len) {
    s_n = 0;
    if (!blob || len < HEAD + PAL || memcmp(blob, "IDL1", 4) != 0) return false;
    const int n = blob[4], w = blob[5], h = blob[6];
    if (n != FRAMES || w == 0 || h == 0 || (w & 1)) return false;
    if (len != HEAD + PAL + (size_t)n * (size_t)(w / 2) * (size_t)h) return false;
    for (int i = 0; i < 16; ++i) s_pal[i] = (uint16_t)(blob[HEAD + 2 * i] | blob[HEAD + 2 * i + 1] << 8);
    s_px = blob + HEAD + PAL;
    s_w = w;
    s_h = h;
    s_n = n;
    return true;
}

int count() { return s_n; }
int width() { return s_n ? s_w : 0; }
int height() { return s_n ? s_h : 0; }

bool pixel(int f, int x, int y, uint16_t* rgb565) {
    if (f < 0 || f >= s_n || x < 0 || y < 0 || x >= s_w || y >= s_h) return false;
    const uint8_t b = s_px[(size_t)f * (size_t)(s_w / 2) * (size_t)s_h + (size_t)y * (size_t)(s_w / 2) + (size_t)(x / 2)];
    const int i = (x & 1) ? (b & 15) : (b >> 4);
    if (i == 0) return false;
    *rgb565 = s_pal[i];
    return true;
}

uint32_t tickAt(uint32_t ms) { return ms / FRAME_MS; }

int frameAt(uint32_t ms) {
    const uint32_t t = tickAt(ms);
    if (t < 12) return STAND;
    if (t == 12) return WAIT2;
    if (t < 16) return WAIT1;
    return ((t - 16) & 1) ? WAIT3 : WAIT2;
}
}
