#include "obdlink.h"
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // NimBLE 2's headers; src/ builds with -Werror
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#include <NimBLEDevice.h>
#pragma GCC diagnostic pop
#include "../config.h"
#include "../common/core/watchdog.h"
#include "../common/net/ota.h"
#include "../util/car/fueleco.h"
#include "../util/car/obdble.h"
#include "../util/car/obdpick.h"

namespace obdlink {
namespace {
using obd::Pid;
using obdplan::Link;

constexpr uint32_t WAIT_MIN_MS = 5000, WAIT_MAX_MS = 60000;   // the reconnect backoff, doubling
constexpr uint32_t SCAN_MS = 4000;                 // the first-time find
constexpr uint32_t CONNECT_MS = 4000;              // NimBLE 2: milliseconds; the default 30 s stalls every retry
constexpr uint32_t ASK_MS = 2000;                  // one reply's wait (the first PID: 5 s, SEARCHING...)
// The battery (ATRV) at least this often with the engine running: obdplan's slow round reaches it
// only every ~22 s, too slow for the battery check's 60 s hold to see a dip.
constexpr uint32_t BATTERY_EVERY_MS = 4000;
constexpr uint32_t CODES_RETRY_MS = 30000;         // a failed 03 / 07 read is tried again after this
// The City (context: tank 44 L, 1.497 L, VE 0.85); E30 when PID 52 doesn't read 0-100.
constexpr float ETHANOL_DEFAULT = 30.0f;
const fueleco::Car CAR{1.497f, 0.85f, 44.0f, ETHANOL_DEFAULT};

SemaphoreHandle_t s_lock = nullptr;
View s_view{};
TaskHandle_t s_askTask = nullptr, s_linkTask = nullptr;

// Who holds the link. DOWN: the link task (tick) may connect. UP: connected and subscribed; the ask
// task owns s_client / s_tx and the link task leaves them alone. OVER: the ask task is done with it
// and won't touch it again; the link task closes it (only it ever deletes the client).
enum : uint8_t { DOWN, UP, OVER };
std::atomic<uint8_t> s_conn{DOWN};
std::atomic<bool> s_ready{false};                  // this link's adapter answered its setup
NimBLEClient* s_client = nullptr;
NimBLERemoteCharacteristic* s_tx = nullptr;        // commands to the adapter
bool s_txResponse = true;                          // write with response (no write-without)
NimBLEAddress s_addr;                              // the adapter being tried / on the link
char s_name[32] = "";

obdble::Inbox s_inbox;                             // replies, filled by notifications
portMUX_TYPE s_inboxMux = portMUX_INITIALIZER_UNLOCKED;
bool s_late = false;                               // the last ask timed out: its reply may still come
fueleco::Trip s_trip;                              // the whole power-on (one drive), ask task only

void setLink(Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    xSemaphoreGive(s_lock);
}

// p's value now, NAN when it's missing or stale (ask task only: it's the writer, so no lock).
float freshNow(Pid p) { return fresh(s_view, p, millis()); }

// One reply: the raw value, then the fuel it implies, published together.
void publish(Pid p, obd::Reply r, float v, Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    if (r == obd::Reply::Ok) {
        // the City has coolant / intake only as 67 / 68: they stand in for 05 / 0F everywhere
        if (p == Pid::CoolantX) p = Pid::Coolant;
        if (p == Pid::IntakeX) p = Pid::Intake;
        const uint32_t now = millis();
        s_view.value[(int)p] = v;
        s_view.have[(int)p] = true;
        s_view.atMs[(int)p] = now;
        if (p == Pid::Status) s_view.mil = obd::milOn(v);
        if (p == Pid::Maf || p == Pid::Map) {      // one trip step per air reading, fresh values only
            const float rpm = freshNow(Pid::Rpm), speed = freshNow(Pid::Speed);
            const float air = fueleco::airGs(p == Pid::Maf ? v : NAN, freshNow(Pid::Map), rpm,
                                             freshNow(Pid::Intake), CAR);
            // No pedal scaling here (feat/obd's obdlearn isn't ported): fuel cut is never seen, so
            // coasting counts its idle-ish fuel. A small over-count, never an under-count.
            const bool cut = fueleco::fuelCut(NAN, NAN, rpm, speed, freshNow(Pid::FuelSys), false);
            const float eth = freshNow(Pid::Ethanol);
            s_trip.step(now, speed, air, cut, eth >= 0.0f && eth <= 100.0f ? eth : ETHANOL_DEFAULT);
            s_view.litresTotal = s_trip.litres();
            s_view.kmlNow = s_trip.nowKmL();
            if (!std::isnan(air)) s_view.airSteps++;   // NAN air counts nothing (fueleco::Trip::step)
        }
    }
    xSemaphoreGive(s_lock);
}

// ---- the adapter's byte stream ----

void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {   // NimBLE's host task
    portENTER_CRITICAL(&s_inboxMux);
    s_inbox.push(data, len);
    portEXIT_CRITICAL(&s_inboxMux);
}

bool rxPop(char* c) {
    portENTER_CRITICAL(&s_inboxMux);
    const bool got = s_inbox.pop(c);
    portEXIT_CRITICAL(&s_inboxMux);
    return got;
}

void rxClear() {
    portENTER_CRITICAL(&s_inboxMux);
    s_inbox.clear();
    portEXIT_CRITICAL(&s_inboxMux);
}

bool linkUp() { return s_conn.load() == UP && s_client && s_client->isConnected() && s_tx; }

// The only write to the adapter. obd::allowed() gates it, so nothing but mode 01, 03 / 07 and the
// adapter's own setup can ever reach the car.
bool send(const char* cmd) {
    if (!obd::allowed(cmd)) {
        Serial.printf("[obd] BLOCKED %.*s\n", (int)strcspn(cmd ? cmd : "", "\r"), cmd ? cmd : "");
        return false;
    }
    return s_tx->writeValue(reinterpret_cast<const uint8_t*>(cmd), strlen(cmd), s_txResponse);
}

// raw on one line for the log ('\n' as '|'), at most 80 characters.
void logRaw(const char* what, const char* raw) {
    char line[81];
    size_t i = 0;
    for (; raw[i] && i < sizeof line - 1; ++i) line[i] = raw[i] == '\n' ? '|' : raw[i];
    line[i] = '\0';
    Serial.printf("[obd] %.*s -> \"%s\"\n", (int)strcspn(what, "\r"), what, line);
}

// Sends cmd and reads up to the '>' prompt (or the timeout) into out, '\r' as '\n'. False when
// refused, the link is down, or the timeout came with nothing read.
bool ask(const char* cmd, char* out, size_t n, uint32_t timeoutMs) {
    out[0] = '\0';
    if (!linkUp()) return false;
    if (s_late) {                                  // let the late reply finish, and drop it
        s_late = false;                            // (the car test: replies drifted one behind)
        const uint32_t t0 = millis();
        for (char c = 0; c != '>' && millis() - t0 < 2000;)
            if (!rxPop(&c)) delay(2);
    }
    rxClear();                                     // drop anything left over
    if (!send(cmd)) return false;
    size_t len = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        char c;
        if (!rxPop(&c)) {
            delay(2);
            continue;
        }
        if (c == '>') {
            out[len] = '\0';
            return true;
        }
        if (len + 1 < n) out[len++] = c == '\r' ? '\n' : c;
    }
    out[len] = '\0';
    s_late = true;                                 // no prompt yet: resync before the next one
    return len > 0;
}

// ---- finding and connecting (the link task) ----

uint16_t uuid16(const NimBLEUUID& u) {
    return u.bitSize() == 16 ? reinterpret_cast<const ble_uuid16_t*>(u.getBase())->value : 0;
}

#ifndef LED_INSIDE_MAC
#define LED_INSIDE_MAC ""   // an older config.h: no lights to keep away from
#endif
#ifndef LED_OUTSIDE_MAC
#define LED_OUTSIDE_MAC ""
#endif

// One of the car's lights by MAC (config.h; the yellow deck owns them). Their names (ELK / MELK)
// are obdpick's to refuse.
bool lightMac(const NimBLEAddress& a) {
    const std::string s = a.toString();
    return strcasecmp(s.c_str(), LED_INSIDE_MAC) == 0 || strcasecmp(s.c_str(), LED_OUTSIDE_MAC) == 0;
}

obdpick::Recent s_failed;                          // scanned devices that failed (link task)
bool s_fromScan = false;                           // s_addr came from findAdapter, not NVS
char s_advName[32] = "";                           // the scanned device's advertised name ("" if none)

// A scanned device that failed: into the skip ring when obdpick::skipAfter says so (an adapter-like
// name only after a failed setup; a nameless fallback after either). The remembered one never.
void failedFromScan(obdpick::Failure f) {
    if (!s_fromScan || !obdpick::skipAfter(s_advName, f)) return;
    s_failed.failed(static_cast<uint64_t>(s_addr), millis());
    Serial.printf("[obd] %s skipped for %u min\n", s_name, static_cast<unsigned>(obdpick::SKIP_MS / 60000));
}

// A 4 s active scan (names often come in the scan response); logs every named or serial-service
// device seen (never the lights), then obdpick weighs the whole list: an adapter-like name first,
// a nameless serial device only without one, recent failures skipped. The results are freed after
// (the deck's only scan).
bool findAdapter() {
    constexpr int MAX_SEEN = 16;
    static char names[MAX_SEEN][32];               // link task only: off its stack
    static NimBLEAddress addrs[MAX_SEEN];
    obdpick::Candidate cands[MAX_SEEN];
    int n = 0;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    const NimBLEScanResults found = scan->getResults(SCAN_MS, false);
    for (int i = 0; i < found.getCount() && n < MAX_SEEN; ++i) {
        const NimBLEAdvertisedDevice* d = found.getDevice(static_cast<uint32_t>(i));
        if (!d || NimBLEDevice::getClientByPeerAddress(d->getAddress()) != nullptr) continue;
        const std::string nm = d->getName();
        bool serial = false;
        for (int k = 0; k < static_cast<int>(d->getServiceUUIDCount()); ++k)
            serial = serial || obdble::serialService(uuid16(d->getServiceUUID(static_cast<uint8_t>(k))));
        if (nm.empty() && !serial) continue;
        const bool light = lightMac(d->getAddress()) || obdpick::isLight(nm.c_str());
        if (light) continue;                       // never logged, never a candidate
        Serial.printf("[obd] seen \"%s\" %s %d dBm%s\n", nm.c_str(), d->getAddress().toString().c_str(),
                      d->getRSSI(), serial ? " (serial service)" : "");
        strlcpy(names[n], nm.c_str(), sizeof names[n]);
        addrs[n] = d->getAddress();
        cands[n] = {static_cast<uint64_t>(d->getAddress()), names[n], d->getRSSI(), serial, false};
        ++n;
    }
    scan->clearResults();                          // after the last use of `found`'s devices
    const int k = obdpick::pick(cands, n, s_failed, millis());
    if (k < 0) return false;
    s_addr = addrs[k];
    strlcpy(s_advName, names[k], sizeof s_advName);
    strlcpy(s_name, names[k][0] ? names[k] : "OBD (no name)", sizeof s_name);
    return true;
}

// Connects, finds the serial characteristics (logged with their services' UUIDs, 128-bit ones
// included: the Vgate's real layout) and subscribes. Leaves s_client for closeLink on failure.
bool openLink() {
    if (!s_client) {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.printf("[obd] no BLE client left (%u in use)\n", static_cast<unsigned>(NimBLEDevice::getCreatedClientCount()));
            return false;
        }
        s_client = NimBLEDevice::createClient();
        if (!s_client) return false;
        s_client->setConnectTimeout(CONNECT_MS);
    }
    s_tx = nullptr;
    watchdog::feed();                              // the connect: CONNECT_MS
    if (!s_client->connect(s_addr, true)) {
        Serial.printf("[obd] %s: connect failed\n", s_name);
        return false;
    }
    watchdog::feed();                              // the discovery: up to the 30 s ATT timeout
    std::vector<obdble::Char> chars;
    std::vector<NimBLERemoteCharacteristic*> refs;
    uint8_t group = 0;
    for (NimBLERemoteService* svc : s_client->getServices(true)) {
        ++group;                                   // 128-bit services all read as 0: keep them apart
        for (NimBLERemoteCharacteristic* c : svc->getCharacteristics(true)) {
            chars.push_back({uuid16(svc->getUUID()), uuid16(c->getUUID()), c->canWrite(), c->canWriteNoResponse(),
                             c->canNotify(), c->canIndicate(), group});
            refs.push_back(c);
            Serial.printf("[obd] gatt %s/%s%s%s%s%s\n", svc->getUUID().toString().c_str(),
                          c->getUUID().toString().c_str(), c->canWrite() ? " w" : "",
                          c->canWriteNoResponse() ? " wn" : "", c->canNotify() ? " n" : "",
                          c->canIndicate() ? " i" : "");
        }
    }
    const obdble::Pick pk = obdble::pick(chars.data(), static_cast<int>(chars.size()));
    if (pk.write < 0 || pk.notify < 0) {
        Serial.printf("[obd] %s: no serial service\n", s_name);
        return false;
    }
    NimBLERemoteCharacteristic* rx = refs[static_cast<size_t>(pk.notify)];
    if (!rx->subscribe(rx->canNotify(), onNotify)) {
        Serial.printf("[obd] %s: subscribe failed\n", s_name);
        return false;
    }
    s_tx = refs[static_cast<size_t>(pk.write)];
    s_txResponse = !s_tx->canWriteNoResponse();
    return true;
}

