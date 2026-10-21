#pragma once
#include <cstdint>
#include "../util/obd.h"
#include "../util/obdplan.h"
// The old deck's link to an ELM327 over Bluetooth Classic (SPP; -D OBD_BLE for BLE through NimBLE,
// util/obdble): finds the adapter, sets it up, asks the gauges' values in obdplan's order and
// publishes them. Its own task (a Bluetooth connect blocks for seconds). Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdlink {
struct View {
    obdplan::Link link;
    float value[(int)obd::Pid::COUNT];
    bool have[(int)obd::Pid::COUNT];        // a reading exists
    uint32_t atMs[(int)obd::Pid::COUNT];    // millis() of the last good reading
    char adapter[32];                       // the adapter's Bluetooth name ("" before one)
    // Derived (spec 2026-10-19-gauge-cluster), NAN when unknown:
    float kmlNow, kmlAvg, kmLeft;           // util/fueleco
    float tripKm;                           // distance since power-on (util/fueleco::Trip)
    float pedalPct, throttlePct;            // util/obdlearn::Range, 0..100
    float atStart;                          // util/obdlearn::StartDip: the battery's crank dip
    bool fuelCut;                           // FuelSys says decel fuel cut (else pedal up, revs, speed)
    uint32_t supported[4];                  // the car's supported-PID scan (all 0 before one)
};
// p's value while fresh (obdplan::staleAfterMs), else NAN.
inline float freshValue(const View& v, obd::Pid p, uint32_t nowMs) {
    return v.have[(int)p] && nowMs - v.atMs[(int)p] < obdplan::staleAfterMs(p) ? v.value[(int)p]
                                                                            : __builtin_nanf("");
}
void start();                // the OBD task on core 0 (no-op if Bluetooth can't start)
void snapshot(View& out);    // copy under the lock (UI loop)
}
