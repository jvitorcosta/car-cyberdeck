#pragma once
#include "../../util/car/drivedash.h"
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
bool demoOn();   // the demo is what live() returns now (the screens mark it). Any task.
// The values now (fresh readings only), seq +1 whenever a new reading came in. Any task.
obdwire::Live live();
// Which GAUGES screen shows (util/car/drivedash::Screen), kept in NVS ("hud" / "screen"); the Suki
// screen until one is picked. setScreen refuses (false) while the car moves (drivedash::canSwitch on
// live()'s speed). Any task.
drivedash::Screen screen();
bool setScreen(drivedash::Screen s);
// The trip and the fuel book (util/car/tripbook, fuelbook; NVS "car" / "fuel"): the trip time, the
// litres charged to the book every 30 s, and its range and price in live(). From the UI loop, every
// pass (it runs at most 4 times a second).
void tick();
// A full tank (GET /fill): the burn since starts again from 0. A price inside fuelbook's range replaces
// the old one; NAN keeps it. Any task: noted here, taken by the next tick(), which charges the old tank
// first and saves the book.
void fill(float pricePerL);
float price();   // the book's R$ per litre, NAN without one. Any task.
void forgetBook();   // the desk's FILL CLEAR: the fuel book back to empty (no full tank, no price)
}
