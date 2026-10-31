#pragma once
#include <cstdint>
#include "../util/car/obd.h"
#include "../util/car/obdplan.h"
// The OBD deck's link to the ELM327 adapter (the Vgate) over Bluetooth Low Energy (NimBLE 2), its only
// BLE link. Two tasks on core 0: the link task finds and connects the adapter (the one place that
// connects, with a 5 s..60 s backoff); once connected, the ask task sets the adapter up and asks the
// car's values in obdplan's order, read-only: every write goes through obd::allowed(). Ported from the parked yellow-deck attempt (feat/obd's BLE path, NimBLE 2.5.1).
// Spec: docs/superpowers/specs/2026-10-28-car-health-check-design.md ("Architecture update")
namespace obdlink {
struct View {
    obdplan::Link link;
    uint32_t nowMs;                          // millis() when the snapshot was taken
    float value[(int)obd::Pid::COUNT];
    bool have[(int)obd::Pid::COUNT];         // a reading exists
    uint32_t atMs[(int)obd::Pid::COUNT];     // millis() of the last good reading
    // The last good 03 / 07 read: counts (-1 before one this power-on), the stored codes first, then
    // the pending ones while they fit, and the check-engine lamp (PID 01). codesReads goes up by one
    // per good read: the caller's cue for carhealth::Watch::codesRead() and the "new code" flags.
    int stored, pending;
    char codes[4][6];
    bool mil;
    uint32_t codesReads;
    // fueleco since power-on, stepped only with fresh values: litres burnt, how many steps had an air
    // reading (unchanged = nothing counted, so the caller feeds NAN litres, not 0), km/L now.
    float litresTotal;
    uint32_t airSteps;
    float kmlNow;
};
// Starts NimBLE (central only) and the two tasks. Logs "[obd] ..." from then on.
void start();
void snapshot(View& out);     // under a mutex, from any task
// p's value while fresh (obdplan::staleAfterMs), else NAN.
float fresh(const View& v, obd::Pid p, uint32_t nowMs);
// The tasks' least stack to spare, for [mem] (0 before start).
unsigned linkStackLeft();
unsigned askStackLeft();
}
