#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// The GAUGES Cockpit screen (the full-height Split Cockpit of the mock rounds), drawn from
// util/car/cockpit's scene in the deck's colours: the speed in 112 px seven-segment cells, KM/H and the
// RPM number, a 16-capsule tach with a 0-7 scale, trip and range, and five cards (COOL, BATT, KM/L,
// PEDAL, LOAD) with pill bars. No film art, so it works on any build. A 320 x 240 frame sprite in PSRAM;
// only the regions whose key changed are pushed. UI loop only.
// Spec: docs/superpowers/specs/2026-11-20-cockpit-screen-design.md
namespace ui {
bool cockpitReady(TFT_eSPI& tft);   // the PSRAM frame is there (made once): else the caller draws another screen
// theme: the yellow deck's INSIDE light swatch (karaoke::themeLight()); the colours and the black are the yellow deck's.
// storedCodes / mil: gauges::lamp(). whole: a new screen, redrawn all over.
void drawCockpit(TFT_eSPI& tft, const obdwire::Live& l, int storedCodes, bool mil, uint32_t nowMs, int theme,
                 bool whole);
}
