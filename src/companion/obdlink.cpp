#include "obdlink.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>
#include "../config.h"
#include "../core/mem.h"
#include "../util/fueleco.h"
#include "../util/obdlearn.h"
#ifdef OBD_BLE
#include <NimBLEDevice.h>
#include <esp_bt.h>
#include <vector>
#include "../util/obdble.h"
#else
#include <BluetoothSerial.h>
#endif

// The transport: Bluetooth Classic (SPP, BluetoothSerial) by default; -D OBD_BLE for Bluetooth Low
// Energy (NimBLE, util/obdble). The owner's adapter does both ("OBD2" classic, "OBDBLE" BLE); the car
// test (2026-10-19) found BLE flaky with it, so classic is the default. OBD_NAME (optional): the
// adapter's exact name; unset = the first device whose name looks like one (obd::looksLikeAdapter).
#ifndef OBD_PIN
#define OBD_PIN "1234"          // classic only: the clones ship with 1234, 0000 or 8888
#endif
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
constexpr uint32_t RECONNECT_MS = 30000;
static bool s_late = false;                           // the last ask timed out: its reply may still come
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
        // the City has coolant / intake only as 67 / 68: they stand in for 05 / 0F everywhere
        if (p == Pid::CoolantX) p = Pid::Coolant;
        if (p == Pid::IntakeX) p = Pid::Intake;
        s_view.value[(int)p] = v;
        s_view.have[(int)p] = true;
        s_view.atMs[(int)p] = millis();
        const float rpm = latest(Pid::Rpm), speed = latest(Pid::Speed);
        if (p == Pid::PedalD || p == Pid::PedalE) s_view.pedalPct = s_pedal.scale(v);
        if (p == Pid::Throttle) s_view.throttlePct = s_throttle.scale(v);
        // The engine as known: Unknown until RPM has answered (a battery reading before then may be
        // the charging voltage after a crank the deck missed), Off with no ECU or RPM 0.
        const obdlearn::Engine engine = s_view.have[(int)Pid::Rpm]
                                            ? (rpm > 0.0f ? obdlearn::Engine::Running : obdlearn::Engine::Off)
                                        : l == obdplan::Link::NoEngine ? obdlearn::Engine::Off
                                                                        : obdlearn::Engine::Unknown;
        if (p == Pid::Battery) s_dip.feed(v, engine);
        if (p == Pid::Rpm && v > 0.0f) s_dip.feed(latest(Pid::Battery), engine);   // the start, seen
        s_view.atStart = s_dip.value();
        const bool sysFresh = s_view.have[(int)Pid::FuelSys] &&
                              millis() - s_view.atMs[(int)Pid::FuelSys] < obdplan::STALE_FAST_MS;
        s_view.fuelCut = fueleco::fuelCut(s_view.pedalPct, s_view.throttlePct, rpm, speed,
                                          latest(Pid::FuelSys), sysFresh);
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

// ---- the transport: linkUp, rxPop, rxClear, txSend, findAdapter, remembered/remember,
// openLink, closeLink, transportBegin ----
#ifdef OBD_BLE
using Addr = NimBLEAddress;
constexpr uint32_t SCAN_S = 5;
static NimBLEClient* s_client = nullptr;
static NimBLERemoteCharacteristic* s_tx = nullptr;   // commands to the adapter (write)
static bool s_txResponse = true;                      // write with response (no write-without)
static obdble::Inbox s_inbox;                         // replies, filled by notifications
static portMUX_TYPE s_inboxMux = portMUX_INITIALIZER_UNLOCKED;
static const char* const ADDR_KEY = "ble";

// Notifications from the adapter (NimBLE's host task): appended to the inbox.
static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    portENTER_CRITICAL(&s_inboxMux);
    s_inbox.push(data, len);
    portEXIT_CRITICAL(&s_inboxMux);
}

static bool rxPop(char* c) {
    portENTER_CRITICAL(&s_inboxMux);
    const bool got = s_inbox.pop(c);
    portEXIT_CRITICAL(&s_inboxMux);
    return got;
}

static void rxClear() {
    portENTER_CRITICAL(&s_inboxMux);
    s_inbox.clear();
    portEXIT_CRITICAL(&s_inboxMux);
}

