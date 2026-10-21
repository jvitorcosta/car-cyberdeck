#pragma once
#include <TFT_eSPI.h>
#include "obdlink.h"
// The old deck's OBD gauge cluster (spec docs/superpowers/specs/2026-10-19-gauge-cluster-design.md,
// drive screens 2026-10-21-companion-drive-design.md): green on black, a tach arc with the speed in
// it, coolant top right, the TRIP panel (distance and time, km/l now, the average; tank % and km
// left when the car reports its fuel level) and the BATTERY panel (volts, charging, the dip at
// start). The bottom 48 px are the lyric strip (companion/carstrip). "--" for anything the car
// hasn't reported; stale values dim; warnings inverted. Redraws only fields that changed.
// Build with -D GAUGE_DEMO to see it with fixed values and no car (companion/app.cpp).
namespace gauges {
constexpr int STRIP_Y = 192;   // the lyric strip below the panels
constexpr int TOP_H = 20;      // drawTop's strip
void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);
void invalidate();   // full repaint on the next draw; expects the screen already black
// The stopped view's strip over her lyrics (y 0..TOP_H): speed, coolant, volts, trip km.
void drawTop(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);
void invalidateTop();   // repaint the strip whole on the next drawTop
}
