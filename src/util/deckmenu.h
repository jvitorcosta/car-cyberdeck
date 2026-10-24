// Screen layouts and navigation of the yellow deck's terminal-style app: a Menu screen (the
// dashboard), a Lights screen and a Karaoke screen, each a list of rectangles on the 320x240
// display. Hit-testing and D-pad neighbour search live here as generic functions over a rect list.
// PURE, host-tested.
#pragma once
#include <stdint.h>

namespace deckmenu {

struct Rect { int16_t x, y, w, h; };
enum class Screen : uint8_t { Menu, Lights, Karaoke };

// Menu (the dashboard): two items side by side, finger-sized, at the bottom under the lights band;
// everything else on it is display only (the bands below).
enum MenuId : int { M_ITEM0 = 0, M_N = 2 };
enum Item : uint8_t { LIGHTS, KARAOKE };  // later: CAR, HEART, MUSIC
constexpr int N_ITEMS = 2;              // items shown, in slot order

// Lights: Back, 3 zones, 8 tiles, 4 presets, -, +
enum LightsId : int { L_BACK = 0, L_ZONE0 = 1, L_TILE0 = 4, L_PRE0 = 12, L_MINUS = 16, L_PLUS = 17, L_N = 18 };

// Karaoke: Back (where Lights has it); the rest is display only.
enum KaraokeId : int { K_BACK = 0, K_N = 1 };

// {0,0,0,0} for an item slot that is not built (or an id out of range).
Rect rect(Screen s, int id);

// Display only: never a hit, never a neighbour.
// The Menu's bands, top to bottom: the header line, the clock and the weather (its last row the
// trip, HI / LO and the messages), the lights, then (the items). A tap in a band doesn't snap to an
// item; the 4 rows between the lights band and the items, and those under them, do.
constexpr Rect HEADER{0, 0, 320, 22}, CLOCK_BAND{0, 22, 320, 72}, LIGHTS_BAND{0, 94, 320, 94};  // menu
constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};                                 // lights
constexpr Rect KARAOKE_BODY{0, 44, 320, 196};                                                    // karaoke

// Number of ids on a screen (M_N / L_N / K_N).
int count(Screen s);

// The button under (x, y), or the nearest within 10 px when the tap lands in a gap; -1 if none
// or on a display-only rect.
int hit(Screen s, int x, int y);
// The nearest button from `from` in direction (dx, dy), by centres; straying sideways costs
// double. Returns `from` when nothing lies that way.
int neighbour(Screen s, int from, int dx, int dy);

bool isPreset(Screen s, int id);
int presetSlot(Screen s, int id);  // 0..3 (Lights only), or -1

}  // namespace deckmenu
