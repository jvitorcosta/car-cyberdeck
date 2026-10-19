#include "obdlink.h"
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <cstring>
#include "../config.h"
#include "../core/mem.h"

#ifndef OBD_PIN
#define OBD_PIN "1234"          // the clones ship with 1234, 0000 or 8888
#endif
// OBD_NAME (optional): the adapter's exact Bluetooth name; unset = the first device whose name
// looks like one (obd::looksLikeAdapter).

namespace obdlink {
static SemaphoreHandle_t s_lock = nullptr;
static View s_view{};
static BluetoothSerial s_bt;
constexpr uint32_t RECONNECT_MS = 30000, NO_ENGINE_MS = 5000, DISCOVER_MS = 10000;

static void setLink(obdplan::Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    xSemaphoreGive(s_lock);
}

static void publish(obd::Pid p, obd::Reply r, float v, obdplan::Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    if (r == obd::Reply::Ok) {
        s_view.value[(int)p] = v;
        s_view.have[(int)p] = true;
        s_view.atMs[(int)p] = millis();
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

static void task(void*) {
    for (;;) {
        setLink(obdplan::Link::Off);
        BTAddress addr;
        char name[32] = "";
        if (!findAdapter(&addr, name, sizeof name)) { delay(RECONNECT_MS); continue; }
        setLink(obdplan::Link::Connecting);
        Serial.printf("[obd] connecting to %s\n", name);
        if (!s_bt.connect(addr) || !init()) {
            Serial.println("[obd] connect failed");
            s_bt.disconnect();
            delay(RECONNECT_MS);
            continue;
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strlcpy(s_view.adapter, name, sizeof s_view.adapter);
        xSemaphoreGive(s_lock);
        mem::log("obd connected");
        obdplan::Plan plan;
        obdplan::Health health;
        bool first = true;                                // the first PID may say SEARCHING... for seconds
        while (s_bt.connected()) {
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
            if (l == obdplan::Link::NoEngine) delay(NO_ENGINE_MS);
        }
        s_bt.disconnect();
        Serial.println("[obd] disconnected");
        delay(2000);
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.link = obdplan::Link::Off;
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
