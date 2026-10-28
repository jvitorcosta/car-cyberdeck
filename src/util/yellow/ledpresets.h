#pragma once
// The panel's six fixed presets (the owner's picks): each a pair of looks (inside light, outside
// light), one colour at 40 % inside and 100 % outside, with a name for the screen and a lowercase
// word for the wire. Nothing is saved, so there is no storage to validate. PURE, host-tested.
#include <cstddef>

#include "../lights/ledlook.h"

namespace ledpresets {
constexpr int N = 6;
// The byte that stands for ゴジラ in a name the deck's screen shows (VT323 has no katakana; ui/gojiraname
// draws the three glyphs in its place). Never on the wire, in logs or in voice replies.
constexpr char GOJIRA_MARK = '\x01';
struct Preset {
    const char* name;  // "ROSE": the wire, logs, voice replies
    const char* word;  // "rose"
    ledlook::Look in, out;
    const char* glyphs = nullptr;  // what the screen shows instead of name, or nullptr: GOJIRA's ゴジラ
};
// Swatch indices come from ledlook: red 0, pink 2, purple 3, blue 4, green 5, light blue 8.
extern const Preset PRESETS[N];
// The name as the deck's screen shows it: "ROSE", or GOJIRA's mark (see GOJIRA_MARK).
inline const char* shown(const Preset& p) { return p.glyphs ? p.glyphs : p.name; }
// slot is 1..N. "Rose: inside Pink 40 percent, outside Pink 100 percent" (name in title case)
void describe(int slot, char* out, size_t n);
// one /presets line: "1 ROSE pink 40 pink 100"
void wireLine(int slot, char* out, size_t n);
}  // namespace ledpresets
