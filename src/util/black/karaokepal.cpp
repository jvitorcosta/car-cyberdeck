#include "karaokepal.h"
#include "../common/termpal.h"

namespace karaokepal {

static constexpr int BLUE = 4, CYAN = 8;

uint32_t tint(uint32_t c, int pct, uint32_t base) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const int a = static_cast<int>((c >> shift) & 0xFF), b = static_cast<int>((base >> shift) & 0xFF);
        out |= static_cast<uint32_t>((a * pct + b * (100 - pct) + 50) / 100) << shift;
    }
    return out;
}

Pal of(int swatch, bool night) {
    if (swatch == CYAN) swatch = BLUE;
    if (swatch < 0 || swatch > CYAN) swatch = DEFAULT_SWATCH;
    const uint32_t t = termpal::rgb(termpal::TEXT, swatch, night);
    const uint32_t m = termpal::rgb(termpal::MID, swatch, night);
    const uint32_t d = termpal::rgb(termpal::DIM, swatch, night);
    const uint32_t grey = night ? 0x5A6068 : 0x8A929C;
    Pal p;
    p.bg = night ? tint(d, 7, 0x030406) : tint(d, 10, 0x0E1218);
    p.song = t;
    p.artist = m;
    p.barOn = t;
    p.barOff = tint(d, 30, p.bg);
    p.lit = night ? 0xEDE6D6 : 0xFFFFFF;
    p.unsung = tint(d, 45, grey);
    p.prev = tint(d, 55, grey);
    p.next = tint(m, 70, grey);
    return p;
}

}  // namespace karaokepal
