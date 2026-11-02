#pragma once
// The lights' welcome on the yellow deck: the first time since boot a car light links, it plays a
// short "wave" instead of jumping to its look. The controllers (ELK-BLEDOM, MELK) drive one colour
// for the whole strip, so the motion runs from light to light: the inside light goes dark, swells
// past its look to a peak and settles on it; the outside light, dark meanwhile, follows a beat later
// (its controller takes ~1 write a second). Every step is the look's own swatch, only its level
// moves (elkcmd::dim's perceptual scale), so the hue never shifts. Each track ends on the look,
// written twice (a lost write still lands on it). PURE, host-tested.
#include <cstdint>
#include "../wire/songpalette.h"

namespace ledintro {
constexpr int PEAK_LIFT = 30;            // the peak: the look's level + this many points (at most 100 %)
constexpr uint32_t IN_GAP_MS = 300;      // the inside controller's steps (it takes ~7 writes a second)
constexpr uint32_t OUT_GAP_MS = 1000;    // the outside controller's (it takes ~1)
constexpr uint32_t OUT_DELAY_MS = 1500;  // the outside light's first rise: as the inside one peaks
constexpr int MAX_KEYS = 12;

struct Key {
    uint32_t at;  // ms after the track starts
    songpalette::Rgb c;
};
struct Track {
    int n = 0;
    Key k[MAX_KEYS];
};

// The level (0..100 %) the wave peaks at for a look at `pct`.
int peakPct(int pct);
// The track for the inside (true) or outside light, whose look is `swatch` (its LED mix at 100 %)
// at `pct`. The first key (at 0, black) goes with the power-on; the last two are the look.
Track plan(bool inside, songpalette::Rgb swatch, int pct);
// When the track's last key goes (0 for an empty one).
uint32_t length(const Track& t);
}  // namespace ledintro