// Disconnects, waits until it's done, then drops the client: the next attempt starts fresh (the car
// test: reconnecting while the old link was still closing failed, and the adapter, still
// "connected", stopped advertising).
void closeLink() {
    s_tx = nullptr;
    if (!s_client) return;
    if (s_client->isConnected()) {
        s_client->disconnect();
        for (int i = 0; i < 30 && s_client->isConnected(); ++i) delay(100);
    }
    NimBLEDevice::deleteClient(s_client);
    s_client = nullptr;
}

// The last adapter that answered its setup (NVS "obd"): connected to straight away next time. Keys
// bleaddr / bletype / blename, as the parked yellow-deck build used (feat/obd's classic build left a
// Bluetooth Classic address under "addr" / "name", which isn't a BLE address).
bool remembered() {
    Preferences p;
    if (!p.begin("obd", false)) return false;      // read-write: creates it, no NOT_FOUND log
    if (!p.isKey("bleaddr")) {                     // none yet (getString would log NOT_FOUND)
        p.end();
        return false;
    }
    const String a = p.getString("bleaddr", ""), nm = p.isKey("blename") ? p.getString("blename", "") : String();
    const uint8_t type = p.getUChar("bletype", 0);
    p.end();
    if (a.isEmpty()) return false;
    s_addr = NimBLEAddress(std::string(a.c_str()), type);
    strlcpy(s_name, nm.c_str(), sizeof s_name);
    return true;
}

