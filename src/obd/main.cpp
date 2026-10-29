// The OBD deck (env obddeck): a third ESP32, no screen, that owns everything that reads the car. Its
// only BLE link is the ELM327 adapter (obdlink); from its readings it keeps the health Watch, this
// power-on's trip, the trip log on its own flash and the fuel book (carstate), and serves them to the
// yellow deck over the phone hotspot:
//   GET /car            the snapshot (util/car/carwire), polled by the yellow deck every second
//   GET /fill[?p=6.49]  a full tank, from the yellow deck's voice fill-up (replies at handleFill)
// Both take a valid signature only (common/net/reqauth, util/wire/sigreq): no ?k= between decks.
// On the hotspot it pins 172.20.10.12 and answers to cardeck-obd.local (common/net/wifi, mDNS).
// Updates over WiFi (obddeck_ota) with the decks' boot guard and rollback (common/net/ota); the task
// watchdog watches the loop and every task (common/core/watchdog).
// Serial: [obd] the link, [car] the trip and a snapshot line every 10 s, [mem] every minute.
// Spec: docs/superpowers/specs/2026-10-28-car-health-check-design.md ("Architecture update")
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <cmath>
#include <cstdlib>
#include "carstate.h"
#include "obdlink.h"
#include "../common/core/watchdog.h"
#include "../common/net/clock.h"
#include "../common/net/ota.h"
#include "../common/net/remote_config.h"
#include "../common/net/reqauth.h"
#include "../common/net/wifi.h"
#include "../util/car/carwire.h"
#include "../util/common/remotegate.h"
#include "../util/common/version.h"

static WebServer* server = nullptr;
static TaskHandle_t netHandle = nullptr;
// Signatures only: the key is never accepted in a URL here (the decks sign; Siri never calls this deck).
static remotegate::Gate gate(REMOTE_TOKEN, remotegate::Accept::SignedOnly);

static void handleCar() {
    if (!reqauth::pass(*server, gate, "/car", "Wrong key")) return;
    static char body[carwire::MAX_JSON];   // the server task's only
    if (!carstate::carJson(body, sizeof body)) {
        server->send(503, "text/plain", "starting");
        return;
    }
    server->send(200, "application/json", body);
}

// p: the price per litre, when the yellow deck heard one ("6.49"); anything that isn't a number
// counts as none said. The full tank is noted in every case; the reply (200, text/plain) says what
// became of the price:
//   "ok 6.49"      the price was taken
//   "ok none"      no price said: the old one stays
//   "refused 649"  a price outside 3.00-15.00 R$/L (what was sent, as sent): the old one stays
static void handleFill() {
    if (!reqauth::pass(*server, gate, "/fill", "Wrong key")) return;
    float price = NAN;
    const String p = server->hasArg("p") ? server->arg("p") : String();
    if (p.length()) {
        char* end = nullptr;
        const float v = strtof(p.c_str(), &end);
        if (end && end != p.c_str() && *end == '\0') price = v;
    }
    char reply[32];
    switch (carstate::fill(price)) {
        case carstate::Fill::Taken: snprintf(reply, sizeof reply, "ok %.2f", static_cast<double>(price)); break;
        case carstate::Fill::NoPrice: snprintf(reply, sizeof reply, "ok none"); break;
        case carstate::Fill::Refused: snprintf(reply, sizeof reply, "refused %.12s", p.c_str()); break;
    }
    Serial.printf("[car] fill-up from %s: %s\n", server->client().remoteIP().toString().c_str(), reply);
    server->send(200, "text/plain", reply);
}

// WiFi, then the server, mDNS, SNTP and the update listener; then the server's loop. Watched: a
// WebServer request waits 5 s at most, a WiFi join feeds before each try.
static void netTask(void*) {
    watchdog::watch();
    net::connectAny();
    while (!net::isOnline()) {
        watchdog::feed();
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    server->begin();
    if (MDNS.begin(OBD_MDNS_NAME)) MDNS.addService("http", "tcp", 80);
    else Serial.println("[net] mDNS failed: reach the deck by its address");
    netclock::begin();  // SNTP: signatures need the clock, trip ids use it
    ota::begin(OBD_MDNS_NAME);
    Serial.printf("[net] on: http://%s/car (http://%s.local/car), signed requests only (heap %u)\n",
                  net::deviceIp().c_str(), OBD_MDNS_NAME, static_cast<unsigned>(ESP.getFreeHeap()));
    for (;;) {
        watchdog::feed();
        server->handleClient();
        net::loop();  // rejoins after a drop
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void beginNet() {
    if (!gate.enabled()) {  // nothing could sign a request: the deck still logs the trips
        Serial.println("[net] off (REMOTE_TOKEN empty)");
        return;
    }
    reqauth::begin();  // HMAC's self-test: /car and /fill only take signed requests
    server = new WebServer(80);
    server->on("/car", HTTP_GET, handleCar);
    server->on("/fill", HTTP_GET, handleFill);
    xTaskCreatePinnedToCore(netTask, "net", 6144, nullptr, 0, &netHandle, 1);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("[boot] obddeck v%s\n", FW_VERSION);
    ota::checkBoot();  // may boot the other app slot (net/ota), before anything else starts
    watchdog::begin();  // 90 s, before any task subscribes
    carstate::begin();  // NVS and the trip log, before the link has anything to say
    obdlink::start();
    beginNet();
    enableLoopWDT();  // the core feeds it before each loop()
}

void loop() {
    const uint32_t now = millis();
    // Up bootguard::HEALTHY_MS, online and the update listener started (when updates are on): this
    // firmware has proven itself (net/ota) and can take the next update. Checked once a second.
    static uint32_t secondAt = 0, checkpointAt = 0, carLineAt = 0, memAt = 0;
    static bool updating = false;
    if (now - secondAt >= 1000) {
        secondAt = now;
        if (net::isOnline()) ota::onGoodPoll();
        carstate::second();
    }
    // An update writes flash and reboots when done: what the log holds goes out once as it starts,
    // and nothing more until the reboot (or a failed one resumes it).
    if (ota::active()) {
        if (!updating) carstate::checkpoint();
        updating = true;
    } else {
        updating = false;
        if (now - checkpointAt >= 30000) {
            checkpointAt = now;
            carstate::checkpoint();
        }
    }
    if (now - carLineAt >= 10000) {
        carLineAt = now;
        carstate::logLine();
    }
    if (now - memAt >= 60000) {
        memAt = now;
        Serial.printf("[mem] heap %u (min %u)  stack left: obd-link %u, obd-ask %u, net %u, ota %u, loop %u\n",
                      static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                      obdlink::linkStackLeft(), obdlink::askStackLeft(),
                      netHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(netHandle)) : 0u,
                      ota::stackLeft(), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    delay(20);
}
