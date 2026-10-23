// Car mic recorder for the voice agent test (NOT the firmware): docs/superpowers/plans/
// 2026-10-23-voice-agent-1-mic-test.md. The screen shows the next phrase; hold BOOT to record
// it (up to 4 s, into PSRAM); tap the screen's left half to redo the previous one, the right
// half to skip. The clips are served over WiFi as WAV for tools/micrec_pull.py or the phone:
//   http://cardeck.local/?k=<REMOTE_TOKEN>   (or the address on screen)
// PSRAM only: the clips are lost at power-off, so pull them before turning the car off.
//   pio run -e micrec_s3 -t upload --upload-port COM12
#include <Arduino.h>
#include <ESPmDNS.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cmath>
#include "../common/board/board.h"
#include "../black/board/s3/mic_es8311.h"
#include "../black/board/s3/touch_ft6336.h"
#include "../config.h"
#include "pins.h"
#include "../util/black/micrec.h"

static constexpr int PIN_BOOT = 0;   // the board's BOOT button, LOW while pressed
static TFT_eSPI s_tft;
static WebServer s_server(80);
static int16_t* s_clips = nullptr;   // SLOTS x MAX_SAMPLES, PSRAM
static int16_t* s_take = nullptr;    // the take in progress: a short tap never touches a slot
static int s_len[micrec::SLOTS] = {};       // samples kept per slot, 0 = empty
static uint8_t s_gainOf[micrec::SLOTS] = {};
static int s_slot = 0;               // next slot to record; SLOTS = full
static bool s_online = false;

// The built-in fonts are ASCII: fold Portuguese accents for the screen (the web page keeps them).
static String ascii(const char* s) {
    String out;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        if (*p == 0xC3 && p[1]) {
            const unsigned char c = *++p;
            const char* from = "\xA0\xA1\xA2\xA3\xA7\xA9\xAA\xAD\xB3\xB4\xB5\xBA\x87\x83\x81\x89\x93";
            const char* to = "aaaaceeiooouCAAEO";
            const char* hit = strchr(from, (char)c);
            out += hit ? to[hit - from] : '?';
        } else {
            out += (char)*p;
        }
    }
    return out;
}

static void drawScreen() {
    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (s_slot >= micrec::SLOTS) {
        s_tft.drawString("cheio - baixe os clips", 10, 100, 4);
    } else {
        char head[64];
        snprintf(head, sizeof head, "Condicao %c", micrec::condition(s_slot));
        s_tft.drawString(head, 10, 8, 4);
        s_tft.drawString(ascii(micrec::conditionText(s_slot)), 10, 40, 2);
        s_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        s_tft.drawString(ascii(micrec::phrase(micrec::phraseOf(s_slot))), 10, 90, 4);
        s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char pos[32];
        snprintf(pos, sizeof pos, "%d/10   %s", micrec::phraseOf(s_slot) + 1,
                 s_len[s_slot] ? "(regravar)" : "");
        s_tft.drawString(pos, 10, 130, 4);
    }
    s_tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    s_tft.drawString("< voltar", 10, 176, 2);
    s_tft.drawString("pular >", 260, 176, 2);
    String addr = s_online ? ("http://" + WiFi.localIP().toString() + "/?k=...") : String("sem WiFi");
    s_tft.drawString(addr, 10, 216, 2);
}

static void drawLevel(int peak, uint32_t ms) {
    s_tft.fillRect(0, 160, 320, 14, TFT_BLACK);
    s_tft.setTextColor(TFT_RED, TFT_BLACK);
    char t[24];
    snprintf(t, sizeof t, "GRAVANDO %.1f s", (double)ms / 1000.0);
    s_tft.drawString(t, 10, 160, 2);
    s_tft.fillRect(150, 162, (int)(160L * peak / 32768), 10, peak > 30000 ? TFT_RED : TFT_GREEN);
}

