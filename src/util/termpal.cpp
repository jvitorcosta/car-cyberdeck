#include "termpal.h"

namespace termpal {

namespace {
uint32_t scale(uint32_t c, uint32_t pct) {
    const uint32_t r = ((c >> 16) & 0xFF) * pct / 100, g = ((c >> 8) & 0xFF) * pct / 100, b = (c & 0xFF) * pct / 100;
    return (r << 16) | (g << 8) | b;
}
}  // namespace

uint32_t rgb(Role r, bool night) {
    static const uint32_t DAY[] = {0x000000, 0x96FFAA, 0x50AA64, 0x235530, 0xFF3C28};
    static const uint32_t NIGHT[] = {0x000000, 0xB37B00, 0x6E4C00, 0x372600, 0xB02A10};
    return night ? NIGHT[r] : DAY[r];
}

uint32_t scan(bool night) { return night ? 0x0A0700 : 0x061008; }

uint32_t tile(uint32_t screenRgb, bool night) { return night ? scale(screenRgb, 70) : screenRgb; }

}  // namespace termpal
