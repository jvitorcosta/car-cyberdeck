#include "gps.h"
// Before Arduino.h: its newlib <limits.h> defines LINE_MAX as a macro, the name of nmea::Reader's buffer size.
#include "../../util/car/nmea.h"
#include <Arduino.h>
#include <WiFi.h>
#include "../../config.h"
#include "../../common/net/wifi.h"
#include "../core/mem.h"

// GPS 2 IP's "TCP push" defaults on an iPhone hotspot: the phone is the hotspot's gateway. A
// config.h can point them elsewhere (the PC's address for tools/nmea_replay.py on the bench).
#ifndef GPS_HOST
#define GPS_HOST "172.20.10.1"
#endif
#ifndef GPS_PORT
#define GPS_PORT 11123
#endif

namespace gps {

constexpr int32_t CONNECT_TIMEOUT_MS = 1000;   // the phone is one hop away: longer means it's not there
constexpr uint32_t BACKOFF_MIN_MS = 2000;      // a refused connect: retry after this...
constexpr uint32_t BACKOFF_MAX_MS = 30000;     // ...doubling up to this (the app closed for the trip)
constexpr uint32_t QUIET_MS = 5000;            // connected but silent this long: drop and reconnect
constexpr uint32_t TICK_MS = 1000;             // no sentence this long: tell the sink "no fix"
constexpr uint32_t POLL_MS = 50;               // ~20 checks a second: a 1 Hz feed waits at most this

static Sink s_sink = nullptr;

static void task(void*) {
    static WiFiClient client;   // static: off the 4 KB stack
    static nmea::Reader reader;
    bool connected = false, failedOnce = false;
    uint32_t backoff = BACKOFF_MIN_MS, nextTry = 0, lastByte = 0, lastSink = millis();
    for (;;) {
        uint32_t now = millis();
        if (!net::isOnline()) {
            if (connected) { client.stop(); connected = false; Serial.println("[gps] WiFi down"); }
            backoff = BACKOFF_MIN_MS;
            nextTry = now;   // try as soon as WiFi is back
        } else if (!connected && (int32_t)(now - nextTry) >= 0) {
            if (client.connect(GPS_HOST, GPS_PORT, CONNECT_TIMEOUT_MS)) {
                connected = true;
                failedOnce = false;
                backoff = BACKOFF_MIN_MS;
                lastByte = millis();
                Serial.printf("[gps] connected to %s:%d\n", GPS_HOST, GPS_PORT);
                mem::log("gps");   // reports this task's free stack too: the 4 KB budget, on the deck
            } else {
                client.stop();
                // One line per outage, not per retry: the phone may stay away for a whole drive.
                if (!failedOnce) Serial.printf("[gps] no feed at %s:%d, retrying\n", GPS_HOST, GPS_PORT);
                failedOnce = true;
                nextTry = millis() + backoff;
                backoff = backoff * 2 > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : backoff * 2;
            }
        }
        if (connected) {
            int n = 0;
            while (client.available() > 0 && n++ < 512) {   // bounded: the tick below still runs
                const int c = client.read();
                if (c < 0) break;
                lastByte = millis();
                cameras::Fix f;
                if (reader.feed((char)c, lastByte, &f)) {
                    s_sink(f);
                    lastSink = lastByte;
                }
            }
            now = millis();
            if (!client.connected() || now - lastByte > QUIET_MS) {
                // A half-open socket (the phone left the hotspot) never says so: silence is the sign.
                client.stop();
                connected = false;
                nextTry = now;   // straight back: it was working a moment ago
                Serial.println("[gps] feed lost, reconnecting");
            }
        }
        now = millis();
        if (now - lastSink >= TICK_MS) {
            cameras::Fix none{};
            none.valid = false;
            none.ms = now;
            s_sink(none);
            lastSink = now;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void start(Sink sink) {
    if (s_sink || !sink) return;
    s_sink = sink;
    xTaskCreatePinnedToCore(task, "gps", 4096, nullptr, 1, nullptr, 0);
}

}  // namespace gps
