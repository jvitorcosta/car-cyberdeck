#include "carlink.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>
#include "../common/core/watchdog.h"
#include "../common/net/httpget.h"
#include "../common/net/ipaddr.h"
#include "../common/net/ota.h"
#include "../common/net/remote_config.h"
#include "../common/net/reqauth.h"
#include "../common/net/wifi.h"
#include "../util/common/hotspotip.h"
#include "../util/yellow/deckaddr.h"

namespace carlink {
namespace {

// The wait after a poll, by misses in a row: 1 s while it answers, then 2, 5 and 10 s.
constexpr uint32_t BACKOFF_MS[] = {1000, 2000, 5000, 10000};
constexpr int N_BACKOFF = sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]);
// While it can't be found by name, a lookup (up to 1.5 s) at most this often.
constexpr uint32_t LOOKUP_MS = 30000;

TaskHandle_t handle = nullptr;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Reading pub;                 // guarded by mux
volatile bool fresh = false;  // a snapshot was published since the last take
bool (*mdnsUp)() = nullptr;

deckaddr::Keep addr;  // only the task touches these
uint32_t lookedAt = 0;
bool looked = false;

// Where the OBD deck is: its fixed address on the hotspot (until it stops answering as the deck),
// else its mDNS name. False when it can't be found.
bool host(char* out, size_t n) {
    const bool hot =
        hotspotip::onHotspot(hostOrder(WiFi.localIP()), hostOrder(WiFi.gatewayIP()), hostOrder(WiFi.subnetMask()));
    if (hot && addr.addr() == 0 && addr.tryFixed()) {
        snprintf(out, n, "172.20.10.%d", OBD_HOTSPOT_OCTET);
        return true;
    }
    if (addr.addr() == 0) {
        const uint32_t now = millis();
        if (looked && now - lookedAt < LOOKUP_MS) return false;
        looked = true;
        lookedAt = now;
        if (!mdnsUp()) return false;
        const IPAddress a = MDNS.queryHost(OBD_MDNS_NAME, 1500);
        if (static_cast<uint32_t>(a) == 0) {
            addr.lookupFailed();  // on the hotspot: the fixed address again
            return false;
        }
        addr.found(static_cast<uint32_t>(a));
        Serial.printf("[car] OBD deck found at %s (mDNS)\n", a.toString().c_str());
    }
    snprintf(out, n, "%s", IPAddress(addr.addr()).toString().c_str());
    return true;
}

constexpr int NOT_SIGNED = -1000;  // nothing sent: no clock yet (or HMAC's self-test failed)

// A signed GET /car into body: the HTTP status, negative without an answer, or NOT_SIGNED.
int get(char* body, size_t n) {
    char auth[sigreq::AUTH_LEN];
    if (!reqauth::signGet("/car", nullptr, 0, auth, sizeof(auth))) return NOT_SIGNED;
    char h[40], url[160];
    if (!host(h, sizeof(h))) return -1;
    if (snprintf(url, sizeof(url), "http://%s/car?%s", h, auth) >= static_cast<int>(sizeof(url))) return -1;
    body[0] = 0;
    return net::httpGet(url, 1500, body, n, 3000);
}

void task(void*) {
    static carwire::Snapshot s;  // static: off the task stack
    static char body[carwire::MAX_JSON];
    int misses = 0;
    bool seen = false;  // a snapshot came this power-on
    uint32_t lastSeq = 0, seqAt = 0, nextAt = 0;
    watchdog::watch();  // each pass: one GET of 1.5 s (3 s deadline) and an mDNS query of 1.5 s at most
    for (;;) {
        watchdog::feed();
        const uint32_t now = millis();
        if (!net::isOnline() || ota::active() || static_cast<int32_t>(now - nextAt) < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const int rc = get(body, sizeof(body));
        if (rc == NOT_SIGNED) {  // no clock yet: nothing was asked, so nothing is missed either
            nextAt = now + BACKOFF_MS[0];
            continue;
        }
        const bool ok = rc == 200 && carwire::decode(body, &s);
        addr.onPoll(ok);  // 3 misses in a row: look the name up again
        if (ok) {
            if (misses > 0 || !seen) Serial.printf("[car] OBD deck %s (seq %lu)\n", seen ? "back" : "answering",
                                                   static_cast<unsigned long>(s.seq));
            looked = false;  // a later loss looks the name up at once
            if (!seen || s.seq != lastSeq) seqAt = millis() | 1;
            lastSeq = s.seq;
            seen = true;
            portENTER_CRITICAL(&mux);
            pub.snap = s;
            pub.seqAt = seqAt;
            portEXIT_CRITICAL(&mux);
            fresh = true;
            misses = 0;
        } else {
            if (misses == 0) Serial.printf("[car] no OBD deck (rc=%d%s)\n", rc, rc == 403 ? ": wrong key or clock off" : "");
            ++misses;
            if (misses < N_BACKOFF) Serial.printf("[car] next poll in %lu s\n", static_cast<unsigned long>(BACKOFF_MS[misses] / 1000));
        }
        // answering: a poll a second, start to start; missing: the back-off after this one ends (a
        // lookup or a timeout may have taken 1.5 s of it)
        nextAt = misses == 0 ? now + BACKOFF_MS[0] : millis() + BACKOFF_MS[misses < N_BACKOFF ? misses : N_BACKOFF - 1];
    }
}

}  // namespace

void begin(bool (*up)()) {
    if (handle) return;
    mdnsUp = up;
    // 4.5 KB: the GET and the mDNS query took ~2.3 KB on the deck (the [mem] line shows what's left)
    xTaskCreatePinnedToCore(task, "car", 4608, nullptr, 0, &handle, 1);
}

bool take(Reading* out) {
    if (!fresh) return false;
    fresh = false;  // before the copy: one published meanwhile raises it again
    portENTER_CRITICAL(&mux);
    *out = pub;
    portEXIT_CRITICAL(&mux);
    return true;
}

unsigned stackLeft() { return handle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(handle)) : 0u; }

}  // namespace carlink
