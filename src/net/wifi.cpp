#include "wifi.h"
#include <WiFi.h>
#include "../config.h"
#include "../util/hotspotip.h"
#include "../util/netup.h"
#include "ping/ping_sock.h"

namespace net {
static uint32_t s_lastCheck = 0;
constexpr uint32_t CONNECT_TRY_MS = 8000;   // per network in connectAny()
constexpr uint32_t CHECK_MS = 5000;         // loop(): reconnect check period
// Fixed address on the iPhone hotspot (util/hotspotip): 172.20.10.10 for the S3, .11 for the
// CYD, so a Home Screen bookmark of http://172.20.10.10/ keeps working.
#ifdef BOARD_S3
constexpr uint8_t HOTSPOT_OCTET = 10;
#else
constexpr uint8_t HOTSPOT_OCTET = 11;
#endif

static uint32_t hostOrder(const IPAddress& a) {
    return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3];
}

static void onPingReply(esp_ping_handle_t, void* arg) { *(volatile bool*)arg = true; }

// True when something already answers at `a` (two pings, ~1 s): then we don't take it.
static bool answers(const IPAddress& a) {
    volatile bool replied = false;
    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    IP_ADDR4(&cfg.target_addr, a[0], a[1], a[2], a[3]);
    cfg.count = 2;
    cfg.interval_ms = 300;
    cfg.timeout_ms = 400;
    esp_ping_callbacks_t cbs = {};
    cbs.on_ping_success = onPingReply;
    cbs.cb_args = (void*)&replied;
    esp_ping_handle_t h = nullptr;
    if (esp_ping_new_session(&cfg, &cbs, &h) != ESP_OK) return true;   // can't tell: stay put
    esp_ping_start(h);
    for (int i = 0; i < 12 && !replied; ++i) delay(100);
    esp_ping_stop(h);
    esp_ping_delete_session(h);
    return replied;
}

static void pinHotspotAddress() {
    const uint32_t want = hotspotip::wanted(hostOrder(WiFi.localIP()), hostOrder(WiFi.gatewayIP()),
                                            hostOrder(WiFi.subnetMask()), HOTSPOT_OCTET);
    if (!want) return;
    const IPAddress target(want >> 24, (want >> 16) & 255, (want >> 8) & 255, want & 255);
    if (answers(target)) {
        Serial.printf("[wifi] %s is taken, keeping %s\n", target.toString().c_str(),
                      WiFi.localIP().toString().c_str());
        return;
    }
    if (WiFi.config(target, WiFi.gatewayIP(), WiFi.subnetMask(), WiFi.gatewayIP()))
        Serial.printf("[wifi] fixed hotspot address %s\n", target.toString().c_str());
}

bool connectAny() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        Serial.printf("[wifi] trying %s\n", WIFI_NETWORKS[i].ssid);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);   // DHCP: undo a fixed hotspot address
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        uint32_t start = millis();
        while (millis() - start < CONNECT_TRY_MS) {
            if (isOnline()) {                       // connected AND an address
                Serial.printf("[wifi] connected to %s ip=%s\n",
                              WIFI_NETWORKS[i].ssid, WiFi.localIP().toString().c_str());
                pinHotspotAddress();
                return true;
            }
            delay(200);
        }
        Serial.printf("[wifi] %s failed\n", WIFI_NETWORKS[i].ssid);
        WiFi.disconnect();   // keep the driver up: disconnect(true) deinit/re-inits it each try
    }
    return false;
}

// Connected AND holding an address: after an auto-reconnect the driver can report connected at
// 0.0.0.0 with no DNS (40 s of failed lookups on 2026-10-14); then loop() reconnects cleanly.
bool isOnline() { return netup::usable(WiFi.status() == WL_CONNECTED, (uint32_t)WiFi.localIP()); }
String deviceIp() { return WiFi.localIP().toString(); }
String ssid() { return isOnline() ? WiFi.SSID() : String(); }

void savedNetworks(char* out, size_t n) {
    if (n == 0) return;
    out[0] = '\0';
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        if (i) strlcat(out, ", ", n);
        strlcat(out, WIFI_NETWORKS[i].ssid, n);
    }
}

void loop() {
    if (millis() - s_lastCheck < CHECK_MS) return;
    s_lastCheck = millis();
    if (!isOnline()) {
        Serial.println("[wifi] dropped, reconnecting...");
        connectAny();
    }
}
}
