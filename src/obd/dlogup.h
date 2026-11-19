#pragma once
// The OBD deck's drive log (util/car/drivelog): one row a second while the car answers (speed, rpm,
// coolant, intake air, throttle, pedal, load, volts, N/V; no position), and each link-up's raw
// supported-PID bitmaps and 01 A4, sent as new files to DLOG_REPO (a private GitHub repo) with
// DLOG_TOKEN over the hotspot, one folder per power-on. Off when either is empty, and on the bench
// build (simulated rows must never reach the log).
// Its own task on core 1 at the lowest priority (the OBD tasks run on core 0): a part goes only when
// the hotspot is up, the clock is set, no update is running and the heap has room for TLS; otherwise
// the rows wait in RAM (drivelog::CAP, the oldest dropped past it). Watched by the task watchdog.
namespace dlogup {
void begin();   // setup(), after obdlink::start(): the task, when on
void second();  // loop(), once a second: this second's row from the link's fresh readings
}