static void txSend(const char* cmd) { s_tx->writeValue((const uint8_t*)cmd, strlen(cmd), s_txResponse); }
static bool linkUp() { return s_client && s_client->isConnected() && s_tx; }
#else
using Addr = BTAddress;
constexpr uint32_t DISCOVER_MS = 10000;
static BluetoothSerial s_bt;
static const char* const ADDR_KEY = "addr";

static bool rxPop(char* c) {
    if (!s_bt.available()) return false;
    *c = (char)s_bt.read();
    return true;
}
static void rxClear() { while (s_bt.available()) s_bt.read(); }
static void txSend(const char* cmd) { s_bt.print(cmd); }
static bool linkUp() { return s_bt.connected(); }
#endif

// raw on one line for the log ('\n' as '|'), at most 80 characters.
static void logRaw(const char* what, const char* raw) {
    char line[81];
    size_t i = 0;
    for (; raw[i] && i < sizeof line - 1; ++i) line[i] = raw[i] == '\n' ? '|' : raw[i];
    line[i] = '\0';
    Serial.printf("[obd] %s -> \"%s\"\n", what, line);
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
    if (!linkUp()) { out[0] = '\0'; return false; }
    if (s_late) {                                         // let the late reply finish, and drop it
        s_late = false;                                   // (the car test: replies drifted one behind)
        const uint32_t t0 = millis();
        for (char c = 0; c != '>' && millis() - t0 < 2000;)
            if (!rxPop(&c)) delay(2);
    }
    rxClear();                                            // drop anything left over
    txSend(cmd);
    size_t len = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        char c;
        if (!rxPop(&c)) { delay(2); continue; }
        if (c == '>') { out[len] = '\0'; return true; }
        if (len + 1 < n) out[len++] = c == '\r' ? '\n' : c;
    }
    out[len] = '\0';
    s_late = true;                                        // no prompt yet: resync before the next one
    return len > 0;
}

#ifdef OBD_BLE
static uint16_t uuid16(const NimBLEUUID& u) {
    return u.bitSize() == 16 ? u.getNative()->u16.value : 0;
}

// Scans for BLE devices and picks the adapter (an adapter-like name, or a clone's serial service
// advertised); logs every named or serial-service device seen.
static bool findAdapter(Addr* addr, char* name, size_t n) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);                            // names often come in the scan response
    NimBLEScanResults found = scan->start(SCAN_S, false);
    bool match = false;
    for (int i = 0; i < found.getCount() && !match; ++i) {
        NimBLEAdvertisedDevice d = found.getDevice(i);
        const std::string nm = d.getName();
        bool serial = false;                              // advertises a clone's serial service
        for (int k = 0; k < (int)d.getServiceUUIDCount(); ++k)
            serial |= obdble::serialService(uuid16(d.getServiceUUID(k)));
        if (nm.empty() && !serial) continue;
        Serial.printf("[obd] seen \"%s\" %s%s\n", nm.c_str(), d.getAddress().toString().c_str(),
                      serial ? " (serial service)" : "");
#ifdef OBD_NAME
        match = nm == OBD_NAME;
#else
        match = obd::looksLikeAdapter(nm.c_str()) || serial;
#endif
        if (match) {
            *addr = d.getAddress();
            strlcpy(name, nm.empty() ? "OBD (no name)" : nm.c_str(), n);
        }
    }
    scan->clearResults();
    return match;
}

static Addr addrFrom(const String& a, uint8_t type) { return NimBLEAddress(std::string(a.c_str()), type); }
static String addrText(const Addr& a) { return a.toString().c_str(); }
static uint8_t addrType(const Addr& a) { return a.getType(); }

