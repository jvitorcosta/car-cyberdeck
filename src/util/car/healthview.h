#pragma once
#include <cmath>
#include <cstdint>
#include "carhealth.h"
#include "tripsum.h"
// What the car-health screens say, as plain text lines plus a colour role per line (the deck maps a
// Role to a termpal colour; nothing here knows about pixels). Screens A/B (the check), the band of the
// driving screen C, and the parked summary D, plus the gate that decides when D shows. Every text is
// all caps, <= 39 chars, NUL-terminated. Unknown values (NAN) print as "--". R$ uses a comma decimal;
// km and km/L one decimal; range whole km. PURE, host-tested.
namespace healthview {
enum class Role : uint8_t { Text, Mid, Dim, Alert };  // mapped to termpal by the deck
struct Line {
    char text[40];
    Role role;
};
// One full row each, label and value together, drawn left-aligned.
struct Check {
    Line battery, batteryWhy, engine, engineWhy, lastTrip, range, status;
    bool problem;  // any item at Level::Problem
};
// Screen A/B. `last` may be null (no trip yet). status: ">2 ISSUES  [TAP FOR DETAILS]" when anything is a
// problem, else ">1 NOTE  [TAP FOR DETAILS]" for notes, else ">ALL SYSTEMS NOMINAL" only when both items
// are known (Verdict::known); while either is still unknown it is ">CHECKING..." (Dim). An unknown item
// reads "BATTERY  CHECKING" / "ENGINE  CHECKING" (Dim) with an empty why-line.
Check check(const carhealth::Verdict& batt, float volts, const carhealth::Verdict& eng,
            const tripsum::Summary* last, float lastCostRs, float rangeKm);

// Screen C's band: "12.4 KM", "11.8 KM/L", "296 KM LEFT" and "0:16  R$ 6,30  14.1V  90C" (H:MM).
struct Band {
    char trip[12], kmlNow[12], range[16], small[40];
};
Band band(float tripKm, float kmlNow, float rangeKm, uint32_t seconds, float costRs, float volts, float coolantC);

// Screen D. An unknown item reads "BATTERY --" / "ENGINE --" (Dim).
struct Parked {
    Line km, minutes, kml, cost, small, battery, engine, footer;
};
Parked parked(const tripsum::Summary& s, float costRs, float rangeKm, const carhealth::Verdict& batt,
              const carhealth::Verdict& eng);

// D shows after 5 s below 1 km/h on a trip of >= 0.5 km; it hides above 3 km/h or on dismiss(), and
// after a dismiss it stays hidden until the car has moved (> 3 km/h) again. An unknown speed neither
// starts nor continues the 5 s, and leaves a shown D as it is.
class ParkGate {
public:
    bool update(float kmh, float tripKm, uint32_t nowMs);  // true while D should show
    void dismiss();                                        // a tap
private:
    bool shown_ = false, dismissed_ = false, stopped_ = false;
    uint32_t stoppedSinceMs_ = 0;
};
}  // namespace healthview
