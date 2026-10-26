#include "musiclights.h"
#include "ledlook.h"

namespace musiclights {
namespace {
// Hue in tenths of a degree (0..3599) and saturation as a % of the brightest channel.
void hueSat(int r, int g, int b, int* hue10, int* satPct) {
    int mx = r, mn = r;
    if (g > mx) mx = g;
    if (b > mx) mx = b;
    if (g < mn) mn = g;
    if (b < mn) mn = b;
    const int d = mx - mn;
    *satPct = mx ? d * 100 / mx : 0;
    if (!d) {
        *hue10 = 0;
        return;
    }
    int h;
    if (mx == r) h = 600 * (g - b) / d;
    else if (mx == g) h = 1200 + 600 * (b - r) / d;
    else h = 2400 + 600 * (r - g) / d;
    *hue10 = (h + 3600) % 3600;
}

int hueDist10(int a, int b) {  // 0..1800
    const int d = ((a - b) % 3600 + 3600) % 3600;
    return d > 1800 ? 3600 - d : d;
}

uint8_t mix(uint8_t from, uint8_t to, int k, int n) {
    const int d = (to - from) * k;
    const int step = d >= 0 ? (d + n / 2) / n : -((-d + n / 2) / n);
    return static_cast<uint8_t>(from + step);
}
}  // namespace

int nearestSwatch(songpalette::Rgb c) {
    int hue, sat;
    hueSat(c.r, c.g, c.b, &hue, &sat);
    const bool white = sat < WHITE_SAT_PCT;
    int best = -1, bestD = 0;
    for (int i = 0; i < ledlook::N_GRID; i++) {
        const uint32_t s = ledlook::SWATCHES[i].screen;
        int h, st;
        hueSat(static_cast<int>(s >> 16), static_cast<int>((s >> 8) & 0xFF), static_cast<int>(s & 0xFF), &h, &st);
        if ((st < WHITE_SAT_PCT) != white) continue;  // whites against whites, colours against colours
        const int d = white ? st : hueDist10(hue, h);   // the least saturated white
        if (best < 0 || d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best < 0 ? 0 : best;
}

int level(int ownerPct, int capPct) {
    if (ownerPct <= FLOOR_PCT) return ownerPct;
    const int cap = capPct > 100 ? 100 : capPct < 0 ? 0 : capPct;
    const int v = (ownerPct * cap + 50) / 100;
    return v < FLOOR_PCT ? FLOOR_PCT : v > ownerPct ? ownerPct : v;
}

songpalette::Rgb blend(songpalette::Rgb from, songpalette::Rgb to, int k, int n) {
    if (n <= 0 || k >= n) return to;
    if (k <= 0) return from;
    return {mix(from.r, to.r, k, n), mix(from.g, to.g, k, n), mix(from.b, to.b, k, n)};
}

State state(bool heard, uint32_t lastAt, uint32_t now) {
    if (now - lastAt >= SIGNAL_MS) return State::NoSignal;
    return heard ? State::Colour : State::Waiting;
}
}  // namespace musiclights
