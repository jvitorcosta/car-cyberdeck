#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "obd.h"
#include "obdplan.h"
// The live gauges' values (the HUD's seven readings and the link state), and GET /obd: the black
// deck, which owns the OBD link, serves them as one flat JSON line to the other decks.
//   {"v":1,"link":3,"rpm":2450,"kmh":62,"cool":88,"iat":34,"thr":18.4,"load":41.2,"volt":14.1,"seq":812}
// A missing or stale reading travels as null and comes back as NAN; a reader refuses another "v".
// No heap. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md
namespace obdwire {
struct Live {
    obdplan::Link link = obdplan::Link::Off;
    float rpm = NAN, kmh = NAN, coolC = NAN, intakeC = NAN;
    float throttle = NAN, load = NAN;   // %
    float volts = NAN;                  // the adapter's own reading (ATRV)
    uint32_t seq = 0;                   // +1 per new snapshot: a reader spots a stale reply
};
constexpr size_t MAX_JSON = 160;   // the widest real values fit (test_too_small_a_buffer_writes_nothing)

// NUL-terminated JSON in out; its length, or 0 when it doesn't fit in n (out is then "").
size_t encode(const Live& l, char* out, size_t n);
// False on junk, a cut-off body, a missing / other "v" or a missing / unknown "link"; *l is then
// untouched. Keys in any order, unknown ones ignored, missing values NAN.
bool decode(const char* json, Live* l);

// Where a reading is kept: the City's extended coolant / intake (67 / 68) stand in for 05 / 0F.
obd::Pid slot(obd::Pid p);
// The HUD's values from a link's readings (obdlink::View's arrays, indexed by obd::Pid): a reading
// counts while it is fresh (obdplan::staleAfterMs), else NAN.
Live fromReadings(obdplan::Link link, const float* value, const bool* have, const uint32_t* atMs,
                  uint32_t nowMs);
}  // namespace obdwire
