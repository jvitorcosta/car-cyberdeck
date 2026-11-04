#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/cameras.h"
#include "../../util/car/camlist.h"
// The camera alert on the black deck's screen, over whatever screen shows. For Alert and Alarm:
// - the sign panel (PANEL, the right 128 x 192 px above the band): the point as a Brazilian road
//   sign, pictograms only. Speed camera: the round limit sign (R-19, `--` for an unknown limit)
//   over a camera badge; bump: the limit sign over the yellow bump diamond (A-18); red light: the
//   yellow traffic-light diamond (A-14) over the camera badge. In an alarm the limit sign swaps to
//   its red face at 2 Hz (util/camsign; only the sign is redrawn at each flip) and `+N` km/h over
//   the limit replaces what is under it.
// - the band over the bottom BAND_H rows: the kind word small (RADAR / LOMBADA / SINAL, in the
//   accent colour: yellow, orange for a red light, white on the red alarm band), a MUTE tag under
//   it while the chime and voice are off (a mute nobody meant shows where the driver looks), the
//   distance and the speed large, and along its top a 4 px countdown bar from the alert distance
//   to the point (util/camsign: it never grows back). Solid red for an alarm.
// For NoFix a small `GPS --` tag in the top left corner (TAG), for Off nothing. main.cpp keeps the
// screens inside screens() (a TFT viewport, left of the panel and above the band) while the band
// shows; the tag needs no rows: it covers only the top strip's left end (the "NOW PLAYING" /
// "PAUSED" title, left of the spinning CD; empty on the status screens), which the screens redraw
// only with the strip, and main.cpp calls invalidate() then. When either goes, the screen redraws
// whole. Assumes the 320 x 240 landscape screen. UI loop only.
namespace camband {
constexpr int BAND_H = 48;
struct Rect { int x, y, w, h; };
constexpr Rect TAG = {2, 2, 54, 16};       // the most the tag may cover ("GPS --" is ~44 px wide)
constexpr Rect PANEL = {192, 0, 128, 192};  // the sign panel, right of the screens, above the band
// The bottom rows this status needs: BAND_H or 0 (the panel comes and goes with the band).
int rows(const cameras::Status& s);
// Where the screens may draw on a w x h screen: all of it, or left of the panel and above the band
// while they show.
Rect screens(const cameras::Status& s, int w, int h);
// Draws what changed since the last call: all of it on a new stage, point or mute, else only the
// numbers (distance in 10 m steps, speed), the countdown bar, the alarm's `+N` and the sign at a
// blink flip, so the panel costs the battle screen nothing between fixes.
// `active` is the point at s.index, nullptr when none; `mute`: the chime and voice are off. Off
// draws nothing (the screen redraws).
void draw(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* active, bool mute);
void invalidate();   // something drew over it: the next draw() is whole
}  // namespace camband
