#pragma once
#include <cstddef>
#include <cstdint>
// The OBD deck's per-second drive log, for checking the car's readings offline (OBD speed against
// the phone's GPS at 60 / 80 / 100 km/h, what PID 01 A4 says, the N/V ratio): one row a second while
// the car answers, kept in RAM (and on flash while the hotspot has no internet) and POSTed in parts
// to the owner's Google Apps Script web app, which appends them to his Google Sheet (tools/drivelog_sheet.gs):
// one tab per power-on, named by its first record's UTC time ("20261119-225001"), and the link-up's
// raw replies on the "linkup" tab. No position: nothing here knows where the car is. PURE, host-tested.
namespace drivelog {
constexpr int16_t NONE = INT16_MIN;     // a missing or stale value in a Row
constexpr uint32_t BATCH = 60;          // rows per part: a minute of driving
constexpr uint32_t CAP = 360;           // rows the RAM ring holds (6 min, ~7 KB)
constexpr uint32_t SPOOL_AT = 300;      // the ring this full: new rows go to flash instead
constexpr uint32_t SPOOL_MAX = 800;     // rows on flash at most (16 KB, ~13 min); past it they're dropped
constexpr float NV_MIN_KMH = 5.0f;      // N/V is blank below this (a crawl, a stop)

// One second's readings. NAN: missing or stale.
struct Sample {
    float kmh, rpm, coolantC, iatC, throttlePct, pedalPct, loadPct, volts;
};
// What the ring and the flash spool keep (20 bytes): tenths, the rpm as is, hundredths of a volt;
// NONE for NAN.
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
constexpr size_t LINE_CAP = 96;         // csvLine's longest line, its '\0' included
// A row's unix time from the clock now: nowUnix at nowMs, the row taken at rowMs (before it).
double rowUnix(int64_t nowUnix, uint32_t nowMs, uint32_t rowMs);

// The rows not yet sent, oldest first, numbered from 0 for the whole power-on. Full: the oldest is
// dropped (counted). A part is read with at(); release(upTo) drops every row numbered below upTo
// once the Sheet has its part (rows dropped meanwhile are simply gone already). Not thread-safe:
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

// Where a new row goes, so the rows stay in order: the ring while it has room and nothing waits on
// flash; flash (appended) once the ring reaches SPOOL_AT or while flash still holds rows; dropped
// when flash is full too.
enum class Route : uint8_t { Ring, Spool, Drop };
Route route(uint32_t ringRows, uint32_t spoolRows);
// How many rows to move from flash (its oldest) to the ring now: up to a BATCH once the ring has
// fewer than a BATCH, never past SPOOL_AT.
uint32_t refill(uint32_t ringRows, uint32_t spoolRows);

// A part is due: BATCH rows waiting, or any row waiting since maxWaitMs (a short last minute).
bool partDue(uint32_t pending, uint32_t oldestMs, uint32_t nowMs, uint32_t maxWaitMs);

// The trip's tab: "20261119-225001" from its first unix second (UTC). False before 2020 (no clock).
bool tripName(int64_t unixStart, char* out, size_t n);

// The POST body's first two lines, before the payload: "<key>\n<kind>\t<trip>\t<num>\n". kind:
// "rows" (the payload is HEADER and the part's lines; num is the part, from 1) or "linkup" (the
// payload is appendRaw's lines; num is the link-up). Returns the length, 0 when it doesn't fit or a
// field holds a tab or a line break.
size_t postHead(char* out, size_t n, const char* key, const char* kind, const char* trip, uint32_t num);

// The link-up's raw replies, one "<request> -> <reply>" line each ('\r' / '\n' inside a reply as
// '|', anything outside printable ASCII as '?'). Appends to out (terminated); false when it's full.
bool appendRaw(char* out, size_t cap, const char* request, const char* raw);

// What became of a POST. The web app runs doPost and answers 302 to a page that holds its reply;
// http and body are that page's (or the POST's own, when it didn't redirect). Done: "ok" (taken)
// or "ok dup" (a retry of a part it already has). Refused: "bad key", or 401 / 403 / 404 (the key,
// URL or deployment is wrong): wait long. Retry: anything else (no answer, Google busy).
enum class Outcome : uint8_t { Done, Refused, Retry };
Outcome outcome(int http, const char* body);
}
