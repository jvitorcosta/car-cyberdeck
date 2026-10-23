#include "termpal.h"
#include <cmath>

namespace termpal {

namespace {
uint32_t scale(uint32_t c, uint32_t pct) {
    const uint32_t r = ((c >> 16) & 0xFF) * pct / 100, g = ((c >> 8) & 0xFF) * pct / 100, b = (c & 0xFF) * pct / 100;
    return (r << 16) | (g << 8) | b;
}

// One sRGB channel (0..255) to linear light, as WCAG 2.x defines it.
double linear(uint32_t c8) {
    const double c = static_cast<double>(c8) / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
}  // namespace

uint32_t rgb(Role r, bool night) {
    static const uint32_t DAY[] = {0x000000, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28};
    static const uint32_t NIGHT[] = {0x000000, 0xFF6600, 0xC84E00, 0xA03A00, 0xFF2A1A};
    return night ? NIGHT[r] : DAY[r];
}

uint32_t scan(bool night) { return night ? 0x050300 : 0x030503; }

uint32_t tile(uint32_t screenRgb, bool night) { return night ? scale(screenRgb, 70) : screenRgb; }

double contrastOnBlack(uint32_t c) {
    const double l = 0.2126 * linear((c >> 16) & 0xFF) + 0.7152 * linear((c >> 8) & 0xFF) + 0.0722 * linear(c & 0xFF);
    return (l + 0.05) / 0.05;
}

}  // namespace termpal
