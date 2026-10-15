#pragma once
#include <cstdint>
#include "../util/obd.h"
#include "../util/obdplan.h"
// The old deck's link to an ELM327 over Bluetooth Classic (BluetoothSerial, master): finds the
// adapter, sets it up, asks the gauges' values in obdplan's order and publishes them. Its own
// task (a Bluetooth connect blocks for seconds). Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdlink {
struct View {
    obdplan::Link link;
    float value[(int)obd::Pid::COUNT];
    bool have[(int)obd::Pid::COUNT];        // a reading exists
    uint32_t atMs[(int)obd::Pid::COUNT];    // millis() of the last good reading
    char adapter[32];                       // the adapter's Bluetooth name ("" before one)
};
void start();                // the OBD task on core 0 (no-op if Bluetooth can't start)
void snapshot(View& out);    // copy under the lock (UI loop)
}
