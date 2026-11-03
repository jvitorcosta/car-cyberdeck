#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/cameras.h"
#include "../../util/car/camlist.h"
// The camera alert on the black deck's screen, over whatever screen shows: for Alert and Alarm a
// band over the bottom BAND_H rows (the kind's icon, RADAR 60 / LOMBADA 40 / SINAL, the camera's
// label, the distance and the speed; red for an alarm), for NoFix a thin strip with a `GPS --` tag
// in the corner, for Off nothing. main.cpp keeps the screens out of those rows (a TFT viewport over
// the rest) while it shows, and redraws the screen whole when it goes. UI loop only.
namespace camband {
constexpr int BAND_H = 48;
constexpr int TAG_H = 16;
// The bottom rows this status needs: BAND_H, TAG_H or 0.
int rows(const cameras::Status& s);
// Draws what changed since the last call: all of it on a new stage or point, else only the numbers
// (distance in 10 m steps, speed), so the band costs the battle screen nothing between fixes.
// `active` is the point at s.index, nullptr when none. Off draws nothing (the screen redraws).
void draw(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* active);
void invalidate();   // something drew over it: the next draw() is whole
}  // namespace camband
