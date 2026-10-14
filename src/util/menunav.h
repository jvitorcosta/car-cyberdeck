#pragma once
#include <cstdint>
// The companion's screens with the avatar as home: each screen's buttons, the hit test, where a
// button leads and the message bar's idle text. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Screens").
namespace menunav {
enum class Screen : uint8_t { Home, Lyrics, Sounds, Trip };
enum Button : int { None = 0, Lyrics, Sounds, Trip, Back, Howl, Vroom, Reset };
struct Rect { int16_t x, y, w, h; };
struct Btn {
    Button id;
    Rect r;
    const char* label;
};
constexpr int MAX_BTNS = 4;
constexpr Rect BAR{4, 206, 312, 34};   // the message bar (dialogue box) on every screen
// The buttons on screen s; vroom: the new deck offers that clip. Returns how many (Home: none,
// it is just her; the phone page opens the other screens).
int buttons(Screen s, bool vroom, Btn out[MAX_BTNS]);
// The button at (x, y) on screen s, or None.
Button hit(Screen s, bool vroom, int x, int y);
// Whether the new deck offers what button b asks for (VROOM only when it lists the clip). The
// phone page shows every button; one the deck doesn't offer must do nothing.
bool offered(Button b, bool vroom);
// RESET fires after a 1 s hold; every other button on release.
bool isHold(Button b);
// Where button b on screen s leads; action buttons stay on s.
Screen next(Screen s, Button b);
// The message bar when she is quiet: the link state first, then the screen's own hint.
const char* idleText(Screen s, bool deckOk, bool wrongKey);
}
