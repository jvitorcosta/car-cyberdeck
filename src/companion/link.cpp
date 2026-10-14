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
static Action s_want = Action::None;          // requested, not yet sent
static Action s_doneAct = Action::None;       // the action the result belongs to
static Result s_result = Result::Idle;

void request(Action a) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_want = a;
    s_doneAct = a;
    s_result = Result::Pending;
    xSemaphoreGive(s_lock);
}

Result takeResult(Action* which) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const Result r = s_result;
    if (which) *which = s_doneAct;
    if (r == Result::Ok || r == Result::Busy || r == Result::Failed) s_result = Result::Idle;
    xSemaphoreGive(s_lock);
    return r;
}

// POST the action; returns the HTTP status (negative: no answer). 2 s timeouts.
static int postAction(Action a) {
    if (!netcfg::linkUp()) return -1;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(2000);
    h.setConnectTimeout(2000);
    char url[128];
    if (a == Action::TripReset)
        snprintf(url, sizeof url, "http://%s/trip/reset?k=%s", COMPANION_DECK, REMOTE_TOKEN);
    else
        snprintf(url, sizeof url, "http://%s/play?clip=%s&k=%s", COMPANION_DECK,
                 a == Action::Howl ? "howl" : "vroom", REMOTE_TOKEN);
    if (!h.begin(c, url)) return -1;
    const int rc = h.POST((uint8_t*)nullptr, 0);
    h.end();
    return rc;
}

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
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const Action a = s_want;
        s_want = Action::None;
        xSemaphoreGive(s_lock);
        if (a != Action::None) {
            const int rc = postAction(a);
            const Result r = (rc >= 200 && rc < 300) ? Result::Ok
                           : (rc == 409 || rc == 429 || rc == 503) ? Result::Busy : Result::Failed;
            Serial.printf("[companion] action %d -> %d\n", (int)a, rc);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (s_want == Action::None && s_doneAct == a) s_result = r;   // not replaced meanwhile
            xSemaphoreGive(s_lock);
        }
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
