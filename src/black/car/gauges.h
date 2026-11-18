#pragma once
#include "../../util/car/obdwire.h"
// The black deck's live gauges: the OBD link (src/obd/obdlink, the same BLE link the OBD deck runs,
// built in with -D OBD_LINK) and the GAUGES switch that puts the HUD (ui/gaugeview) on the screen.
// The switch is kept in NVS ("hud" / "on"), so it survives a power cycle: on once, the deck starts on
// the gauges in the car. net/remote serves live() on GET /obd and sets the switch on GET /gauges.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md
namespace gauges {
void begin();          // reads the switch; starts the link (OBD_LINK). Once, from setup().
bool on();             // any task
void set(bool on);     // any task; written to NVS only when it changes (and ends a demo)
// The desk demo (core/serialcmd's GAUGES DEMO): the switch on and live() from the OBD deck's simulated
// drive (util/car/benchdrive), in RAM only, until set() or a reboot.
void demo();
// The values now (fresh readings only), seq +1 whenever a new reading came in. Any task.
obdwire::Live live();
}
