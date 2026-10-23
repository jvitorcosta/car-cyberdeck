#include "termpal.h"

namespace termpal {

namespace {
uint32_t scale(uint32_t c, uint32_t pct) {
    const uint32_t r = ((c >> 16) & 0xFF) * pct / 100, g = ((c >> 8) & 0xFF) * pct / 100, b = (c & 0xFF) * pct / 100;
    return (r << 16) | (g << 8) | b;
}
}  // namespace

uint32_t rgb(Role r, bool night) {
    static const uint32_t DAY[] = {0x000000, 0x3CFF7A, 0x1F9E48, 0x0F5024};
    static const uint32_t NIGHT[] = {0x000000, 0xB37B00, 0x6E4C00, 0x372600};
    return night ? NIGHT[r] : DAY[r];
}

uint32_t scan(bool night) { return night ? 0x0A0700 : 0x031008; }

uint32_t tile(uint32_t screenRgb, bool night) { return night ? scale(screenRgb, 70) : screenRgb; }

}  // namespace termpal
