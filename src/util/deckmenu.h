// Screen layouts and navigation of the yellow deck's terminal-style app: a Menu screen, a
// Lights screen, a Weather screen and a Radio screen, each a list of rectangles on the 320x240 display. Hit-testing and D-pad
// neighbour search live here as generic functions over a rect list. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace deckmenu {

struct Rect { int16_t x, y, w, h; };
enum class Screen : uint8_t { Menu, Lights, Weather, Radio };

// Menu: three item slots side by side on one row (LIGHTS, WEATHER, RADIO), over the 4 presets as
// a 2x2 grid of big blocks.
enum MenuId : int { M_ITEM0 = 0, M_PRE0 = 3, M_N = 7 };
enum Item : uint8_t { LIGHTS, WEATHER, RADIO };  // later: CAR, HEART, MUSIC
constexpr int N_ITEMS = 3;                       // items shown, in slot order

// Lights: Back, 3 zones, 8 tiles, 4 presets, -, +
enum LightsId : int { L_BACK = 0, L_ZONE0 = 1, L_TILE0 = 4, L_PRE0 = 12, L_MINUS = 16, L_PLUS = 17, L_N = 18 };

// Weather: Back (where Lights has it); the rest is display only.
enum WeatherId : int { W_BACK = 0, W_N = 1 };

// Radio: Back, like Weather's; the rest is display only.
enum RadioId : int { R_BACK = 0, R_N = 1 };

// {0,0,0,0} for an item slot that is not built (or an id out of range).
Rect rect(Screen s, int id);

// Display only: never a hit, never a neighbour.
constexpr Rect HEADER{0, 0, 320, 44}, MENU_STRIP{0, 184, 320, 56};  // menu
constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};    // lights
constexpr Rect WEATHER_BODY{0, 44, 320, 196};                       // weather
constexpr Rect RADIO_BODY{0, 44, 320, 196};                         // radio

// Number of ids on a screen (M_N / L_N / W_N / R_N).
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
