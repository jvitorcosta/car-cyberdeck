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
static const uint32_t THEMES[9][5] = {
    {0, 0xFF8C78, 0xFF4A3A, 0xF03424, 0xFFFFFF},  // red
    {0, 0xFFAA64, 0xF57814, 0xDC6400, 0xFFFFFF},  // orange
    {0, 0xFF8CCD, 0xFF50B4, 0xF0379E, 0xFFFFFF},  // pink
    {0, 0xD2B4FF, 0xB48CFF, 0x9A6EFA, 0xFF3C28},  // purple
    {0, 0xA0C8FF, 0x78A5FF, 0x5A87F5, 0xFF3C28},  // blue
    {0, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28},  // green
    {0, 0xD2D2C8, 0x9C9C94, 0x7C7C74, 0xFF3C28},  // white
    {0, 0xFFDC5A, 0xFF9900, 0xD27E00, 0xFF3C28},  // yellow
    {0, 0x8CF0FF, 0x32D2F0, 0x00AACC, 0xFF3C28},  // cyan
};

uint32_t rgb(Role r, int theme) {
    if (theme < 0 || theme >= 9 || r > ALERT) return rgb(r);
    return THEMES[theme][r];
}

// Night, per theme as above, then the default (the green theme's); the figures in termpal.h.
static const uint32_t NIGHT[10][5] = {
    {0, 0xFF0000, 0xE00000, 0xBC0000, 0xFFFFFF},  // red
    {0, 0xFF4600, 0xE03400, 0xBC2800, 0xFFFFFF},  // orange
    {0, 0xFF0040, 0xE00038, 0xBC0030, 0xFFFFFF},  // pink
    {0, 0xD200FF, 0xB800E0, 0x9A00BC, 0xFF0000},  // purple
    {0, 0x0046FF, 0x003CE0, 0x0032BC, 0xFF0000},  // blue
    {0, 0x50FF46, 0x46E03C, 0x3ABC32, 0xFF0000},  // green
    {0, 0xFFFFFF, 0xEBEBEB, 0xD2D2D2, 0xFF0000},  // white
    {0, 0xFFB400, 0xFFB400, 0xFFB400, 0xFF0000},  // yellow
    {0, 0x00E6FF, 0x00C8E0, 0x00A5BC, 0xFF0000},  // cyan
    {0, 0x50FF46, 0x46E03C, 0x3ABC32, 0xFF0000},  // default (green)
};

uint32_t rgb(Role r, int theme, bool night) {
    if (!night) return rgb(r, theme);
    if (r > ALERT) r = BG;
    return NIGHT[theme < 0 || theme >= 9 ? 9 : theme][r];
}

uint32_t scan() { return 0x030503; }

double contrastOnBlack(uint32_t c) {
    const double l = 0.2126 * linear((c >> 16) & 0xFF) + 0.7152 * linear((c >> 8) & 0xFF) + 0.0722 * linear(c & 0xFF);
    return (l + 0.05) / 0.05;
}

}  // namespace termpal
