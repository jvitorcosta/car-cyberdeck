#include "obdlink.h"
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>
#include "../config.h"
#include "../core/mem.h"
#include "../util/fueleco.h"
#include "../util/obdlearn.h"

#ifndef OBD_PIN
#define OBD_PIN "1234"          // the clones ship with 1234, 0000 or 8888
#endif
// OBD_NAME (optional): the adapter's exact Bluetooth name; unset = the first device whose name
// looks like one (obd::looksLikeAdapter).
// The car, for km/l and km left (spec 2026-10-19-gauge-cluster): the Honda City 1.5 L flex, its
// 44 L tank, a volumetric efficiency for speed-density, and the ethanol % when the car doesn't
// report it (Brazilian pump gasoline: E30).
#ifndef OBD_TANK_L
#define OBD_TANK_L 44
#endif
#ifndef OBD_DISPLACEMENT_CC
#define OBD_DISPLACEMENT_CC 1497
#endif
#ifndef OBD_VE
#define OBD_VE 0.85
#endif
#ifndef OBD_ETHANOL_PCT
#define OBD_ETHANOL_PCT 30
#endif

namespace obdlink {
static SemaphoreHandle_t s_lock = nullptr;
static View s_view{};
static BluetoothSerial s_bt;
constexpr uint32_t RECONNECT_MS = 30000, DISCOVER_MS = 10000;
static const fueleco::Car CAR{OBD_DISPLACEMENT_CC / 1000.0f, (float)OBD_VE, (float)OBD_TANK_L,
                              (float)OBD_ETHANOL_PCT};

// What the cluster learns, kept for the whole power-on (the deck is powered with the ignition, so
// that's one drive), across Bluetooth reconnects. Only the OBD task touches them.
static fueleco::Trip s_trip;
static obdlearn::Range s_pedal(16, 80), s_throttle(12, 80);
static obdlearn::StartDip s_dip;

static void setLink(obdplan::Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    xSemaphoreGive(s_lock);
}

// The latest value of p in the published view, NAN without one (OBD task only: it's the writer).
static float latest(obd::Pid p) { return s_view.have[(int)p] ? s_view.value[(int)p] : NAN; }

// One reply: the raw value, then everything derived from it, published together.
static void publish(obd::Pid p, obd::Reply r, float v, obdplan::Link l) {
    using obd::Pid;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    if (r == obd::Reply::Ok) {
        s_view.value[(int)p] = v;
        s_view.have[(int)p] = true;
        s_view.atMs[(int)p] = millis();
        const float rpm = latest(Pid::Rpm), speed = latest(Pid::Speed);
        if (p == Pid::PedalD || p == Pid::PedalE) s_view.pedalPct = s_pedal.scale(v);
        if (p == Pid::Throttle) s_view.throttlePct = s_throttle.scale(v);
        if (p == Pid::Battery) s_dip.feed(v, rpm > 0.0f);
        if (p == Pid::Rpm && v > 0.0f) s_dip.feed(latest(Pid::Battery), true);   // the start, seen
        s_view.atStart = s_dip.value();
        if (s_view.have[(int)Pid::FuelSys])
            s_view.fuelCut = ((int)s_view.value[(int)Pid::FuelSys] & 4) != 0;
        else
            s_view.fuelCut = s_view.pedalPct < 2.0f && rpm > 1200.0f && speed > 20.0f;
        if (p == Pid::Maf || p == Pid::Map) {             // one trip step per air reading
            const float air = fueleco::airGs(p == Pid::Maf ? v : NAN, latest(Pid::Map), rpm,
                                             latest(Pid::Intake), CAR);
            const float eth = s_view.have[(int)Pid::Ethanol] ? s_view.value[(int)Pid::Ethanol]
                                                             : (float)OBD_ETHANOL_PCT;
            s_trip.step(millis(), speed, air, s_view.fuelCut, eth);
            s_view.kmlNow = s_trip.nowKmL();
            s_view.kmlAvg = s_trip.avgKmL();
        }
        s_view.kmLeft = s_trip.kmLeft(latest(Pid::Fuel), CAR);
    }
    xSemaphoreGive(s_lock);
}

// Sends cmd and reads up to the '>' prompt (or the timeout) into out, '\r' as '\n'. False on
// timeout with nothing read. The only write to the adapter: obd::allowed() gates it, so nothing
// but mode 01 and the adapter's own setup can ever reach the car.
static bool ask(const char* cmd, char* out, size_t n, uint32_t timeoutMs) {
    if (!obd::allowed(cmd)) {
        Serial.printf("[obd] BLOCKED %.*s\n", (int)strcspn(cmd ? cmd : "", "\r"), cmd ? cmd : "");
        out[0] = '\0';
        return false;
    }
    while (s_bt.available()) s_bt.read();                 // drop anything left over
    s_bt.print(cmd);
    size_t len = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (!s_bt.available()) { delay(2); continue; }
        const int c = s_bt.read();
        if (c == '>') { out[len] = '\0'; return true; }
        if (len + 1 < n) out[len++] = c == '\r' ? '\n' : (char)c;
    }
    out[len] = '\0';
    return len > 0;
}

// Discovers nearby Bluetooth devices and picks the adapter; logs every name seen.
static bool findAdapter(BTAddress* addr, char* name, size_t n) {
    BTScanResults* found = s_bt.discover(DISCOVER_MS);
    if (!found) return false;
    for (int i = 0; i < found->getCount(); ++i) {
        BTAdvertisedDevice* d = found->getDevice(i);
        const std::string nm = d->getName();
        Serial.printf("[obd] seen \"%s\"\n", nm.c_str());
#ifdef OBD_NAME
        const bool match = nm == OBD_NAME;
#else
        const bool match = obd::looksLikeAdapter(nm.c_str());
#endif
        if (match) {
            *addr = d->getAddress();
            strlcpy(name, nm.c_str(), n);
            return true;
        }
    }
    return false;
}

