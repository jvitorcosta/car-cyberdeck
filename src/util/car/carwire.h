#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "carhealth.h"
#include "tripsum.h"
// The /car snapshot: one flat line of JSON the OBD deck serves on GET /car and the yellow deck reads.
// Short fixed keys, a version ("v":1) so an old deck refuses a newer format, a sequence number so a
// stale reply is spotted. Unknown readings travel as null and come back as NAN. The two "why" strings
// are escaped. decode() ignores key order and unknown keys, and leaves *s alone unless the whole
// body is good. No heap. PURE, host-tested.
namespace carwire {
enum class Link : uint8_t { Off, Connecting, NoEngine, Live };  // obdplan::Link's values
struct Snapshot {
    Link link = Link::Off;
    bool checkReady = false;  // first codes read + a volts reading are in, or 20 s passed
    float kmh = NAN, rpm = NAN, volts = NAN, coolantC = NAN, kmlNow = NAN;
    carhealth::Verdict battery{}, engine{};  // level, known, why
    tripsum::Summary trip{};                 // this power-on so far
    float tripCostRs = NAN;
    bool hasLast = false;
    tripsum::Summary last{};
    float lastCostRs = NAN;
    float rangeKm = NAN, pricePerL = NAN;
    bool critical = false;  // carhealth::Watch::critical()
    bool sim = false;       // a bench build's simulated drive (obddeck_bench), never the car
    uint32_t seq = 0;       // +1 per snapshot: the yellow deck spots a stale reply
};
constexpr size_t MAX_JSON = 768;  // every real at REAL_CHARS still fits (test_widest_reals_fit_max_json)
constexpr int REAL_CHARS = 12;    // a real printed wider than this goes out as null
// NUL-terminated JSON in out; returns its length, or 0 if it doesn't fit in n. NAN/inf, and a finite
// value wider than REAL_CHARS, go out as null.
size_t encode(const Snapshot& s, char* out, size_t n);
// False on junk, a cut-off body or a missing/other "v"; *s is then untouched.
bool decode(const char* json, Snapshot* s);
}  // namespace carwire
