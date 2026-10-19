#pragma once
#include <TFT_eSPI.h>
#include "obdlink.h"
// The old deck's OBD gauge cluster (spec docs/superpowers/specs/2026-10-19-gauge-cluster-design.md):
// green on black, a tach arc with the speed in it, coolant top right, the FUEL panel (km/l now,
// tank %, average, km left) and the BATTERY panel (volts, charging, the dip at start), pedal vs
// throttle bars. Zero Two sits top left at half size (avatarview::HALF). "--" for anything the car
// hasn't reported; stale values dim; warnings inverted. Redraws only fields that changed.
// Build with -D GAUGE_DEMO to see it with fixed values and no car (companion/app.cpp).
namespace gauges {
void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);
void invalidate();   // full repaint on the next draw; expects the screen already black
}
