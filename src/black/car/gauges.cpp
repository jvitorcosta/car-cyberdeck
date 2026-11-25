#include "gauges.h"
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include "../../util/car/benchdrive.h"
#include "../../util/car/fuelbook.h"
#include "../../util/car/fuel22.h"
#include "../../util/car/tripbook.h"
#ifdef OBD_LINK
#include "../../obd/obdlink.h"
#endif

namespace gauges {
namespace {
std::atomic<bool> s_on{false}, s_demo{false};
std::atomic<uint8_t> s_screen{(uint8_t)drivedash::Screen::Suki};
std::atomic<uint32_t> s_demoAt{0};

// The simulated drive as the HUD's values; throttle, load and intake made up from its rpm and speed.
obdwire::Live demoLive(uint32_t ms) {
    const benchdrive::Sample b = benchdrive::at(ms);
    obdwire::Live l;
    l.link = b.link;
    l.rpm = b.rpm;
    l.kmh = b.kmh;
    l.coolC = b.coolantC;
    l.volts = b.volts;
    if (b.link == obdplan::Link::Live) {
        l.throttle = std::fmin(100.0f, std::fmax(0.0f, (b.rpm - 750.0f) / 55.0f));
        l.load = std::fmin(100.0f, 18.0f + b.rpm / 90.0f);
        l.intakeC = 34.0f;
        if (b.kmh > 3.0f) l.kml = std::fmax(4.0f, 22.0f - l.load * 0.2f);
    }
    // a made-up trip at 38 km/h and 12.8 km/l, R$ 6.49 a litre, from a 44 L tank's 520 km
    l.tripS = ms / 1000;
    l.tripKm = l.tripS * 38.0f / 3600.0f;
    l.tripKmL = 12.8f;
    l.priceL = 6.49f;
    l.tripRs = l.tripKm / l.tripKmL * l.priceL;
    l.rangeKm = std::fmax(0.0f, 520.0f - l.tripKm);
    l.seq = ms / 250;
    return l;
}
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
obdwire::Seq s_seq;   // under s_mux

// The fuel book, under s_bookLock (a mutex: its NVS write may block); the trip meter, UI loop only.
SemaphoreHandle_t s_bookLock = nullptr;
fuelbook::Book s_book;
uint8_t s_saved[fuelbook::RECORD_BYTES];
bool s_savedOk = false;   // s_saved holds what NVS has
tripbook::Meter s_meter;
std::atomic<uint32_t> s_tripS{0};
bool s_fillWanted = false;   // a full tank asked (fill), taken by tick: under s_mux
bool s_forgetWanted = false;   // forgetBook, taken by tick: under s_mux
float s_fillPrice = NAN;

struct BookLock {
    BookLock() { xSemaphoreTake(s_bookLock, portMAX_DELAY); }
    ~BookLock() { xSemaphoreGive(s_bookLock); }
    BookLock(const BookLock&) = delete;
    BookLock& operator=(const BookLock&) = delete;
};

void saveBookLocked() {   // only when it changed
    uint8_t rec[fuelbook::RECORD_BYTES];
    fuelbook::encode(s_book, rec);
    if (s_savedOk && memcmp(rec, s_saved, sizeof rec) == 0) return;
    Preferences p;
    if (!p.begin("car", false)) return;
    if (p.putBytes("fuel", rec, sizeof rec) == sizeof rec) {
        memcpy(s_saved, rec, sizeof rec);
        s_savedOk = true;
    }
    p.end();
}
}

void begin() {
    Preferences p;
    if (p.begin("hud", false)) {           // read-write: creates it, no NOT_FOUND log
        s_on.store(p.isKey("on") && p.getBool("on", false));
        const uint8_t sc = p.isKey("screen") ? p.getUChar("screen", 0) : 0;
        if (sc < (uint8_t)drivedash::Screen::COUNT) s_screen.store(sc);
        p.end();
    }
    s_bookLock = xSemaphoreCreateMutex();
    if (p.begin("car", false)) {
        uint8_t rec[fuelbook::RECORD_BYTES];
        if (p.isKey("fuel") && p.getBytesLength("fuel") == sizeof rec && p.getBytes("fuel", rec, sizeof rec) == sizeof rec &&
            fuelbook::decode(rec, sizeof rec, &s_book)) {
            memcpy(s_saved, rec, sizeof rec);
            s_savedOk = true;
        }
        p.end();
    }
    Serial.printf("[obd] fuel book %s\n", !s_savedOk ? "empty" : s_book.hasFull ? "loaded" : "loaded, no full tank yet");
#ifdef OBD_LINK
    obdlink::start();
    Serial.printf("[obd] gauges %s, link on (BLE)\n", s_on.load() ? "on" : "off");
#else
    Serial.printf("[obd] gauges %s, no link in this build\n", s_on.load() ? "on" : "off");
#endif
}

bool on() { return s_on.load(); }

drivedash::Screen screen() { return (drivedash::Screen)s_screen.load(); }

bool setScreen(drivedash::Screen s) {
    if ((int)s >= (int)drivedash::Screen::COUNT || !drivedash::canSwitch(live().kmh)) return false;
    if (s_screen.exchange((uint8_t)s) == (uint8_t)s) return true;
    Preferences p;
    if (p.begin("hud", false)) {
        p.putUChar("screen", (uint8_t)s);
        p.end();
    }
    Serial.printf("[obd] gauges screen %s\n", drivedash::name(s));
    return true;
}

void demo() {
    s_demoAt.store(millis());
    s_demo.store(true);
    s_on.store(true);
    Serial.println("[obd] gauges demo: the simulated drive (GAUGES ON / OFF ends it)");
}

bool demoOn() { return s_demo.load(); }

void set(bool on) {
    const bool wasDemo = s_demo.exchange(false);
    if (s_on.exchange(on) == on && !wasDemo) return;
    Preferences p;
    if (p.begin("hud", false)) {
        p.putBool("on", on);
        p.end();
    }
    Serial.printf("[obd] gauges %s\n", on ? "on" : "off");
}

void tick() {
    static uint32_t at = 0;
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    const bool full = s_fillWanted, forget = s_forgetWanted;
    const float fillPrice = s_fillPrice;
    s_fillWanted = s_forgetWanted = false;
    portEXIT_CRITICAL(&s_mux);
    if (forget && s_bookLock) {
        {
            BookLock lock;
            s_book = fuelbook::Book{};
            saveBookLocked();
        }
        Serial.println("[obd] fuel book cleared");
    }
    if (!s_bookLock || (!full && now - at < 250)) return;
    at = now;
#ifdef OBD_LINK
    obdlink::View snap;   // ~240 B on the UI loop's stack
    obdlink::snapshot(snap);
    const tripbook::Totals t{snap.kmTotal, snap.litresTotal, snap.airSteps};
#endif
    {
        BookLock lock;
        bool changed = false;
#ifdef OBD_LINK
        // a full tank: what burnt until now goes to the old tank first
        changed = full ? s_meter.charge(snap.nowMs, t, &s_book)
                       : s_meter.tick(snap.nowMs, snap.link == obdplan::Link::Live, t, &s_book);
#endif
        if (full) {
            fuelbook::fillUp(&s_book, fillPrice);
            changed = true;
        }
        if (changed) saveBookLocked();
    }
#ifdef OBD_LINK
    s_tripS.store(s_meter.seconds());
#endif
    if (!full) return;
    if (std::isnan(fillPrice)) Serial.println("[obd] full tank noted");
    else Serial.printf("[obd] full tank noted, R$ %.2f a litre\n", (double)fillPrice);
}

void fill(float pricePerL) {   // the book changes on the UI loop (tick): no NVS write on the caller's stack
    portENTER_CRITICAL(&s_mux);
    s_fillWanted = true;
    s_fillPrice = pricePerL;
    portEXIT_CRITICAL(&s_mux);
}

void forgetBook() {
    portENTER_CRITICAL(&s_mux);
    s_forgetWanted = true;
    portEXIT_CRITICAL(&s_mux);
}

float price() {
    if (!s_bookLock) return NAN;
    BookLock lock;
    return s_book.pricePerL;
}

bool lamp(int* storedCodes) {
    if (storedCodes) *storedCodes = -1;
#ifdef OBD_LINK
    if (s_demo.load()) return false;
    obdlink::View snap;
    obdlink::snapshot(snap);
    if (storedCodes) *storedCodes = snap.stored;
    return snap.link == obdplan::Link::Live && snap.mil;
#else
    return false;
#endif
}

obdwire::Live live() {
    if (s_demo.load()) return demoLive(millis() - s_demoAt.load());
#ifdef OBD_LINK
    obdlink::View snap;   // ~240 B on the caller's stack (the UI loop, the remote task)
    obdlink::snapshot(snap);
    obdwire::Live l = obdwire::fromReadings(snap.link, snap.value, snap.have, snap.atMs, snap.nowMs);
    l.kml = snap.kmlNow;   // fueleco's, stepped with fresh values only (NAN without air or under 3 km/h)
    l.tripKm = snap.kmTotal;
    l.tripKmL = snap.kmlAvg;
    l.tripS = s_tripS.load();
    float bookKmL = NAN;
    if (s_bookLock) {
        BookLock lock;
        l.rangeKm = fuelbook::rangeKm(s_book);
        l.priceL = s_book.pricePerL;
        l.tripRs = fuelbook::costRs(s_book, snap.litresTotal);
        bookKmL = s_book.recentKmL;
    }
    // The meter ECU's measured level (fuel22) wins over the FULL TANK book while it is fresh: no click needed.
    const float byLevel = fuelbook::rangeFromLevel(fuel22::freshLitres(snap.fuelL, snap.fuelAtMs, snap.nowMs), bookKmL, snap.kmlAvg);
    if (std::isfinite(byLevel)) l.rangeKm = byLevel;
    portENTER_CRITICAL(&s_mux);
    l.seq = s_seq.update(snap.have, snap.atMs);
    portEXIT_CRITICAL(&s_mux);
    return l;
#else
    return obdwire::Live{};
#endif
}
}  // namespace gauges
