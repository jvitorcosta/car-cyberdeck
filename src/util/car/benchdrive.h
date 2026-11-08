#pragma once
#include <cstdint>
#include "obdplan.h"
// The simulated drive of the OBD deck's bench build (env obddeck_bench): what the car would answer at
// any moment since power-on, no adapter needed. CYCLE_MS long and repeating: stops long enough for
// screen D, an overheat while moving, a low battery past carhealth's 60 s hold, then a stored code
// with the MIL on. PURE, host-tested.
namespace benchdrive {
constexpr uint32_t CYCLE_MS = 600000;
constexpr uint32_t CONNECT_MS = 5000, LIVE_MS = 15000;   // Connecting, then NoEngine, then Live
constexpr uint32_t HOT_FROM_S = 240, HOT_TO_S = 270;     // of the cycle: coolant at HOT_C
constexpr uint32_t LOWV_FROM_S = 440, LOWV_TO_S = 520;   // battery at LOW_V
constexpr uint32_t CODE_FROM_S = 530;                    // to the cycle's end: 1 stored code, the MIL
constexpr float HOT_C = 108.0f, LOW_V = 11.7f;
struct Sample {
    obdplan::Link link;
    float kmh, rpm, mafGs, volts, coolantC, trimS, trimL;   // NAN until Live
    bool mil;
    int stored;
};
Sample at(uint32_t ms);
}  // namespace benchdrive
