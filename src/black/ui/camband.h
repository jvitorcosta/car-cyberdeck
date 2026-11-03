#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/cameras.h"
#include "../../util/car/camlist.h"
// The camera alert on the black deck's screen, over whatever screen shows: for Alert and Alarm a
// band over the bottom BAND_H rows (the kind's icon, RADAR 60 / LOMBADA 40 / SINAL, the camera's
// label, the distance and the speed; red for an alarm; a MUTE tag before the label while the beeps
// are off, so a mute nobody meant shows where the driver looks), for NoFix a small `GPS --` tag in
// the top left corner (TAG), for Off nothing. main.cpp keeps the screens out of the band's rows (a
// TFT viewport over the rest) while it shows; the tag needs no rows: it covers only the top strip's
// left end (the "NOW PLAYING" / "PAUSED" title, left of the spinning CD; empty on the status
// screens), which the screens redraw only with the strip, and main.cpp calls invalidate() then.
// When either goes, the screen redraws whole. UI loop only.
namespace camband {
constexpr int BAND_H = 48;
struct TagRect { int x, y, w, h; };
constexpr TagRect TAG = {2, 2, 54, 16};   // the most the tag may cover ("GPS --" is ~44 px wide)
// The bottom rows this status needs: BAND_H or 0.
int rows(const cameras::Status& s);
// Draws what changed since the last call: all of it on a new stage or point, else only the numbers
// (distance in 10 m steps, speed), so the band costs the battle screen nothing between fixes.
// `active` is the point at s.index, nullptr when none; `mute`: the beeps are off. Off draws nothing
// (the screen redraws).
void draw(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* active, bool mute);
void invalidate();   // something drew over it: the next draw() is whole
}  // namespace camband
