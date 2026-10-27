#include "backdrop.h"
#include <cstring>

namespace backdrop {
namespace {
constexpr size_t HEAD = 8;
constexpr int MAX_W = 320, MAX_H = 240;   // the screen
const uint8_t* s_px = nullptr;
int s_w = 0, s_h = 0;
}

bool parse(const uint8_t* blob, size_t len) {
    s_px = nullptr;
    s_w = s_h = 0;
    if (!blob || len < HEAD || memcmp(blob, "BKD1", 4) != 0) return false;
    const int w = blob[4] | blob[5] << 8, h = blob[6] | blob[7] << 8;
    if (w <= 0 || h <= 0 || w > MAX_W || h > MAX_H) return false;
    if (len != HEAD + (size_t)w * (size_t)h * 2) return false;
    s_px = blob + HEAD;
    s_w = w;
    s_h = h;
    return true;
}

int width() { return s_w; }
int height() { return s_h; }

const uint8_t* row(int y) {
    if (!s_px || y < 0 || y >= s_h) return nullptr;
    return s_px + (size_t)y * (size_t)s_w * 2;
}
}
