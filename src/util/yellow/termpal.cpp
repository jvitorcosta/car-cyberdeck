#include "termpal.h"
#include <cmath>

namespace termpal {

namespace {
// One sRGB channel (0..255) to linear light, as WCAG 2.x defines it.
double linear(uint32_t c8) {
    const double c = static_cast<double>(c8) / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
}  // namespace

uint32_t rgb(Role r) {
    static const uint32_t DEFAULT[] = {0x000000, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28};
    return DEFAULT[r];
}

// Per theme (ledlook's swatch order), BG TEXT MID DIM ALERT; the figures in termpal.h.
static const uint32_t THEMES[8][5] = {
    {0, 0xFF8C78, 0xFF4A3A, 0xF03424, 0xFFFFFF},  // red
    {0, 0xFFAA64, 0xF57814, 0xDC6400, 0xFFFFFF},  // orange
    {0, 0xFF8CCD, 0xFF50B4, 0xF0379E, 0xFFFFFF},  // pink
    {0, 0xD2B4FF, 0xB48CFF, 0x9A6EFA, 0xFF3C28},  // purple
    {0, 0xA0C8FF, 0x78A5FF, 0x5A87F5, 0xFF3C28},  // blue
    {0, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28},  // green
    {0, 0xD2D2C8, 0x9C9C94, 0x7C7C74, 0xFF3C28},  // white
    {0, 0xFFDC5A, 0xFF9900, 0xD27E00, 0xFF3C28},  // yellow
};

uint32_t rgb(Role r, int theme) {
    if (theme < 0 || theme >= 8 || r > ALERT) return rgb(r);
    return THEMES[theme][r];
}

uint32_t scan() { return 0x030503; }

double contrastOnBlack(uint32_t c) {
    const double l = 0.2126 * linear((c >> 16) & 0xFF) + 0.7152 * linear((c >> 8) & 0xFF) + 0.0722 * linear(c & 0xFF);
    return (l + 0.05) / 0.05;
}

}  // namespace termpal
