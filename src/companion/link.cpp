#include "link.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../config.h"
#include "../net/wifi.h"
#include "../net/http_config.h"
#include "../util/pollpace.h"
#include "../util/readguard.h"

#ifndef COMPANION_DECK
#define COMPANION_DECK "172.20.10.10"   // the S3's fixed address on the iPhone hotspot
#endif
#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""
#endif

namespace decklink {
static SemaphoreHandle_t s_lock = nullptr;
static View s_view{};
static char s_body[1536];   // /state is < 1 KB; static: off the task stack

void snapshot(View& out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
}

// GET /state into s_body; returns the HTTP status (negative: no answer). Short timeouts: the
// deck is one hop away, and a missing deck must not hold the next poll up.
static int pollState() {
    s_body[0] = '\0';
    if (!netcfg::linkUp()) return -1;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(1500);
    h.setConnectTimeout(1500);
    char url[128];
    snprintf(url, sizeof url, "http://%s/state?k=%s", COMPANION_DECK, REMOTE_TOKEN);
    if (!h.begin(c, url)) return -1;
    const int rc = h.GET();
    size_t len = 0;
    if (rc == 200) {
        WiFiClient* s = h.getStreamPtr();
        readguard::Guard g(millis(), 1500, 3000);
        while (len < sizeof s_body - 1 && (h.connected() || s->available())) {
            int a = s->available();
            if (a <= 0) {
                if (g.check(millis()) != readguard::State::Ok) break;
                delay(1);
                continue;
            }
            size_t want = sizeof s_body - 1 - len;
            if ((size_t)a < want) want = (size_t)a;
            len += s->readBytes(s_body + len, want);
            g.onData(millis());
        }
    }
    s_body[len] = '\0';
    h.end();
    return rc;
}

static void run(void*) {
    pollpace::Pace pace;
    uint32_t lastPoll = 0;
    bool first = true;
    static stateparse::Deck d;   // static: ~600 B, off the task stack
    for (;;) {
        net::loop();
        const uint32_t now = millis();
        if (first || now - lastPoll >= pace.intervalMs()) {
            first = false;
            lastPoll = now;
            const int rc = pollState();
            const bool ok = rc == 200 && stateparse::parse(s_body, d);
            if (pace.misses() == 0 && !ok) Serial.printf("[companion] deck lost (rc=%d)\n", rc);
            if (pace.misses() > 0 && ok) Serial.println("[companion] deck back");
            pace.onResult(ok);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_view.wrongKey = rc == 403;
            s_view.deckOk = ok;
            if (ok) { s_view.deck = d; s_view.polledAtMs = millis(); }
            xSemaphoreGive(s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(run, "link", 6144, nullptr, 1, nullptr, 0);
}
}
