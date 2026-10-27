#include "musiclights.h"
#include "hue.h"
#include "ledlook.h"

namespace musiclights {
namespace {
// Hue in tenths of a degree (0..3599) and saturation as a % of the brightest channel.
constexpr int TURN = 3600;
void hueSat(int r, int g, int b, int* hue10, int* satPct) { hue::hueSat(r, g, b, TURN, 100, hue10, satPct); }

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
        const int d = white ? st : hue::dist(hue, h, TURN);   // the least saturated white
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
