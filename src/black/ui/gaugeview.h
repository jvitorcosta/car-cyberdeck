#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// CITY DATALINK: the live gauge screen (car/gauges' switch puts it on), street look. The 320 x 240
// landscape screen on deep violet: a header (CITY DATALINK, the blinking SHIFT tag, the clock, the
// link tag), the RPM ramp (24 slanted blocks rising to the right, cyan / pink / red, a peak marker)
// over its scale, the speed in drawn pink 7-segment digits with their unlit segments faint, the RPM
// in digits, throttle and load cells, three chrome pills (coolant / intake / battery, red on a
// warning) and the street panel: the navigator, whose face follows the engine (util/car/streetface),
// stripes that run with the speed and the last 12 s of RPM. The maths is util/car/gaugehud's and
// streetface's. whole: the frame and everything; otherwise only what changed since the last call,
// plus the panel (a PSRAM sprite) every call. UI loop only.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md
namespace ui {
void drawGauges(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole);
}
