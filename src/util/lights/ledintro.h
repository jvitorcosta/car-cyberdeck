#pragma once
// The lights' welcome on the yellow deck: the first time since boot a car light links, it plays a
// short intro instead of jumping to its look. The inside light (a MELK controller) goes dark, then
// runs its own built-in curtain effect, colour filling its strip from both ends inwards over
// FILL_MS (the deck can't light single LEDs: the controller draws the fill), then takes the look.
// The outside light (ELK-BLEDOM: one colour, no such effect) stays dark meanwhile and rises through
// a peak to its look as the fill ends, so the colour seems to spread from the cabin outwards. Only
// its level moves (elkcmd::dim's perceptual scale), never its hue. Each track ends on the look,
// written twice (a lost write still lands on it; a colour frame also ends the effect). PURE,
// host-tested.
#include <cstdint>
#include "../wire/songpalette.h"

namespace ledintro {
// The curtain effect and its speed. The MELK notes give 57..76 as the curtains without naming each
// one, so these are a first pick, to be tuned in the car (docs/yellow-deck.md, Welcome).
constexpr uint8_t FILL_MODE = 58;
constexpr uint8_t FILL_SPEED = 50;       // 0..100
constexpr uint32_t FILL_MS = 4000;       // the look goes this long after the effect starts
constexpr int PEAK_LIFT = 30;            // the outside peak: the look's level + this many points (at most 100 %)
constexpr uint32_t IN_GAP_MS = 300;      // the inside controller's steps (it takes ~7 writes a second)
constexpr uint32_t OUT_GAP_MS = 1000;    // the outside controller's (it takes ~1)
// The outside light's first rise: its peak lands as the fill ends.
constexpr uint32_t OUT_DELAY_MS = IN_GAP_MS + FILL_MS - OUT_GAP_MS;
constexpr int MAX_KEYS = 12;

struct Key {
    uint32_t at;          // ms after the track starts
    songpalette::Rgb c;   // the colour frame (fill: the colour the strip is filling with)
    bool fill = false;    // start the curtain effect (FILL_SPEED, FILL_MODE) instead of sending c
};
struct Track {
    int n = 0;
    Key k[MAX_KEYS];
};

// The level (0..100 %) the outside light peaks at for a look at `pct`.
int peakPct(int pct);
// The track for the inside (true) or outside light, whose look is `swatch` (its LED mix at 100 %)
// at `pct`. The first key (at 0, black) goes with the power-on; the last two are the look.
Track plan(bool inside, songpalette::Rgb swatch, int pct);
// When the track's last key goes (0 for an empty one).
uint32_t length(const Track& t);
}  // namespace ledintro
