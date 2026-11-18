#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// The GAUGES Trip ticker (screen 3 of the CarDeck mocks), in the yellow deck's green VT323 terminal
// look: "> TRIP · nn MIN" and the link's state on top, six cells (KM/L NOW the bright one, the trip's
// km/l, km and time, the range and the trip's R$), a prompt line at the bottom. The text is
// util/car/tripticker's; a low range turns its cell inverted red. A 320 x 240 frame sprite in PSRAM;
// each cell is pushed only when its text changed. UI loop only.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Trip ticker")
namespace ui {
bool tripReady(TFT_eSPI& tft);   // the PSRAM frame is there (made once): else the caller draws another screen
void drawTrip(TFT_eSPI& tft, const obdwire::Live& l, bool whole);
}
