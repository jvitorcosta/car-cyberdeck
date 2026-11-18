#include "gauges.h"
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <cmath>
#include "../../util/car/benchdrive.h"
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
    l.seq = ms / 250;
    return l;
}
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
obdwire::Seq s_seq;   // under s_mux
}

void begin() {
    Preferences p;
    if (p.begin("hud", false)) {           // read-write: creates it, no NOT_FOUND log
        s_on.store(p.isKey("on") && p.getBool("on", false));
        const uint8_t sc = p.isKey("screen") ? p.getUChar("screen", 0) : 0;
        if (sc < (uint8_t)drivedash::Screen::COUNT) s_screen.store(sc);
        p.end();
    }
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

obdwire::Live live() {
    if (s_demo.load()) return demoLive(millis() - s_demoAt.load());
#ifdef OBD_LINK
    obdlink::View snap;   // ~240 B on the caller's stack (the UI loop, the remote task)
    obdlink::snapshot(snap);
    obdwire::Live l = obdwire::fromReadings(snap.link, snap.value, snap.have, snap.atMs, snap.nowMs);
    l.kml = snap.kmlNow;   // fueleco's, stepped with fresh values only (NAN without air or under 3 km/h)
    portENTER_CRITICAL(&s_mux);
    l.seq = s_seq.update(snap.have, snap.atMs);
    portEXIT_CRITICAL(&s_mux);
    return l;
#else
    return obdwire::Live{};
#endif
}
}  // namespace gauges
