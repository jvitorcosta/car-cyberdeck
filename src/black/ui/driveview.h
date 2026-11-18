#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// The GAUGES Drive dash (screen 2 of the CarDeck mocks), in the Suki colours but drawn from scratch:
// the speed as the one big number in cyan seven-segment digits over ghost 888s, a 20-segment RPM arc
// above it (red from 5600) with the RPM in thousands, coolant / load / volts ladders and a warm-up bar
// (coolant 40 to 85 C). Red means a problem: a coolant over 105 C or 60 s of a low battery turns its
// value inverted red until it clears. The maths is util/car/drivedash's. A 320 x 240 frame sprite and
// its baked background live in PSRAM; each part is pushed only when it changed. UI loop only.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Drive dash")
namespace ui {
bool driveReady(TFT_eSPI& tft);   // the two PSRAM buffers are there (made once): else the caller draws another screen
void drawDrive(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole);
}
