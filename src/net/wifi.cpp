#include "wifi.h"
#include <WiFi.h>
#include "../config.h"
#include "../util/hotspotip.h"
#include "../util/netup.h"
#include "../util/roam.h"
#include "ping/ping_sock.h"

namespace net {
static uint32_t s_lastCheck = 0;
static int s_index = -1;          // network in use (config.h order; 0 = preferred), -1 = none
static int s_lastIndex = -1;      // the one before a drop (two tries when it was the preferred)
static roam::Roam s_roam;
static bool s_roaming = true;     // the companion turns this off: it follows the new deck
static bool s_scanning = false;
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

// Joins network i (tries attempts of CONNECT_TRY_MS each); true when online.
static bool join(int i, int tries) {
    for (int t = 0; t < tries; ++t) {
        Serial.printf("[wifi] trying %s\n", WIFI_NETWORKS[i].ssid);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);   // DHCP: undo a fixed hotspot address
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        const uint32_t start = millis();
        while (millis() - start < CONNECT_TRY_MS) {
            if (isOnline()) {                       // connected AND an address
                Serial.printf("[wifi] connected to %s ip=%s\n",
                              WIFI_NETWORKS[i].ssid, WiFi.localIP().toString().c_str());
                pinHotspotAddress();
                s_index = s_lastIndex = i;
                s_roam.onConnected(i, millis());
                return true;
            }
            delay(200);
        }
        Serial.printf("[wifi] %s failed\n", WIFI_NETWORKS[i].ssid);
        WiFi.disconnect();   // keep the driver up: disconnect(true) deinit/re-inits it each try
    }
    return false;
}

bool connectAny() {
    WiFi.mode(WIFI_STA);
    s_scanning = false;
    s_index = -1;
    for (int i = 0; i < WIFI_NETWORK_COUNT; ++i)
        if (join(i, roam::tries(i, s_lastIndex))) return true;   // the hotspot first
    return false;
}

bool switchNetwork() {
    if (WIFI_NETWORK_COUNT < 2) return false;
    const int next = (s_index < 0 ? 0 : s_index + 1) % WIFI_NETWORK_COUNT;
    Serial.printf("[wifi] switching to %s\n", WIFI_NETWORKS[next].ssid);
    s_scanning = false;
    WiFi.disconnect();
    if (join(next, 1)) return true;
    return connectAny();                     // that one isn't there: anything that is
}

void setRoaming(bool on) { s_roaming = on; }

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

// On a fallback network: a background scan every minute or so (util/roam); once the preferred
// network (the hotspot) has been seen strong enough twice in a row, move back to it.
static void roamStep(uint32_t now) {
    if (!s_roaming || s_index <= 0) return;
    if (!s_scanning) {
        if (!s_roam.wantScan(now)) return;
        s_scanning = WiFi.scanNetworks(true /* async */) == WIFI_SCAN_RUNNING;
        return;
    }
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    s_scanning = false;
    bool seen = false;
    int rssi = -127;
    for (int k = 0; k < n; ++k)
        if (WiFi.SSID(k) == WIFI_NETWORKS[0].ssid && WiFi.RSSI(k) > rssi) {
            seen = true;
            rssi = WiFi.RSSI(k);
        }
    WiFi.scanDelete();
    if (!s_roam.onScan(seen, rssi, now)) return;
    Serial.printf("[wifi] %s is back (%d dBm), moving to it\n", WIFI_NETWORKS[0].ssid, rssi);
    WiFi.disconnect();
    if (!join(0, 1) && connectAny() && s_index > 0) s_roam.onMoveFailed(millis());   // didn't stick
}

void loop() {
    if (millis() - s_lastCheck < CHECK_MS) return;
    s_lastCheck = millis();
    if (!isOnline()) {
        Serial.println("[wifi] dropped, reconnecting...");
        connectAny();
        return;
    }
    roamStep(millis());
}
}