// Records while BOOT is held, then keeps the take in the current slot if it was long enough.
static void record() {
    const uint32_t t0 = millis();
    int n = 0, peak = 0, framePeak = 0;
    double sum = 0;
    uint32_t lastDraw = 0;
    int16_t chunk[256];
    mic::read(chunk, 256);   // drop what sat in the DMA buffers before the press
    while (digitalRead(PIN_BOOT) == LOW) {
        const size_t got = mic::read(chunk, 256);
        for (size_t i = 0; i < got && n < micrec::MAX_SAMPLES; i++) {
            const int v = chunk[i];
            s_take[n++] = (int16_t)v;
            sum += (double)v * v;
            if (abs(v) > peak) peak = abs(v);
            if (abs(v) > framePeak) framePeak = abs(v);
        }
        if (millis() - lastDraw > 100) {
            drawLevel(framePeak, millis() - t0);
            framePeak = 0;
            lastDraw = millis();
        }
        if (n >= micrec::MAX_SAMPLES) break;   // 4 s: stop here, keep what we have
    }
    const uint32_t held = millis() - t0;
    while (digitalRead(PIN_BOOT) == LOW) delay(10);   // capped take: wait for the release
    int keep = micrec::samplesToKeep(held);
    if (keep > n) keep = n;
    if (keep == 0) {
        drawScreen();
        return;
    }
    memcpy(s_clips + (size_t)s_slot * micrec::MAX_SAMPLES, s_take, keep * sizeof(int16_t));
    s_len[s_slot] = keep;
    s_gainOf[s_slot] = mic::gain();
    char name[8];
    micrec::clipName(s_slot, name, sizeof name);
    Serial.printf("[micrec] %s %.1f s peak %d rms %d\n", name, (double)keep / micrec::RATE, peak,
                  n ? (int)sqrt(sum / n) : 0);
    s_slot++;
    drawScreen();
}

static bool keyOk() {
    if (s_server.arg("k") == REMOTE_TOKEN) return true;
    s_server.send(403, "text/plain", "wrong key");
    return false;
}

static void handleRoot() {
    if (!keyOk()) return;
    String k = s_server.arg("k");
    String h = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
               "<title>micrec</title><body style='font-family:sans-serif'><h3>Clips</h3><ul>";
    for (int i = 0; i < micrec::SLOTS; i++) {
        if (!s_len[i]) continue;
        char name[8];
        micrec::clipName(i, name, sizeof name);
        h += "<li><a href='/clip?i=" + String(i) + "&k=" + k + "'>" + name + "</a> " +
             micrec::phrase(micrec::phraseOf(i)) + " (" + String(s_len[i] / (float)micrec::RATE, 1) + " s)</li>";
    }
    h += "</ul></body>";
    s_server.send(200, "text/html; charset=utf-8", h);
}

static void handleClip() {
    if (!keyOk()) return;
    const int i = s_server.arg("i").toInt();
    if (!s_server.hasArg("i") || i < 0 || i >= micrec::SLOTS || !s_len[i]) {
        s_server.send(404, "text/plain", "no clip");
        return;
    }
    const uint32_t bytes = s_len[i] * sizeof(int16_t);
    uint8_t hdr[44];
    micrec::wavHeader(hdr, bytes, micrec::RATE);
    char name[8];
    micrec::clipName(i, name, sizeof name);
    s_server.sendHeader("Content-Disposition", String("attachment; filename=") + name + ".wav");
    s_server.setContentLength(sizeof hdr + bytes);
    s_server.send(200, "audio/wav", "");
    WiFiClient c = s_server.client();
    c.write(hdr, sizeof hdr);
    const uint8_t* p = (const uint8_t*)(s_clips + (size_t)i * micrec::MAX_SAMPLES);
    for (uint32_t off = 0; off < bytes; off += 4096) {
        const uint32_t n = min<uint32_t>(4096, bytes - off);
        if (c.write(p + off, n) != n) {   // the phone or PC went away: the pull tool retries
            Serial.printf("[micrec] %s cut at %u of %u bytes\n", name, (unsigned)off, (unsigned)bytes);
            return;
        }
    }
}

