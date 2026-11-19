#pragma once
// The OBD deck's drive log (util/car/drivelog): one row a second while the car answers (speed, rpm,
// coolant, intake air, throttle, pedal, load, volts, N/V; no position), and each link-up's raw
// supported-PID bitmaps and 01 A4, POSTed a minute at a time to DLOG_URL (the owner's Apps Script web app,
// tools/drivelog_sheet.gs) with DLOG_KEY, over the hotspot; the script appends them to that Google
// Sheet, one tab per power-on. Off when either is empty, and on the bench build (simulated rows must
// never reach the log).
// Its own task on core 1 at the lowest priority (the OBD tasks run on core 0): a part goes only when
// the hotspot is up, the clock is set, no update is running and the heap has room for TLS; otherwise
// nothing is tried and the rows wait: 5 min in RAM, then ~13 min more on flash (/dlog.bin, written
// and read by the loop task only), the newest dropped past that. Watched by the task watchdog.
namespace dlogup {
void begin();   // setup(), after carstate::begin() (the flash) and obdlink::start(): the task, when on
void second();  // loop(), once a second: this second's row from the link's fresh readings
}