// The last adapter that answered (NVS "obd"): connected to straight away at the next boot.
static bool remembered(BTAddress* addr, char* name, size_t n) {
    Preferences p;
    if (!p.begin("obd", true)) return false;
    const String a = p.getString("addr", ""), nm = p.getString("name", "");
    p.end();
    if (a.isEmpty()) return false;
    *addr = BTAddress(a);
    strlcpy(name, nm.c_str(), n);
    return true;
}

static void remember(const BTAddress& addr, const char* name) {
    Preferences p;
    if (!p.begin("obd", false)) return;
    const String a = addr.toString();
    if (p.getString("addr", "") != a) p.putString("addr", a);   // no flash write when unchanged
    if (p.getString("name", "") != name) p.putString("name", name);
    p.end();
}

static bool init() {
    static const char* const CMDS[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATSP0\r"};
    char raw[64];
    for (const char* c : CMDS) {
        if (!ask(c, raw, sizeof raw, 2000)) {
            Serial.printf("[obd] no answer to %.4s\n", c);
            return false;
        }
    }
    Serial.println("[obd] adapter ready");
    return true;
}

// The car's supported mode 01 PIDs (0100, 0120, ... while each block says the next exists), logged
// as one line. False: the car didn't answer the first block (the plan keeps "all supported").
static bool scan(uint32_t bits[4]) {
    for (int b = 0; b < 4; ++b) bits[b] = 0;
    char raw[96];
    for (int b = 0; b < 4; ++b) {
        if (!ask(obd::supportRequest(b), raw, sizeof raw, b == 0 ? 5000 : 1000) ||
            !obd::parseSupported(b, raw, &bits[b])) {
            if (b == 0) return false;
            for (int k = b; k < 4; ++k) bits[k] = 0xFFFFFFFFu;   // a lost block: ask, NO DATA backs off
            break;
        }
        if (!(bits[b] & 1u)) break;                      // PID b*20h+20h: is there a next block?
    }
    char line[400];
    size_t len = 0;
    for (int pid = 1; pid <= 0x80 && len + 4 < sizeof line; ++pid)
        if (pid % 0x20 && obd::isSupported(bits, (uint8_t)pid)) len += snprintf(line + len, sizeof line - len, " %02X", pid);
    line[len] = '\0';
    Serial.printf("[obd] supported:%s\n", line);
    return true;
}

// Connects to the remembered adapter, else discovers one. False: none answered this round.
static bool connectAdapter(char* name, size_t n) {
    BTAddress addr;
    if (remembered(&addr, name, n)) {
        setLink(obdplan::Link::Connecting);
        Serial.printf("[obd] connecting to %s (remembered)\n", name);
        if (s_bt.connect(addr) && init()) { remember(addr, name); return true; }
        s_bt.disconnect();
    }
    setLink(obdplan::Link::Off);
    if (!findAdapter(&addr, name, n)) return false;
    setLink(obdplan::Link::Connecting);
    Serial.printf("[obd] connecting to %s\n", name);
    if (s_bt.connect(addr) && init()) { remember(addr, name); return true; }
    s_bt.disconnect();
    Serial.println("[obd] connect failed");
    return false;
}

static void task(void*) {
    for (;;) {
        char name[32] = "";
        if (!connectAdapter(name, sizeof name)) { delay(RECONNECT_MS); continue; }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strlcpy(s_view.adapter, name, sizeof s_view.adapter);
        xSemaphoreGive(s_lock);
        mem::log("obd connected");
        obdplan::Plan plan;
        obdplan::Health health;
        uint32_t bits[4];
        if (scan(bits)) {
            plan.setSupported(bits);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            memcpy(s_view.supported, bits, sizeof bits);
            xSemaphoreGive(s_lock);
        } else {
            Serial.println("[obd] supported: unknown");
        }
        bool first = true;                                // the first PID may say SEARCHING... for seconds
        while (s_bt.connected()) {
            // Until the engine runs: the battery ~10 a second (its crank dip) and RPM now and then.
            const float rpm = latest(obd::Pid::Rpm);
            plan.crankWatch(health.state() == obdplan::Link::NoEngine || !(rpm > 0.0f));
            const obd::Pid p = plan.next(millis());
            char raw[128];
            const bool got = ask(obd::request(p), raw, sizeof raw, first ? 5000 : 1000);
            first = false;
            float v = 0.0f;
            const obd::Reply r = got ? obd::parse(p, raw, &v) : obd::Reply::Error;
            plan.done(p, r, millis());
            const obdplan::Link l = health.reply(p, r);
            publish(p, r, v, l);
            if (health.lost()) { Serial.println("[obd] adapter stopped answering"); break; }
            if (l == obdplan::Link::NoEngine) delay(100);  // no ECU: pace the battery reads
        }
        s_bt.disconnect();
        Serial.println("[obd] disconnected");
        delay(2000);
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.link = obdplan::Link::Off;
    s_view.kmlNow = s_view.kmlAvg = s_view.kmLeft = NAN;
    s_view.pedalPct = s_view.throttlePct = s_view.atStart = NAN;
    if (!s_bt.begin("cardeck-obd", true) || !s_bt.setPin(OBD_PIN)) {
        Serial.println("[obd] Bluetooth didn't start: no gauges");
        return;
    }
    mem::log("bt on");
    xTaskCreatePinnedToCore(task, "obd", 6144, nullptr, 1, nullptr, 0);
}

void snapshot(View& out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
}
}