static void handleManifest() {
    if (!keyOk()) return;
    String j = "[";
    for (int i = 0; i < micrec::SLOTS; i++) {
        if (!s_len[i]) continue;
        char name[8];
        micrec::clipName(i, name, sizeof name);
        if (j.length() > 1) j += ",";
        j += "{\"slot\":" + String(i) + ",\"name\":\"" + name + "\",\"condition\":\"" +
             micrec::condition(i) + "\",\"phrase\":" + String(micrec::phraseOf(i)) + ",\"ms\":" +
             String(s_len[i] * 1000L / micrec::RATE) + ",\"gain\":" + String(s_gainOf[i]) + "}";
    }
    j += "]";
    s_server.send(200, "application/json", j);
}

static void handleGain() {
    if (!keyOk()) return;
    const int g = s_server.arg("g").toInt();
    if (!s_server.hasArg("g") || g < 0 || g > 7) {
        s_server.send(400, "text/plain", "g = 0..7");
        return;
    }
    mic::setGain((uint8_t)g);
    Serial.printf("[micrec] mic gain %d\n", g);
    s_server.send(204);
}

// Starts serving once WiFi is up. The deck may boot before the hotspot does, and a reboot would
// wipe the clips, so loop() keeps retrying in the background (WiFi.begin doesn't block).
static void startServing() {
    s_online = true;
    MDNS.begin("cardeck");
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/clip", HTTP_GET, handleClip);
    s_server.on("/manifest.json", HTTP_GET, handleManifest);
    s_server.on("/gain", HTTP_POST, handleGain);
    s_server.begin();
    Serial.printf("[micrec] serving at http://%s/?k=...\n", WiFi.localIP().toString().c_str());
    drawScreen();
}

static void wifiLoop() {
    static uint32_t lastTry = 0;
    static int net = -1;
    if (s_online || WiFi.status() == WL_CONNECTED) {
        if (!s_online) startServing();
        return;
    }
    if (net >= 0 && millis() - lastTry < 8000) return;   // give each network 8 s
    net = (net + 1) % WIFI_NETWORK_COUNT;
    lastTry = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_NETWORKS[net].ssid, WIFI_NETWORKS[net].pass);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n[micrec] start");
    board::begin();
    pinMode(PIN_BOOT, INPUT_PULLUP);
    s_tft.init();
    s_tft.invertDisplay(true);
    s_tft.setRotation(1);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_BL, 0);
    ledcWrite(0, 255);
    s_clips = (int16_t*)ps_malloc((size_t)micrec::SLOTS * micrec::MAX_SAMPLES * sizeof(int16_t));
    s_take = (int16_t*)ps_malloc((size_t)micrec::MAX_SAMPLES * sizeof(int16_t));
    Serial.printf("[micrec] PSRAM buffer %s (%u KB)\n", s_clips && s_take ? "ok" : "FAILED",
                  (unsigned)(micrec::SLOTS * micrec::MAX_SAMPLES * 2 / 1024));
    if (!s_clips || !s_take || !mic::begin(micrec::RATE)) {
        s_tft.fillScreen(TFT_BLACK);
        s_tft.drawString("mic ou PSRAM falhou", 10, 100, 4);
        return;
    }
    touchft::begin();
    WiFi.mode(WIFI_STA);
    drawScreen();
}

void loop() {
    if (!s_clips) {
        delay(1000);
        return;
    }
    wifiLoop();
    if (s_online) s_server.handleClient();
    if (digitalRead(PIN_BOOT) == LOW && s_slot < micrec::SLOTS) {
        delay(20);   // debounce
        if (digitalRead(PIN_BOOT) == LOW) record();
    }
    static bool wasDown = false;
    int x, y;
    const bool down = touchft::read(&x, &y);
    if (down && !wasDown) {
        if (x < 160 && s_slot > 0) s_slot--;
        else if (x >= 160 && s_slot < micrec::SLOTS) s_slot++;
        drawScreen();
    }
    wasDown = down;
    delay(5);
}
