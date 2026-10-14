#pragma once
#include <TFT_eSPI.h>
#include "../util/menunav.h"
// Drawing for the companion's avatar-home screens: background, buttons, the message bar and the
// trip time. Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Screens").
namespace screens {
// Repaints screen s: background, its buttons (cursor marks the last menu choice on Home), and
// an empty message bar. Her portrait and the lyric column are drawn by their own modules.
void draw(TFT_eSPI& t, menunav::Screen s, bool vroom, menunav::Button cursor);
// One button: normal, pressed (dark), with a hold fill 0..1 from the left; cursor: the "▶" mark.
void button(TFT_eSPI& t, const menunav::Btn& b, bool pressed, float fill, bool cursor);
// The message bar's text, repainted only when it changes.
void bar(TFT_eSPI& t, const char* text);
// The trip time (h:mm, or m:ss under an hour), repainted when the shown text changes.
void tripTime(TFT_eSPI& t, uint32_t seconds);
void invalidate();   // forget the shown bar text and trip time (after a full repaint)
}
