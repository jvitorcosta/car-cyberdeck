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

// The quiet tone: the theme's DIM, or DIM at QUIET_PCT where it is too close to TEXT for the sweep to
// show (night yellow is one colour at all three levels).
static uint32_t quiet(uint32_t text, uint32_t dim) {
    if (termpal::contrastOnBlack(text) >= 1.5 * termpal::contrastOnBlack(dim)) return dim;
    return tint(dim, QUIET_PCT, 0x000000);
}

Pal of(int swatch, bool night) {
    if (swatch == CYAN) swatch = BLUE;
    if (swatch < 0 || swatch >= CYAN) swatch = -1;   // termpal's default, the yellow deck's green
    const uint32_t t = termpal::rgb(termpal::TEXT, swatch, night);
    const uint32_t m = termpal::rgb(termpal::MID, swatch, night);
    const uint32_t d = quiet(t, termpal::rgb(termpal::DIM, swatch, night));
    Pal p;
    p.bg = termpal::rgb(termpal::BG, swatch, night);
    p.song = m;
    p.artist = d;
    p.barOn = t;
    p.barOff = d;
    p.lit = t;
    p.unsung = d;
    p.prev = d;
    p.next = d;
    return p;
}

}  // namespace karaokepal
