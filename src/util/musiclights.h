#pragma once
// Music mode on the yellow deck: both car lights take the current song's main colour. The black deck
// sends the song's palette (util/songpalette, over util/musicwire); its main colour c[0] is snapped
// to one of ledlook's 8 swatches (so the lights get that swatch's tuned LED mix), each light's level
// is the owner's under the palette's time-of-day cap with a floor, a new song fades in, and the
// prompt line says what is going on. PURE, host-tested.
#include <cstdint>
#include "songpalette.h"

namespace musiclights {
constexpr int FLOOR_PCT = 40;          // the cap never takes a light below this (unless the owner's is lower)
constexpr int WHITE_SAT_PCT = 50;      // a colour less saturated than this is white
constexpr uint32_t SIGNAL_MS = 15000;  // no palette for this long: NO SIGNAL

// The ledlook swatch whose screen colour is nearest: by hue (circular) among the saturated swatches,
// or the white one for a colour under WHITE_SAT_PCT saturation (black included).
int nearestSwatch(songpalette::Rgb c);
// A light's level in music mode: ownerPct x capPct / 100 (rounded), never under FLOOR_PCT, never over
// ownerPct (an owner's level at or under the floor is kept as it is). capPct over 100 counts as 100.
int level(int ownerPct, int capPct);
// Step k of an n-step fade from `from` to `to` (each channel rounded): k <= 0 is from, k >= n (or
// n <= 0) is to.
songpalette::Rgb blend(songpalette::Rgb from, songpalette::Rgb to, int k, int n);

// The prompt line: Waiting (no palette yet, ">SONG COLOURS" alone), Colour (">SONG COLOURS" and the
// swatch) or NoSignal (">NO SIGNAL"). heard: a palette came this run, the latest at lastAt; until one
// has, lastAt is when the mode went on. NoSignal from SIGNAL_MS after lastAt.
enum class State : uint8_t { Waiting, Colour, NoSignal };
State state(bool heard, uint32_t lastAt, uint32_t now);
}  // namespace musiclights
