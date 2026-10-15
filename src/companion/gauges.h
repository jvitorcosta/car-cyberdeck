#pragma once
#include <TFT_eSPI.h>
#include "obdlink.h"
// The old deck's OBD gauges in the battle-UI look: RPM as a wide HP-style bar, speed as the
// biggest number, coolant / battery / load / throttle in four small boxes, her message bar kept
// at the bottom. Redraws only boxes whose shown text changed.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md ("Screen")
namespace gauges {
void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);
void invalidate();
}
