#pragma once
#include <cstddef>
#include <cstdint>
// The OBD deck's per-second drive log, for checking the car's readings offline (OBD speed against
// the phone's GPS at 60 / 80 / 100 km/h, what PID 01 A4 says, the N/V ratio): one row a second while
// the car answers, kept in RAM and sent to a private GitHub repo in parts over the hotspot
// (obd/dlogup). No position: nothing here knows where the car is. PURE, host-tested.
//   drive/<trip>/p0001.csv     the rows (CSV, HEADER first), BATCH rows per part
//   drive/<trip>/linkup1.txt   the raw replies at a link-up (the supported-PID bitmaps, 01 A4)
// <trip> is the UTC time of the trip's first record, "20261119-225001". A part is created once and
// never updated (the Contents API needs the old file's sha to change one): tools/dlog_join.py joins
// a trip's parts into one CSV.
namespace drivelog {
constexpr int16_t NONE = INT16_MIN;     // a missing or stale value in a Row
constexpr uint32_t BATCH = 60;          // rows per part: a minute of driving
constexpr uint32_t CAP = 360;           // rows kept while the hotspot is away (6 min, ~7 KB)
constexpr float NV_MIN_KMH = 5.0f;      // N/V is blank below this (a crawl, a stop)

// One second's readings. NAN: missing or stale.
struct Sample {
    float kmh, rpm, coolantC, iatC, throttlePct, pedalPct, loadPct, volts;
};
// What the ring keeps (20 bytes): tenths, the rpm as is, hundredths of a volt; NONE for NAN.
struct Row {
    uint32_t ms;                         // millis() of the sample
    int16_t kmh10, rpm, coolant10, iat10, throttle10, pedal10, load10, volt100;
};
Row pack(uint32_t ms, const Sample& s);

// N/V, rpm per km/h: the CVT's ratio standing in for a gear (a CVT has none). NAN below NV_MIN_KMH,
// at 0 rpm or with either missing.
float nv(float rpm, float kmh);

// The CSV's first line. t_utc: unix seconds (one decimal), ms: the deck's millis().
extern const char HEADER[];
// One row as a CSV line ending in '\n', blank cells for NONE. unixT <= 0: t_utc blank (no clock).
// Returns its length, 0 when it doesn't fit in n (out is then "").
size_t csvLine(const Row& r, double unixT, char* out, size_t n);
// A row's unix time from the clock now: nowUnix at nowMs, the row taken at rowMs (before it).
double rowUnix(int64_t nowUnix, uint32_t nowMs, uint32_t rowMs);

// The rows not yet sent, oldest first, numbered from 0 for the whole power-on. Full: the oldest is
// dropped (counted). A part is read with at(); release(upTo) drops every row numbered below upTo
// once its part is on GitHub (rows dropped meanwhile are simply gone already). Not thread-safe:
// the caller locks.
class Ring {
public:
    void push(const Row& r);
    uint32_t size() const { return count_; }
    uint32_t firstSeq() const { return next_ - count_; }   // the oldest row's number
    uint32_t nextSeq() const { return next_; }
    const Row& at(uint32_t i) const;                       // i-th oldest, i < size()
    void release(uint32_t upTo);
    uint32_t dropped() const { return dropped_; }
private:
    Row rows_[CAP] = {};
    uint32_t head_ = 0, count_ = 0, next_ = 0, dropped_ = 0;
};

// A part is due: BATCH rows waiting, or any row waiting since maxWaitMs (a short last minute).
bool partDue(uint32_t pending, uint32_t oldestMs, uint32_t nowMs, uint32_t maxWaitMs);

// Base64 (RFC 4648, padded). b64Len(n): the encoded length of n bytes. b64(): writes it terminated;
// returns the length, 0 when cap is too small.
size_t b64Len(size_t n);
size_t b64(const uint8_t* in, size_t n, char* out, size_t cap);

// "drive/20261119-225001" from the trip's first unix second (UTC). False before 2020 (no clock).
bool tripDir(int64_t unixStart, char* out, size_t n);
// "<dir>/p0007.csv", "<dir>/linkup2.txt".
bool partPath(const char* dir, uint32_t part, char* out, size_t n);
bool linkupPath(const char* dir, uint32_t k, char* out, size_t n);

// The Contents API's PUT body for a new file: {"message":"<message>","content":"<base64 of data>"}.
// message is copied as is: plain ASCII, no quote or backslash (the deck writes it). Returns the
// length, 0 when cap is too small. bodyLen() is that length, for sizing the buffer.
size_t bodyLen(const char* message, size_t n);
size_t putBody(const char* message, const uint8_t* data, size_t n, char* out, size_t cap);

// The link-up's raw replies, one "<request> -> <reply>" line each ('\r' / '\n' inside a reply as
// '|', anything outside printable ASCII as '?'). Appends to out (terminated); false when it's full.
bool appendRaw(char* out, size_t cap, const char* request, const char* raw);

// What became of a PUT. Done: created (201), or already there (422: a retry of a part whose 201
// was lost; the path is unique per part). Refused: the token or repo is wrong (401, 403, 404), wait
// long. Retry: no answer, GitHub busy (429, 5xx) or anything else, soon again.
enum class Outcome : uint8_t { Done, Refused, Retry };
Outcome outcome(int http);
}
