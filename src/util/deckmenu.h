// Screen layouts and navigation of the yellow deck's terminal-style app: a Menu screen and a
// Lights screen, each a list of rectangles on the 320x240 display. Hit-testing and D-pad
// neighbour search live here as generic functions over a rect list. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace deckmenu {

struct Rect { int16_t x, y, w, h; };
enum class Screen : uint8_t { Menu, Lights };

// Menu: one item slot (a full-width row, Lights; a later plan adds more), then 4 presets as a
// 2x2 grid of big blocks.
enum MenuId : int { M_ITEM0 = 0, M_PRE0 = 1, M_N = 5 };
enum Item : uint8_t { LIGHTS };  // later: CAR, WEATHER, HEART, MUSIC
constexpr int N_ITEMS = 1;       // items shown, in slot order

// Lights: Back, 3 zones, 8 tiles, 4 presets, -, +
enum LightsId : int { L_BACK = 0, L_ZONE0 = 1, L_TILE0 = 4, L_PRE0 = 12, L_MINUS = 16, L_PLUS = 17, L_N = 18 };

// {0,0,0,0} for an item slot that is not built (or an id out of range).
Rect rect(Screen s, int id);

// Display only: never a hit, never a neighbour.
constexpr Rect HEADER{0, 0, 320, 44}, MENU_STRIP{0, 184, 320, 56};  // menu
constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};    // lights

// Number of ids on a screen (M_N / L_N).
int count(Screen s);

// The button under (x, y), or the nearest within 10 px when the tap lands in a gap; -1 if none
// or on a display-only rect.
int hit(Screen s, int x, int y);
// The nearest button from `from` in direction (dx, dy), by centres; straying sideways costs
// double. Returns `from` when nothing lies that way.
int neighbour(Screen s, int from, int dx, int dy);

bool isPreset(Screen s, int id);
int presetSlot(Screen s, int id);  // 0..3, or -1

}  // namespace deckmenu