void remember() {                                  // ask task, after the adapter's setup answered
    Preferences p;
    if (!p.begin("obd", false)) return;
    const String a = s_addr.toString().c_str();
    // no flash write when unchanged (and no NOT_FOUND log for a key not there yet)
    if (!p.isKey("bleaddr") || p.getString("bleaddr", "") != a) p.putString("bleaddr", a);
    if (!p.isKey("bletype") || p.getUChar("bletype", 0xFF) != s_addr.getType()) p.putUChar("bletype", s_addr.getType());
    if (!p.isKey("blename") || p.getString("blename", "") != s_name) p.putString("blename", s_name);
    p.end();
}

// ---- the ask task ----

// The adapter's setup. ATZ resets it, and clones can take seconds to answer the next commands
// after it (the car test: no answer to ATSP0 within 2 s): longer waits, one retry each.
bool setup() {
    static const char* const CMDS[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATH0\r", "ATSP0\r"};
    char raw[64];
    for (const char* c : CMDS) {
        watchdog::feed();
        const uint32_t wait = c[2] == 'Z' ? 4000 : 3000;
        bool ok = ask(c, raw, sizeof raw, wait) && obd::setupOk(c, raw);
        if (!ok) ok = ask(c, raw, sizeof raw, wait) && obd::setupOk(c, raw);
        if (!ok) {
            logRaw(c, raw[0] ? raw : "(no answer)");
            return false;
        }
    }
    return true;
}

// The car's supported mode 01 PIDs (0100, 0120, ... while each block says the next exists), logged
// as one line. False: the car didn't answer the first block.
bool supportScan(uint32_t bits[4]) {
    for (int b = 0; b < 4; ++b) bits[b] = 0;
    char raw[96];
    for (int b = 0; b < 4; ++b) {
        watchdog::feed();
        const bool got = ask(obd::supportRequest(b), raw, sizeof raw, b == 0 ? 5000 : ASK_MS);
        logRaw(obd::supportRequest(b), got ? raw : "(timeout)");
        if (!got || !obd::parseSupported(b, raw, &bits[b])) {
            if (b == 0) return false;
            for (int k = b; k < 4; ++k) bits[k] = 0xFFFFFFFFu;   // a lost block: ask, NO DATA backs off
            break;
        }
        if (!(bits[b] & 1u)) break;                // PID b*20h+20h: is there a next block?
    }
    static char line[400];                         // static: off the ask task's 6 KB stack
    size_t len = 0;
    for (int pid = 1; pid <= 0x80 && len + 4 < sizeof line; ++pid)
        if (pid % 0x20 && obd::isSupported(bits, static_cast<uint8_t>(pid)))
            len += static_cast<size_t>(snprintf(line + len, sizeof line - len, " %02X", pid));
    line[len] = '\0';
    Serial.printf("[obd] supported:%s\n", line);
    return true;
}

// "[obd] codes 1 P0301"
void logCodes(const char* what, const char (*codes)[6], int n) {
    char line[32] = "";
    for (int i = 0; i < n; ++i) {
        strlcat(line, " ", sizeof line);
        strlcat(line, codes[i], sizeof line);
    }
    Serial.printf("[obd] %s %d%s\n", what, n, line);
}

// 03 then 07, read-only. True when both answered: the view gets the counts and the codes.
bool readCodes() {
    char raw[128];
    char stored[4][6], pending[4][6];
    watchdog::feed();
    const int ns = ask(obd::codesRequest(false), raw, sizeof raw, 3000) ? obd::parseCodes(raw, false, stored, 4) : -1;
    if (ns < 0) {
        logRaw(obd::codesRequest(false), raw[0] ? raw : "(timeout)");
        return false;
    }
    watchdog::feed();
    const int np = ask(obd::codesRequest(true), raw, sizeof raw, 3000) ? obd::parseCodes(raw, true, pending, 4) : -1;
    if (np < 0) {
        logRaw(obd::codesRequest(true), raw[0] ? raw : "(timeout)");
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.stored = ns;
    s_view.pending = np;
    int k = 0;
    for (int i = 0; i < ns && k < 4; ++i) strlcpy(s_view.codes[k++], stored[i], sizeof s_view.codes[0]);
    for (int i = 0; i < np && k < 4; ++i) strlcpy(s_view.codes[k++], pending[i], sizeof s_view.codes[0]);
    for (; k < 4; ++k) s_view.codes[k][0] = '\0';
    s_view.codesReads++;
    xSemaphoreGive(s_lock);
    logCodes("codes", stored, ns);
    logCodes("pending", pending, np);
    return true;
}

// One connected adapter, from its setup until it stops answering or the link drops.
void session() {
    s_late = false;
    rxClear();
    if (!setup()) {
        Serial.printf("[obd] %s: no answer to the setup\n", s_name);
        return;
    }
    s_ready.store(true);
    remember();
    Serial.printf("[obd] connected %s\n", s_name);
    obdplan::Plan plan;
    obdplan::Health health;
    uint32_t bits[4];
    // A car that doesn't answer yet (key just turned) is scanned again on its first live reply.
    // setSupported() either way: it also starts the codes' cadence (codesDue).
    bool scanned = supportScan(bits);
    if (!scanned) {
        Serial.println("[obd] supported: unknown (asked again when the car answers)");
        for (uint32_t& b : bits) b = 0xFFFFFFFFu;
    }
    plan.setSupported(bits);
    bool first = true;                             // the first PID may say SEARCHING... for seconds
    uint32_t batteryAt = millis(), codesRetryAt = millis();
    while (linkUp()) {
        watchdog::feed();
        if (ota::active()) {                       // an update has the radio: no traffic meanwhile
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const uint32_t now = millis();
        // The codes only from a car that answers: "NO DATA" from a silent one isn't "no codes".
        if (health.state() == Link::Live && plan.codesDue(now) && static_cast<int32_t>(now - codesRetryAt) >= 0) {
            if (readCodes()) plan.codesDone(now);
            else codesRetryAt = now + CODES_RETRY_MS;
            continue;
        }
        const float rpm = s_view.have[(int)Pid::Rpm] ? s_view.value[(int)Pid::Rpm] : NAN;
        const bool waiting = health.state() == Link::NoEngine || !(rpm > 0.0f);
        plan.crankWatch(waiting);                  // until the engine runs: the battery ~9 in 10
        const Pid p = !waiting && now - batteryAt >= BATTERY_EVERY_MS ? Pid::Battery : plan.next(now);
        if (p == Pid::Battery) batteryAt = now;
        char raw[128];
        const bool got = ask(obd::request(p), raw, sizeof raw, first ? 5000 : ASK_MS);
        first = false;
        float v = 0.0f;
        const obd::Reply r = got ? obd::parse(p, raw, &v) : obd::Reply::Error;
        if (r == obd::Reply::Error) logRaw(obd::request(p), got ? raw : "(timeout)");
        plan.done(p, r, millis());
        const Link l = health.reply(p, r);
        publish(p, r, v, l);
        if (!scanned && l == Link::Live) {         // once
            scanned = true;
            if (supportScan(bits)) plan.setSupported(bits);
        }
        if (health.lost()) {
            Serial.println("[obd] adapter stopped answering");
            break;
        }
        if (l == Link::NoEngine) vTaskDelay(pdMS_TO_TICKS(100));   // no ECU: pace the battery reads
    }
}

void askTask(void*) {
    watchdog::watch();                             // an ask never waits more than ~5 s; setup ~40 s
    for (;;) {
        watchdog::feed();
        if (s_conn.load() != UP) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        session();
        setLink(Link::Off);                        // not Live through the close and the wait
        s_late = false;                            // that link's late reply never comes
        s_conn.store(OVER);                        // the link task closes it
    }
}

// From the link task: find / connect / reconnect with the 5 s..60 s backoff (back to 5 s at once
// when a working adapter drops); skipped while ota::active(). Feeds the watchdog before the
// connect, the discovery and the scan.
void tick(uint32_t nowMs) {
    static uint32_t waitMs = WAIT_MIN_MS, nextMs = 0;
    if (ota::active()) return;
    const uint8_t c = s_conn.load();
    if (c == UP) return;                           // the ask task has it
    if (c == OVER) {
        const bool wasReady = s_ready.exchange(false);
        closeLink();
        s_conn.store(DOWN);
        Serial.printf("[obd] %s: disconnected\n", s_name);
        if (!wasReady) failedFromScan(obdpick::Failure::Setup);   // connected, never answered as an ELM327
        if (wasReady) {                            // a working adapter dropped: back at once
            waitMs = WAIT_MIN_MS;
            nextMs = millis();
        } else {
            nextMs = millis() + waitMs;
            waitMs = waitMs * 2 > WAIT_MAX_MS ? WAIT_MAX_MS : waitMs * 2;
        }
        return;
    }
    if (static_cast<int32_t>(nowMs - nextMs) < 0) return;
    bool up = false;
    s_fromScan = false;                            // the remembered adapter is never skipped
    if (remembered()) {
        setLink(Link::Connecting);
        Serial.printf("[obd] connecting to %s %s (remembered)\n", s_name, s_addr.toString().c_str());
        up = openLink();
        if (!up) closeLink();
    }
    if (!up) {
        setLink(Link::Off);
        watchdog::feed();                          // the scan: SCAN_MS
        if (findAdapter()) {
            s_fromScan = true;
            setLink(Link::Connecting);
            Serial.printf("[obd] connecting to %s %s\n", s_name, s_addr.toString().c_str());
            up = openLink();
            if (!up) {
                closeLink();
                failedFromScan(obdpick::Failure::Connect);
            }
        }
    }
    if (up) {
        s_conn.store(UP);                          // the ask task takes it from here
        return;
    }
    setLink(Link::Off);
    nextMs = millis() + waitMs;
    Serial.printf("[obd] no adapter: next try in %u s\n", static_cast<unsigned>(waitMs / 1000));
    waitMs = waitMs * 2 > WAIT_MAX_MS ? WAIT_MAX_MS : waitMs * 2;
}

void linkTask(void*) {
    watchdog::watch();                             // a connect + discovery: ~35 s at worst
    for (;;) {
        watchdog::feed();
        tick(millis());
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}  // namespace

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.link = Link::Off;
    s_view.stored = s_view.pending = -1;
    s_view.litresTotal = 0.0f;
    s_view.kmlNow = NAN;
    // Central only (platformio.ini turns the peripheral and broadcaster roles off): the deck connects
    // out to the adapter and scans for it, never advertises. NimBLE's default 3 connections; the
    // adapter is the only one used.
    NimBLEDevice::init("");
    NimBLEDevice::setPower(9);                     // +9 dBm: the adapter sits under the dashboard
    Serial.printf("[obd] BLE up, %d connections allowed\n", NIMBLE_MAX_CONNECTIONS);
    // 6 KB each: the link task's connect and discovery (std::vector, std::string, the gatt log
    // lines); the ask task's replies (128-byte buffers) and the supported-PID line (static).
    xTaskCreatePinnedToCore(linkTask, "obd-link", 6144, nullptr, 1, &s_linkTask, 0);
    xTaskCreatePinnedToCore(askTask, "obd-ask", 6144, nullptr, 1, &s_askTask, 0);
}

void snapshot(View& out) {
    if (!s_lock) {
        out = View{};
        out.link = Link::Off;
        out.stored = out.pending = -1;
        out.kmlNow = NAN;
        out.nowMs = millis();
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
    out.nowMs = millis();
}

float fresh(const View& v, Pid p, uint32_t nowMs) {
    return v.have[(int)p] && nowMs - v.atMs[(int)p] < obdplan::staleAfterMs(p) ? v.value[(int)p] : NAN;
}

static unsigned left(TaskHandle_t t) { return t ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(t)) : 0u; }
unsigned linkStackLeft() { return left(s_linkTask); }
unsigned askStackLeft() { return left(s_askTask); }
}  // namespace obdlink