// Connects, finds the serial characteristics (logged, for unknown clones) and subscribes.
static bool openLink(const Addr& addr) {
    if (!s_client) {
        s_client = NimBLEDevice::createClient();
        s_client->setConnectTimeout(5);                   // seconds; the default 30 stalls every retry
    }
    s_tx = nullptr;
    if (!s_client->connect(addr)) {
        Serial.println("[obd] ble connect failed");
        return false;
    }
    std::vector<obdble::Char> chars;
    std::vector<NimBLERemoteCharacteristic*> refs;
    std::vector<NimBLERemoteService*>* svcs = s_client->getServices(true);
    for (NimBLERemoteService* svc : *svcs) {
        std::vector<NimBLERemoteCharacteristic*>* cs = svc->getCharacteristics(true);
        for (NimBLERemoteCharacteristic* c : *cs) {
            chars.push_back({uuid16(svc->getUUID()), uuid16(c->getUUID()), c->canWrite(),
                             c->canWriteNoResponse(), c->canNotify(), c->canIndicate()});
            refs.push_back(c);
            Serial.printf("[obd] ble %s/%s%s%s%s%s\n", svc->getUUID().toString().c_str(),
                          c->getUUID().toString().c_str(), c->canWrite() ? " w" : "",
                          c->canWriteNoResponse() ? " wn" : "", c->canNotify() ? " n" : "",
                          c->canIndicate() ? " i" : "");
        }
    }
    const obdble::Pick pk = obdble::pick(chars.data(), (int)chars.size());
    if (pk.write < 0 || pk.notify < 0) {
        Serial.println("[obd] no serial service on this device");
        s_client->disconnect();
        return false;
    }
    NimBLERemoteCharacteristic* rx = refs[pk.notify];
    if (!rx->subscribe(rx->canNotify(), onNotify)) {
        Serial.println("[obd] ble subscribe failed");
        s_client->disconnect();
        return false;
    }
    s_tx = refs[pk.write];
    s_txResponse = !s_tx->canWriteNoResponse();
    return true;
}

// Disconnects and waits until it's done, then drops the client: the next attempt starts fresh
// (the car test: reconnecting while the old link was still closing failed, and the adapter, still
// "connected", stopped advertising).
static void closeLink() {
    s_tx = nullptr;
    if (!s_client) return;
    if (s_client->isConnected()) {
        s_client->disconnect();
        for (int i = 0; i < 30 && s_client->isConnected(); ++i) delay(100);
    }
    NimBLEDevice::deleteClient(s_client);
    s_client = nullptr;
}

static void transportRestart() {}   // BLE: a fresh client per attempt is enough (closeLink)

