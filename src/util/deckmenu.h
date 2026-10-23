// Screen layouts and navigation of the yellow deck's terminal-style app: a Menu screen and a
// Lights screen, each a list of rectangles on the 320x240 display. Hit-testing and D-pad
// neighbour search live here as generic functions over a rect list. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace deckmenu {

struct Rect { int16_t x, y, w, h; };
enum class Screen : uint8_t { Menu, Lights };

// Menu: up to 6 item slots (3 cols x 2 rows), then 4 favourites. Part 1 builds one item, Lights.
enum MenuId : int { M_ITEM0 = 0, M_FAV0 = 6, M_N = 10 };
enum Item : uint8_t { LIGHTS };  // later: CAR, WEATHER, HEART, MUSIC
constexpr int N_ITEMS = 1;       // items shown, in slot order

// Lights: Back, 3 zones, 8 tiles, 4 favourites, -, +
enum LightsId : int { L_BACK = 0, L_ZONE0 = 1, L_TILE0 = 4, L_FAV0 = 12, L_MINUS = 16, L_PLUS = 17, L_N = 18 };

// {0,0,0,0} for an item slot that is not built (or an id out of range).
Rect rect(Screen s, int id);

// Display only: never a hit, never a neighbour.
constexpr Rect HEADER{0, 0, 320, 34}, MENU_STRIP{0, 184, 320, 56};  // menu
constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};    // lights

// Number of ids on a screen (M_N / L_N).
int count(Screen s);

// The button under (x, y), or the nearest within 10 px when the tap lands in a gap; -1 if none
// or on a display-only rect.
int hit(Screen s, int x, int y);
// The nearest button from `from` in direction (dx, dy), by centres; straying sideways costs
// double. Returns `from` when nothing lies that way.
int neighbour(Screen s, int from, int dx, int dy);

bool isFav(Screen s, int id);
int favSlot(Screen s, int id);  // 0..3, or -1

constexpr uint32_t HOLD_MS = 800;  // long-press / hold A on a Lights favourite saves it

}  // namespace deckmenu
