#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// CITY DATALINK: the live gauge screen (car/gauges' switch puts it on). The 320 x 240 landscape
// screen, dark, all blocks and lines: a header (name, the blinking SHIFT tag, the link tag), the RPM
// ramp (24 slanted blocks rising to the right, green / amber / red, a peak marker) over its scale
// and the RPM in digits, the speed in drawn 7-segment digits with their unlit segments faint, the
// throttle and load bars, and the coolant / intake / battery tiles (inverted on a warning). The
// maths is util/car/gaugehud's. whole: the frame and everything; otherwise only what changed since
// the last call. UI loop only.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md
namespace ui {
void drawGauges(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole);
}