static bool transportBegin() {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);   // BLE only: classic's RAM back to the heap
    NimBLEDevice::init("cardeck-obd");
    mem::log("ble on");
    return true;
}
#else
// Discovers nearby Bluetooth Classic devices and picks the adapter; logs every name seen.
static bool findAdapter(Addr* addr, char* name, size_t n) {
    BTScanResults* found = s_bt.discover(DISCOVER_MS);
    Serial.printf("[obd] scan: %d device(s)\n", found ? found->getCount() : -1);
    if (!found) return false;
    for (int i = 0; i < found->getCount(); ++i) {
        BTAdvertisedDevice* d = found->getDevice(i);
        const std::string nm = d->getName();
        Serial.printf("[obd] seen \"%s\" %s\n", nm.c_str(), d->getAddress().toString().c_str());
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

static Addr addrFrom(const String& a, uint8_t) { return BTAddress(a); }
static String addrText(const Addr& a) { return a.toString(); }
static uint8_t addrType(const Addr&) { return 0; }

static bool openLink(const Addr& addr) {
    if (s_bt.connect(addr)) return true;
    Serial.println("[obd] bt connect failed");
    return false;
}

// Disconnects and waits until it's done, so the next attempt starts clean.
static void closeLink() {
    s_bt.disconnect();
    for (int i = 0; i < 30 && s_bt.connected(); ++i) delay(100);
}

// Restarts the Bluetooth Classic stack. The car test: after a failed connect, discover() found
// nothing at all (not even devices it saw before), so the deck looped on a dead reconnect forever.
static void transportRestart() {
    s_bt.end();
    delay(500);
    if (!s_bt.begin("cardeck-obd", true) || !s_bt.setPin(OBD_PIN)) Serial.println("[obd] Bluetooth restart failed");
    else Serial.println("[obd] Bluetooth restarted");
}

static bool transportBegin() {
    if (!s_bt.begin("cardeck-obd", true) || !s_bt.setPin(OBD_PIN)) {
        Serial.println("[obd] Bluetooth didn't start: no gauges");
        return false;
    }
    mem::log("bt on");
    return true;
}
#endif

// The last adapter that answered (NVS "obd"): connected to straight away at the next boot.
static bool remembered(Addr* addr, char* name, size_t n) {
    Preferences p;
    if (!p.begin("obd", false)) return false;             // read-write: creates it, no NOT_FOUND
    const String a = p.getString(ADDR_KEY, ""), nm = p.getString("name", "");
    const uint8_t type = p.getUChar("bletype", 0);
    p.end();
    if (a.isEmpty()) return false;
    *addr = addrFrom(a, type);
    strlcpy(name, nm.c_str(), n);
    return true;
}

static void remember(const Addr& addr, const char* name) {
    Preferences p;
    if (!p.begin("obd", false)) return;
    const String a = addrText(addr);
    if (p.getString(ADDR_KEY, "") != a) p.putString(ADDR_KEY, a);   // no flash write when unchanged
    if (p.getUChar("bletype", 0xFF) != addrType(addr)) p.putUChar("bletype", addrType(addr));
    if (p.getString("name", "") != name) p.putString("name", name);
    p.end();
}

// The adapter's setup. ATZ resets it, and the owner's clone can take seconds to answer the next
// commands after it (the car test: no answer to ATSP0 within 2 s): longer waits, one retry each.
static bool init() {
    static const char* const CMDS[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATSP0\r"};
    char raw[64];
    for (const char* c : CMDS) {
        const uint32_t wait = c[2] == 'Z' ? 4000 : 3000;
        bool ok = ask(c, raw, sizeof raw, wait);
        if (!ok) ok = ask(c, raw, sizeof raw, wait);
        if (!ok) {
            logRaw(c, raw[0] ? raw : "(no answer)");
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
        const bool got = ask(obd::supportRequest(b), raw, sizeof raw, b == 0 ? 5000 : 2000);
        logRaw(obd::supportRequest(b), got ? raw : "(timeout)");
        if (!got || !obd::parseSupported(b, raw, &bits[b])) {
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

// Connects to the remembered adapter, else scans for one. False: none answered this round.
static bool connectAdapter(char* name, size_t n) {
    Addr addr;
    if (remembered(&addr, name, n)) {
        setLink(obdplan::Link::Connecting);
        Serial.printf("[obd] connecting to %s (remembered)\n", name);
        if (openLink(addr) && init()) { remember(addr, name); return true; }
        closeLink();
        transportRestart();                               // a failed connect leaves discovery blind
    }
    setLink(obdplan::Link::Off);
    if (!findAdapter(&addr, name, n)) return false;
    setLink(obdplan::Link::Connecting);
    Serial.printf("[obd] connecting to %s\n", name);
    if (openLink(addr) && init()) { remember(addr, name); return true; }
    closeLink();
    transportRestart();
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
        while (linkUp()) {
            // Until the engine runs: the battery ~10 a second (its crank dip) and RPM now and then.
            const float rpm = latest(obd::Pid::Rpm);
            plan.crankWatch(health.state() == obdplan::Link::NoEngine || !(rpm > 0.0f));
            const obd::Pid p = plan.next(millis());
            char raw[128];
            const bool got = ask(obd::request(p), raw, sizeof raw, first ? 5000 : 2000);
            first = false;
            float v = 0.0f;
            const obd::Reply r = got ? obd::parse(p, raw, &v) : obd::Reply::Error;
            if (r == obd::Reply::Error) logRaw(obd::request(p), got ? raw : "(timeout)");
            plan.done(p, r, millis());
            const obdplan::Link l = health.reply(p, r);
            publish(p, r, v, l);
            if (health.lost()) { Serial.println("[obd] adapter stopped answering"); break; }
            if (l == obdplan::Link::NoEngine) delay(100);  // no ECU: pace the battery reads
        }
        closeLink();
        Serial.println("[obd] disconnected");
        delay(2000);
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.link = obdplan::Link::Off;
    s_view.kmlNow = s_view.kmlAvg = s_view.kmLeft = NAN;
    s_view.pedalPct = s_view.throttlePct = s_view.atStart = NAN;
    if (!transportBegin()) return;
    xTaskCreatePinnedToCore(task, "obd", 6144, nullptr, 1, nullptr, 0);
}

void snapshot(View& out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
}
}
