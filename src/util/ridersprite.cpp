#include "ridersprite.h"
#include <cstring>

namespace ridersprite {
namespace {
constexpr size_t HEAD = 8, PAL = 32;
const uint8_t* s_px = nullptr;
uint16_t s_pal[16];
int s_run = 0, s_w = 0, s_h = 0;
}

bool parse(const uint8_t* blob, size_t len) {
    s_px = nullptr;
    s_run = s_w = s_h = 0;
    if (!blob || len < HEAD + PAL || memcmp(blob, "SHR1", 4) != 0) return false;
    const int run = blob[4], w = blob[5], h = blob[6];
    if (run == 0 || w == 0 || h == 0 || w > MAX_SIDE || h > MAX_SIDE) return false;
    const size_t row = (size_t)(w + 1) / 2;
    if (len != HEAD + PAL + (size_t)(run + 1) * row * (size_t)h) return false;
    for (int i = 0; i < 16; ++i) s_pal[i] = (uint16_t)(blob[HEAD + 2 * i] | blob[HEAD + 2 * i + 1] << 8);
    s_px = blob + HEAD + PAL;
    s_run = run;
    s_w = w;
    s_h = h;
    return true;
}

int runFrames() { return s_run; }
int stand() { return s_run; }
int width() { return s_w; }
int height() { return s_h; }

bool pixel(int f, int x, int y, uint16_t* rgb565) {
    if (!s_px || f < 0 || f > s_run || x < 0 || y < 0 || x >= s_w || y >= s_h) return false;
    const size_t row = (size_t)(s_w + 1) / 2;
    const uint8_t b = s_px[((size_t)f * (size_t)s_h + (size_t)y) * row + (size_t)(x / 2)];
    const int i = (x & 1) ? (b & 15) : (b >> 4);
    if (i == 0) return false;
    *rgb565 = s_pal[i];
    return true;
}

int frameAt(uint32_t ms) { return s_run ? (int)((ms / FRAME_MS) % (uint32_t)s_run) : 0; }
}
