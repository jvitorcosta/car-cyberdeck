#pragma once
// The panel's four fixed presets (the owner's picks): each a pair of looks (inside light, outside
// light), with a name for the screen and a lowercase word for the wire. Nothing is saved, so there is
// no storage to validate. PURE, host-tested.
#include <cstddef>

#include "ledlook.h"

namespace ledpresets {
constexpr int N = 4;
struct Preset {
    const char* name;  // "ROSE"
    const char* word;  // "rose"
    ledlook::Look in, out;
};
// Swatch indices come from ledlook: red 0, pink 2, purple 3, white 6.
extern const Preset PRESETS[N];
// slot is 1..4. "Rose: inside Pink 40 percent, outside Pink 100 percent" (name in title case)
void describe(int slot, char* out, size_t n);
// one /presets line: "1 ROSE pink 40 pink 100"
void wireLine(int slot, char* out, size_t n);
}  // namespace ledpresets
