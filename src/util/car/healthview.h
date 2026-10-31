#pragma once
#include <cmath>
#include <cstddef>
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
    Line advice[3];  // screen B only: what to do, in plain words; "" on A
    bool problem;    // any item at Level::Problem
};
// Screen A/B. `last` may be null (no trip yet). status: ">2 ISSUES  [TAP TO CLOSE]" when anything is a
// problem (the details are on screen; a tap goes back to the Menu), else ">1 NOTE" for notes (they
// leave by themselves after 5 s), else ">ALL SYSTEMS NOMINAL" only when both items
// are known (Verdict::known); while either is still unknown it is ">CHECKING..." (Dim). An unknown item
// reads "BATTERY  CHECKING" / "ENGINE  CHECKING" (Dim) with an empty why-line. An engine that's Ok
// shows the last trip's fuel trim when there is one (trimPct, NAN: none). With a problem the advice
// lines take the place of the last trip and the range, which are then "": the most urgent problem's,
// worst first (overheating, the check-engine lamp, a low battery, stored codes, the fuel trim).
Check check(const carhealth::Verdict& batt, float volts, const carhealth::Verdict& eng,
            const tripsum::Summary* last, float lastCostRs, float rangeKm, float trimPct = NAN);
// A/B shows once per power-on, and only with the car still (below 3 km/h; an unknown speed is not
// still): a deck that reboots mid-drive skips it rather than cover the road screen.
inline bool checkMayShow(float kmh) { return kmh < 3.0f; }

// Screen C's band: "12.4 KM", "11.8 KM/L", "296 KM LEFT" and "0:16  R$ 6,30  14.1V  90C" (H:MM).
struct Band {
    char trip[12], kmlNow[12], range[16], small[40];
};
Band band(float tripKm, float kmlNow, float rangeKm, uint32_t seconds, float costRs, float volts, float coolantC);

// Screen C's live warning on the prompt line (the deck draws it in ALERT): ">" and the problem's why.
// While `critical` (coolant hot or the MIL) the engine's, whatever the speed: the only ones worth a
// look while driving. Stopped (below 1 km/h; an unknown speed is not a stop) any Problem, the engine's
// first. Notes never warn. Each kind of warning at most once per WARN_EVERY_MS: the kind is the why
// without its digits ("ENGINE HOT C", "STORED CODES"), so a temperature that climbs 108 -> 109 -> 110
// is one warning, shown with its current text when it is due again; another kind shows at once.
// next() remembers what it returned (it is the caller's to show it).
constexpr uint32_t WARN_EVERY_MS = 120000;
class Warnings {
public:
    // True with the warning in out (cut to n bytes) when one is due now.
    bool next(const carhealth::Verdict& batt, const carhealth::Verdict& eng, bool critical, float kmh, uint32_t nowMs,
              char* out, size_t n);
private:
    struct Shown {
        char kind[sizeof(carhealth::Verdict::why)];  // the why without its digits
        uint32_t atMs;
        bool used;
    };
    Shown shown_[4] = {};  // the last few kinds and when each was returned; the oldest makes room
    bool due(const char* kind, uint32_t nowMs) const;
    void mark(const char* kind, uint32_t nowMs);
};

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
