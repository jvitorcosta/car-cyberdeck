#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// The GAUGES Ride screen: the Suki drive screen R1.1 (util/car/sukiride draws it). The film's art,
// recoloured to the deck's theme (the inside light's swatch, karaoke::light()) and day / night; the
// girl animated from the cels by the true speed; the speed in the bezel window with a 13-sample RPM
// trace; the estimated gear; km/l, intake air and trip km; PEDAL, COOL and BATT ribbons. Red (white in
// the red, orange and pink themes) pulses only for a hot coolant or a low battery (drivedash::Alerts).
// Off the link: "--", the art dimmed, the girl still. The art is linked into the app
// (data/sukiride.bin from tools/gen_suki_ride.py, git-ignored: the film's); a build without it
// (CI's zero-filled stand-in) falls back to the other screens. One region buffer in PSRAM; only the
// regions whose key changed are pushed. UI loop only.
// Spec: docs/superpowers/specs/2026-11-19-suki-drive-screen-design.md
namespace ui {
bool rideReady();   // the art parsed and the buffer is there (once): else the caller draws another screen
// demo: the values are GAUGES DEMO's simulated drive: "DEMO" shows in the top band.
void drawRide(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, int theme, bool night, bool demo, bool whole);
}
