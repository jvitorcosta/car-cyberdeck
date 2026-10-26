// Screen layouts and navigation of the yellow deck's terminal-style app: a Menu screen (the
// dashboard), a Lights screen (with its PRESETS tab, laid out as a screen of its own) and a Karaoke
// screen, each a list of rectangles on the 320x240 display. Hit-testing and D-pad neighbour search
// live here as generic functions over a rect list. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace deckmenu {

struct Rect { int16_t x, y, w, h; };
// Presets is the Lights screen on its PRESETS tab: the same < BACK and tabs (the same ids), then the
// presets in place of the colours and the level row.
enum class Screen : uint8_t { Menu, Lights, Karaoke, Presets };

// Menu (the dashboard): three items side by side, finger-sized, at the bottom under the prompt line;
// everything else on it is display only (the bands below).
enum MenuId : int { M_ITEM0 = 0, M_N = 3 };
enum Item : uint8_t { LIGHTS, KARAOKE, MUSIC };  // later: CAR, HEART
constexpr int N_ITEMS = 3;                     // items shown, in slot order

// Lights' tabs, left to right: the three look pages, then PRESETS.
enum Tab : int { TAB_BOTH, TAB_INSIDE, TAB_OUTSIDE, TAB_PRESETS, N_TABS };

// Lights: Back, 4 tabs, 8 tiles, -, +
enum LightsId : int { L_BACK = 0, L_TAB0 = 1, L_TILE0 = L_TAB0 + N_TABS, L_MINUS = L_TILE0 + 8, L_PLUS, L_N };

// Presets (the PRESETS tab): Back and the tabs as on Lights, then the six presets, two rows of three.
enum PresetsId : int { P_BACK = L_BACK, P_TAB0 = L_TAB0, P_PRE0 = L_TAB0 + N_TABS, P_N = P_PRE0 + 6 };

// Karaoke: Back (where Lights has it); the rest is display only.
enum KaraokeId : int { K_BACK = 0, K_N = 1 };

// {0,0,0,0} for an item slot that is not built (or an id out of range).
Rect rect(Screen s, int id);

// Display only: never a hit, never a neighbour.
// The Menu's bands, top to bottom, each from the rule above it: the header line, the clock and the
// weather (its last row HI / LO), the trip, the two lights on one row, the prompt line (the
// messages, music mode's line), then (the items). A tap in a band doesn't snap to an item; the 4
// rows between the prompt line and the items, and those under them, do.
constexpr Rect HEADER{0, 0, 320, 22}, CLOCK_BAND{0, 22, 320, 72}, TRIP_BAND{0, 94, 320, 41};      // menu
constexpr Rect LIGHTS_BAND{0, 135, 320, 31}, PROMPT_BAND{0, 166, 320, 22};                        // menu
constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};  // lights (STATUS: presets too)
constexpr Rect KARAOKE_BODY{0, 44, 320, 196};                                                    // karaoke

// Number of ids on a screen (M_N / L_N / K_N / P_N).
int count(Screen s);

// The button under (x, y), or the nearest within 10 px when the tap lands in a gap; -1 if none
// or on a display-only rect.
int hit(Screen s, int x, int y);
// The nearest button from `from` in direction (dx, dy), by centres; straying sideways costs
// double; left / right, a button more up or down than ahead doesn't count (right from the last
// column stays, rather than jump up to a tab). Up / down keep no such cone: the middle tiles reach
// - and + below them. Returns `from` when nothing lies that way.
int neighbour(Screen s, int from, int dx, int dy);

bool isPreset(Screen s, int id);
int presetSlot(Screen s, int id);  // 0..5 (Presets only), or -1

}  // namespace deckmenu
