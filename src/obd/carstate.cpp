#include "carstate.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include "carfs.h"
#include "obdlink.h"
#include "../common/net/clock.h"
#include "../util/car/carhealth.h"
#include "../util/car/carwire.h"
#include "../util/car/fuelbook.h"
#include "../util/car/triplog.h"
#include "../util/car/tripsum.h"

namespace carstate {
namespace {
using obd::Pid;
static_assert(static_cast<int>(carwire::Link::Live) == static_cast<int>(obdplan::Link::Live) &&
                  static_cast<int>(carwire::Link::NoEngine) == static_cast<int>(obdplan::Link::NoEngine) &&
                  static_cast<int>(carwire::Link::Connecting) == static_cast<int>(obdplan::Link::Connecting),
              "carwire::Link mirrors obdplan::Link");

constexpr uint32_t CHECK_AFTER_LIVE_MS = 20000;   // the check is ready this long after Live anyway

// NVS "last": km, litres (f32), seconds, idleS (u32), avgKmh, costRs (f32), then the carhealth Memo;
// this chip's byte order, read back only by this firmware (the length is checked).
constexpr size_t LAST_BYTES = 24 + carhealth::MEMO_BYTES;

SemaphoreHandle_t s_lock = nullptr;   // the fuel book and the snapshot: the loop task vs the server task
carfs::Store s_store;
triplog::Log s_log(s_store);          // static: its 2 KB queue stays off every task's stack
bool s_logOk = false;
uint32_t s_bootSeq = 0;
char s_tripId[20] = "";               // the rows' "t": set at the first row

carhealth::Watch s_watch;             // loop task only
tripsum::Trip s_trip;                 // loop task only
fuelbook::Book s_book;                // under s_lock
uint8_t s_savedFuel[fuelbook::RECORD_BYTES];
bool s_fuelSaved = false;             // s_savedFuel holds what NVS has (under s_lock)

bool s_hasLast = false;               // the last trip, as loaded at boot
tripsum::Summary s_last{};
float s_lastCost = NAN;
uint8_t s_savedLast[LAST_BYTES];
bool s_lastSaved = false;

int s_knownStored = -1, s_knownPending = -1;   // NVS "codes": the last read's counts (-1: never read)
uint32_t s_codesReads = 0;
uint32_t s_airSteps = 0;
float s_litresSeen = 0.0f;
bool s_liveSeen = false, s_ready = false;
uint32_t s_liveSinceMs = 0;
uint32_t s_seq = 0;

carwire::Snapshot s_snap;             // under s_lock, with its JSON
char s_json[carwire::MAX_JSON];
size_t s_jsonLen = 0;

struct Lock {
    Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_lock); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

bool loadBytes(Preferences& p, const char* key, uint8_t* out, size_t n) {
    return p.isKey(key) && p.getBytesLength(key) == n && p.getBytes(key, out, n) == n;
}

void encodeLast(const tripsum::Summary& s, float cost, const carhealth::Memo& m, uint8_t out[LAST_BYTES]) {
    memcpy(out, &s.km, 4);
    memcpy(out + 4, &s.litres, 4);
    memcpy(out + 8, &s.seconds, 4);
    memcpy(out + 12, &s.idleS, 4);
    memcpy(out + 16, &s.avgKmh, 4);
    memcpy(out + 20, &cost, 4);
    carhealth::encode(m, out + 24);
}

// False on a record that can't be a trip; the Memo is restored only when it decodes.
bool decodeLast(const uint8_t in[LAST_BYTES], tripsum::Summary* s, float* cost, carhealth::Memo* m, bool* memoOk) {
    tripsum::Summary t{};
    float c;
    memcpy(&t.km, in, 4);
    memcpy(&t.litres, in + 4, 4);
    memcpy(&t.seconds, in + 8, 4);
    memcpy(&t.idleS, in + 12, 4);
    memcpy(&t.avgKmh, in + 16, 4);
    memcpy(&c, in + 20, 4);
    if (!std::isfinite(t.km) || t.km < 0.0f || !std::isfinite(t.litres) || t.litres < 0.0f) return false;
    t.kmL = t.litres > 0.0f ? t.km / t.litres : NAN;
    *s = t;
    *cost = std::isfinite(c) ? c : NAN;
    *memoOk = carhealth::decode(in + 24, carhealth::MEMO_BYTES, m);
    return true;
}

void saveFuelLocked() {
    uint8_t rec[fuelbook::RECORD_BYTES];
    fuelbook::encode(s_book, rec);
    if (s_fuelSaved && memcmp(rec, s_savedFuel, sizeof rec) == 0) return;
    Preferences p;
    if (!p.begin("car", false)) return;
    if (p.putBytes("fuel", rec, sizeof rec) == sizeof rec) {
        memcpy(s_savedFuel, rec, sizeof rec);
        s_fuelSaved = true;
    }
    p.end();
}

void saveCodes(int stored, int pending) {
    s_knownStored = stored;
    s_knownPending = pending;
    const uint8_t rec[2] = {static_cast<uint8_t>(stored), static_cast<uint8_t>(pending)};
    Preferences p;
    if (!p.begin("car", false)) return;
    p.putBytes("codes", rec, sizeof rec);
    p.end();
}

// The rows' trip id: the start's local time once SNTP has answered ("20261029-0742"), else the boot
// sequence ("boot000042"); fixed at the first row, so a trip keeps one id.
void makeTripId() {
    if (s_tripId[0]) return;
    int64_t t = 0;
    if (netclock::unixNow(&t)) {
        const time_t start = static_cast<time_t>(t - static_cast<int64_t>(millis() / 1000));
        struct tm lt;
        localtime_r(&start, &lt);
        strftime(s_tripId, sizeof s_tripId, "%Y%m%d-%H%M", &lt);
    } else {
        snprintf(s_tripId, sizeof s_tripId, "boot%06lu", static_cast<unsigned long>(s_bootSeq));
    }
}

const char* linkName(carwire::Link l) {
    switch (l) {
        case carwire::Link::Connecting: return "CONNECTING";
        case carwire::Link::NoEngine: return "NO-ENGINE";
        case carwire::Link::Live: return "LIVE";
        default: return "OFF";
    }
}

const char* levelName(const carhealth::Verdict& v) {
    if (!v.known) return "--";
    return v.level == carhealth::Level::Problem ? "PROBLEM" : v.level == carhealth::Level::Note ? "NOTE" : "OK";
}
}  // namespace

void begin() {
    s_lock = xSemaphoreCreateMutex();
    Preferences p;
    p.begin("car", false);                         // read-write: creates it, no NOT_FOUND log
    s_bootSeq = (p.isKey("tseq") ? p.getUInt("tseq", 0) : 0) + 1;
    p.putUInt("tseq", s_bootSeq);
    // The last power-on's trip: shown as "last trip", and its Memo judged now (the power cut came
    // before it could be).
    uint8_t last[LAST_BYTES];
    carhealth::Memo memo{};
    bool memoOk = false;
    if (loadBytes(p, "last", last, sizeof last) && decodeLast(last, &s_last, &s_lastCost, &memo, &memoOk)) {
        s_hasLast = true;
        memcpy(s_savedLast, last, sizeof last);
        s_lastSaved = true;
        if (memoOk) s_watch.restore(memo);
    }
    s_watch.endTrip();
    uint8_t fuel[fuelbook::RECORD_BYTES];
    if (loadBytes(p, "fuel", fuel, sizeof fuel) && fuelbook::decode(fuel, sizeof fuel, &s_book)) {
        memcpy(s_savedFuel, fuel, sizeof fuel);
        s_fuelSaved = true;
    }
    uint8_t codes[2];
    if (loadBytes(p, "codes", codes, sizeof codes)) {
        s_knownStored = codes[0];
        s_knownPending = codes[1];
    }
    p.end();
    // The trip log: a row cut by the last power cut goes, the oldest trips go past the cap, then this
    // boot's file.
    if (carfs::begin()) {
        if (s_log.recover()) Serial.println("[car] log: cut a partial row left by a power cut");
        s_log.enforceCap();
        s_log.open(s_bootSeq);
        s_logOk = true;
    }
    Serial.printf("[car] boot %lu, last trip %s, fuel book %s\n", static_cast<unsigned long>(s_bootSeq),
                  s_hasLast ? "loaded" : "none", s_fuelSaved ? "loaded" : "empty");
}

void second() {
    obdlink::View v;
    obdlink::snapshot(v);
    const uint32_t now = v.nowMs;
    const bool live = v.link == obdplan::Link::Live;
    const float kmh = obdlink::fresh(v, Pid::Speed, now), rpm = obdlink::fresh(v, Pid::Rpm, now);
    const float volts = obdlink::fresh(v, Pid::Battery, now), cool = obdlink::fresh(v, Pid::Coolant, now);
    const float trimS = obdlink::fresh(v, Pid::TrimS, now), trimL = obdlink::fresh(v, Pid::TrimL, now);
    // This second's fuel: NAN when fueleco counted nothing (no air reading), so tripsum leaves it out
    // instead of logging a false 0.
    const float litres = v.airSteps != s_airSteps ? v.litresTotal - s_litresSeen : NAN;
    s_airSteps = v.airSteps;
    s_litresSeen = v.litresTotal;
    s_trip.add({now, kmh, rpm, litres, volts, cool, trimS, trimL, live && !std::isnan(kmh)});

    uint8_t flags = live && v.mil ? 1 : 0;          // tripsum::Row::flags
    if (v.codesReads != s_codesReads) {             // a good 03 / 07 read
        s_codesReads = v.codesReads;
        s_watch.codesRead();
        if (s_knownStored >= 0 && v.stored > s_knownStored) flags |= 2;
        if (s_knownPending >= 0 && v.pending > s_knownPending) flags |= 4;
        if (v.stored != s_knownStored || v.pending != s_knownPending) saveCodes(v.stored, v.pending);
    }
    s_trip.orFlags(flags);

    carhealth::Reading r;
    r.nowMs = now;
    r.engineRunning = rpm > 0.0f;
    r.volts = volts;
    r.coolantC = cool;
    r.ltftPct = trimL;
    r.storedCodes = v.stored;
    r.pendingCodes = v.pending;
    r.mil = v.mil;
    s_watch.feed(r);

    if (live && !s_liveSeen) {
        s_liveSeen = true;
        s_liveSinceMs = now;
    }
    if (!s_ready)
        s_ready = (s_codesReads > 0 && v.have[(int)Pid::Battery]) || (s_liveSeen && now - s_liveSinceMs >= CHECK_AFTER_LIVE_MS);

    bool rowDone = false;
    tripsum::Row row{};
    if (s_trip.rowReady()) {
        row = s_trip.takeRow();
        rowDone = true;
        makeTripId();
        char line[triplog::LINE_BYTES];
        if (tripsum::jsonRow(row, s_tripId, line, sizeof line) > 0) s_log.queue(line);
        else Serial.printf("[car] minute %u: row too long, dropped\n", static_cast<unsigned>(row.minute));
    }

    carwire::Snapshot s;
    s.link = static_cast<carwire::Link>(v.link);
    s.checkReady = s_ready;
    s.kmh = kmh;
    s.rpm = rpm;
    s.volts = volts;
    s.coolantC = cool;
    s.kmlNow = live ? v.kmlNow : NAN;
    s.battery = s_watch.item(carhealth::Item::Battery);
    s.engine = s_watch.item(carhealth::Item::Engine);
    s.trip = s_trip.summary();
    s.hasLast = s_hasLast;
    s.last = s_last;
    s.lastCostRs = s_lastCost;
    s.critical = s_watch.critical();
    Lock lock;
    fuelbook::burn(&s_book, litres);
    if (rowDone) fuelbook::learn(&s_book, row.km, row.litres);
    s.tripCostRs = fuelbook::costRs(s_book, s.trip.litres);
    s.rangeKm = fuelbook::rangeKm(s_book);
    s.pricePerL = s_book.pricePerL;
    s.seq = ++s_seq;
    s_snap = s;
    s_jsonLen = carwire::encode(s, s_json, sizeof s_json);
    if (!s_jsonLen) Serial.println("[car] snapshot too long for carwire::MAX_JSON");
}

void checkpoint() {
    if (s_logOk && !s_log.flush()) Serial.printf("[car] log write failed, %d rows queued\n", s_log.queued());
    // The last-trip record once this power-on is a trip (the car moved): a deck that only sat in the
    // driveway keeps the previous one.
    const tripsum::Summary sum = s_trip.summary();
    if (sum.km > 0.0f) {
        float cost;
        {
            Lock lock;
            cost = fuelbook::costRs(s_book, sum.litres);
        }
        uint8_t rec[LAST_BYTES];
        encodeLast(sum, cost, s_watch.memo(), rec);
        if (!s_lastSaved || memcmp(rec, s_savedLast, sizeof rec) != 0) {
            Preferences p;
            if (p.begin("car", false)) {
                if (p.putBytes("last", rec, sizeof rec) == sizeof rec) {
                    memcpy(s_savedLast, rec, sizeof rec);
                    s_lastSaved = true;
                }
                p.end();
            }
        }
    }
    Lock lock;
    saveFuelLocked();
}

size_t carJson(char* out, size_t n) {
    if (!s_lock || n == 0) return 0;
    Lock lock;
    if (!s_jsonLen || s_jsonLen >= n) return 0;
    memcpy(out, s_json, s_jsonLen + 1);
    return s_jsonLen;
}

void fill(float pricePerL, char* priceOut, size_t n) {
    Lock lock;
    fuelbook::fillUp(&s_book, pricePerL);
    saveFuelLocked();
    if (std::isnan(s_book.pricePerL)) snprintf(priceOut, n, "none");
    else snprintf(priceOut, n, "%.2f", static_cast<double>(s_book.pricePerL));
}

void logLine() {
    carwire::Snapshot s;
    int queued, dropped;
    {
        Lock lock;
        s = s_snap;
    }
    queued = s_log.queued();
    dropped = s_log.dropped();
    Serial.printf("[car] %s %.0f km/h %.0f rpm %.1f V %.0f C | trip %.2f km %.3f L %lus | batt %s eng %s%s | "
                  "range %.0f km | log q%d drop%d | seq %lu\n",
                  linkName(s.link), static_cast<double>(s.kmh), static_cast<double>(s.rpm),
                  static_cast<double>(s.volts), static_cast<double>(s.coolantC), static_cast<double>(s.trip.km),
                  static_cast<double>(s.trip.litres), static_cast<unsigned long>(s.trip.seconds),
                  levelName(s.battery), levelName(s.engine), s.checkReady ? "" : " (checking)",
                  static_cast<double>(s.rangeKm), queued, dropped, static_cast<unsigned long>(s.seq));
}
}  // namespace carstate
