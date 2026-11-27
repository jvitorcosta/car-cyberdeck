// Standalone hardware check for the Guition JC4827W543, the 4.3" ESP32-S3 panel (NOT the firmware).
//   pio run -e hwcheck_panel -t upload --upload-port COMx
// Proves: the NV3041A QSPI display and backlight, the time to push a 320x240 frame centred on it,
// GT911 touch, the microSD slot (read only, never formatted), the I2S speaker output, PSRAM and
// flash size, and the biggest internal heap block with the card mounted, WiFi up and a TLS
// connection to Spotify open (no token sent). Pins: moononournation/Dev_Device_Pins, JC4827W543.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <cmath>
#include "../config.h"

namespace {
constexpr int BL = 1;
constexpr int SD_CS_PIN = 10, SD_MOSI_PIN = 11, SD_SCK_PIN = 12, SD_MISO_PIN = 13;
constexpr int SDA_PIN = 8, SCL_PIN = 4, TOUCH_INT_PIN = 3, TOUCH_RST_PIN = 38;
constexpr int I2S_BCLK_PIN = 42, I2S_LRCK_PIN = 2, I2S_DOUT_PIN = 41;
constexpr int W = 480, H = 272, FRAME_W = 320, FRAME_H = 240;
constexpr size_t TLS_BLOCK = 40 * 1024;   // the pass mark: one block this big with all of it running
constexpr int TARGETS[5][2] = {{40, 40}, {440, 40}, {240, 136}, {40, 216}, {440, 216}};   // touch aim
uint16_t touchMaxX = 0, touchMaxY = 0;   // the resolution the touch controller reports in

Arduino_DataBus* bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
// "IPS" off: on (the vendor pin file's value) showed every colour inverted on this panel.
Arduino_GFX* gfx = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, false);
SPIClass sdSpi(HSPI);   // the display holds the other SPI host

bool screenOk = false, sdOk = false, wifiOk = false, tlsOk = false, audioOk = false;
uint32_t frameUs = 0, sdKbS = 0, sdCardMb = 0;
uint8_t touchAddr = 0, touches = 0;
size_t blockBoot = 0, blockSd = 0, blockWifi = 0, blockTls = 0, freeTls = 0;
WiFiClientSecure tls;

size_t internalLargest() { return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); }
size_t internalFree() { return heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); }

void checkScreen() {
    screenOk = gfx->begin(32000000);
    pinMode(BL, OUTPUT);
    digitalWrite(BL, HIGH);
    if (!screenOk) return;
    gfx->fillRect(0, 0, W, 90, RED);
    gfx->fillRect(0, 90, W, 90, GREEN);
    gfx->fillRect(0, 180, W, H - 180, BLUE);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(3);
    gfx->setCursor(8, 8);
    gfx->print("RED (top)");
    gfx->setCursor(8, 98);
    gfx->print("GREEN");
    gfx->setCursor(8, 188);
    gfx->print("BLUE (bottom)");
    delay(2000);
    // The music screens are 320x240: time one centred, from PSRAM, as the deck would push it.
    auto* frame = static_cast<uint16_t*>(heap_caps_malloc(FRAME_W * FRAME_H * 2, MALLOC_CAP_SPIRAM));
    if (!frame) return;
    for (int y = 0; y < FRAME_H; ++y)
        for (int x = 0; x < FRAME_W; ++x)
            // black top left, red to the right, green downwards
            frame[y * FRAME_W + x] = static_cast<uint16_t>(((x * 31 / (FRAME_W - 1)) << 11) |
                                                           ((y * 63 / (FRAME_H - 1)) << 5));
    gfx->fillScreen(BLACK);
    const uint32_t t0 = micros();
    for (int i = 0; i < 20; ++i) gfx->draw16bitRGBBitmap((W - FRAME_W) / 2, (H - FRAME_H) / 2, frame, FRAME_W, FRAME_H);
    frameUs = (micros() - t0) / 20;
    heap_caps_free(frame);
}

bool gtRead(uint16_t reg, uint8_t* out, size_t n) {
    Wire.beginTransmission(touchAddr);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(touchAddr), static_cast<int>(n)) != static_cast<int>(n)) return false;
    for (size_t i = 0; i < n; ++i) out[i] = static_cast<uint8_t>(Wire.read());
    return true;
}

void checkTouch() {
    // GT911 reset with INT held low picks address 0x5D.
    pinMode(TOUCH_RST_PIN, OUTPUT);
    pinMode(TOUCH_INT_PIN, OUTPUT);
    digitalWrite(TOUCH_INT_PIN, LOW);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(11);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(60);
    pinMode(TOUCH_INT_PIN, INPUT);
    Wire.begin(SDA_PIN, SCL_PIN, 400000U);
    for (uint8_t a : {static_cast<uint8_t>(0x5D), static_cast<uint8_t>(0x14)}) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            touchAddr = a;
            break;
        }
    }
    uint8_t res[4];
    if (touchAddr && gtRead(0x8146, res, 4)) {
        touchMaxX = static_cast<uint16_t>(res[0] | (res[1] << 8));
        touchMaxY = static_cast<uint16_t>(res[2] | (res[3] << 8));
    }
}

void drawTargets() {
    for (const auto& t : TARGETS) {
        gfx->drawFastHLine(t[0] - 12, t[1], 25, WHITE);
        gfx->drawFastVLine(t[0], t[1] - 12, 25, WHITE);
        gfx->drawCircle(t[0], t[1], 7, WHITE);
    }
}

// How far a press landed from the closest target.
void logPress(int x, int y) {
    int best = 0;
    long bestD = 0;
    for (int i = 0; i < 5; ++i) {
        const long dx = x - TARGETS[i][0], dy = y - TARGETS[i][1], d = dx * dx + dy * dy;
        if (i == 0 || d < bestD) {
            best = i;
            bestD = d;
        }
    }
    Serial.printf("[check] touch %d,%d target %d off %d,%d\n", x, y, best + 1, x - TARGETS[best][0],
                  y - TARGETS[best][1]);
}

void pollTouch() {
    if (!touchAddr) return;
    uint8_t status = 0;
    if (!gtRead(0x814E, &status, 1) || !(status & 0x80)) return;
    uint8_t p[4];
    if ((status & 0x0F) && gtRead(0x8150, p, 4)) {
        const int x = p[0] | (p[1] << 8), y = p[2] | (p[3] << 8);
        if (screenOk) gfx->fillCircle(x, y, 5, YELLOW);
        if (touches < 255) ++touches;
        logPress(x, y);
    }
    Wire.beginTransmission(touchAddr);   // clear the ready flag
    Wire.write(static_cast<uint8_t>(0x81));
    Wire.write(static_cast<uint8_t>(0x4E));
    Wire.write(static_cast<uint8_t>(0));
    Wire.endTransmission();
}

void checkSd() {
    sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdOk = SD.begin(SD_CS_PIN, sdSpi, 20000000, "/sd", 2, false);
    if (!sdOk) return;
    sdCardMb = static_cast<uint32_t>(SD.cardSize() >> 20);
    // Read speed from the first file of 64 KB or more in the root; nothing is written.
    File root = SD.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (f.isDirectory() || f.size() < 65536) continue;
        static uint8_t buf[4096];
        size_t got = 0;
        const uint32_t t0 = millis();
        while (got < 1048576) {
            const int n = f.read(buf, sizeof buf);
            if (n <= 0) break;
            got += static_cast<size_t>(n);
        }
        const uint32_t ms = millis() - t0;
        if (ms) sdKbS = static_cast<uint32_t>(got / ms);   // bytes per ms ~ KB/s
        break;
    }
}

void beep() {
    if (!audioOk) return;
    static int16_t buf[320];   // 160 stereo samples: 10 ms
    for (int n = 0; n < 60; ++n) {
        for (int i = 0; i < 160; ++i) {
            const auto v = static_cast<int16_t>(sinf(6.2831853f * 1000.0f * static_cast<float>(n * 160 + i) / 16000.0f) * 3000.0f);
            buf[2 * i] = buf[2 * i + 1] = v;
        }
        size_t written = 0;
        i2s_write(I2S_NUM_0, buf, sizeof buf, &written, portMAX_DELAY);
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void checkAudio() {
    i2s_config_t cfg = {};
    cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = 16000;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = 256;
    cfg.tx_desc_auto_clear = true;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_BCLK_PIN;
    pins.ws_io_num = I2S_LRCK_PIN;
    pins.data_out_num = I2S_DOUT_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    audioOk = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) == ESP_OK && i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
}

void checkNetwork() {
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < WIFI_NETWORK_COUNT && !wifiOk; ++i) {
        WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
        const uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(200);
        wifiOk = WiFi.status() == WL_CONNECTED;
        if (!wifiOk) WiFi.disconnect();
    }
    blockWifi = internalLargest();
    if (!wifiOk) return;
    tls.setInsecure();   // a memory check only: nothing is sent
    tlsOk = tls.connect("api.spotify.com", 443);
    blockTls = internalLargest();   // with the connection (and the card) held open
    freeTls = internalFree();
}

void report() {
    Serial.printf("[check] screen %s, 320x240 push %lu us\n", screenOk ? "ok" : "FAIL", static_cast<unsigned long>(frameUs));
    Serial.printf("[check] touch %s (0x%02X) %ux%u, %u presses\n", touchAddr ? "GT911" : "none", touchAddr,
                  touchMaxX, touchMaxY, touches);
    Serial.printf("[check] sd %s, %lu MB, %lu KB/s\n", sdOk ? "ok" : "none", static_cast<unsigned long>(sdCardMb),
                  static_cast<unsigned long>(sdKbS));
    Serial.printf("[check] audio driver %s\n", audioOk ? "ok" : "FAIL");
    Serial.printf("[check] psram %u KB, flash %u MB\n", static_cast<unsigned>(ESP.getPsramSize() / 1024),
                  static_cast<unsigned>(ESP.getFlashChipSize() >> 20));
    Serial.printf("[check] wifi %s, tls %s\n", wifiOk ? "ok" : "FAIL", tlsOk ? "ok" : "FAIL");
    Serial.printf("[check] largest internal block: boot %u, sd %u, wifi %u, tls %u (free %u) -> %s\n",
                  static_cast<unsigned>(blockBoot), static_cast<unsigned>(blockSd), static_cast<unsigned>(blockWifi),
                  static_cast<unsigned>(blockTls), static_cast<unsigned>(freeTls),
                  tlsOk && sdOk && blockTls >= TLS_BLOCK ? "PASS" : "not passed");
    if (!screenOk) return;
    gfx->fillRect(0, H - 16, W, 16, BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);
    gfx->setCursor(4, H - 12);
    gfx->printf("sd %s  touch %s  tls %s  block %u KB  push %lu ms", sdOk ? "ok" : "none", touchAddr ? "ok" : "none",
                tlsOk ? "ok" : "fail", static_cast<unsigned>(blockTls / 1024), static_cast<unsigned long>(frameUs / 1000));
}
}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(1);   // a closed PC port must not stall the board
    delay(1500);
    Serial.println("\n[check] hwcheck_panel start");
    blockBoot = internalLargest();
    checkScreen();
    checkTouch();
    checkSd();
    blockSd = internalLargest();
    checkAudio();
    for (int i = 0; i < 3; ++i) {
        beep();
        delay(300);
    }
    checkNetwork();
    if (screenOk) drawTargets();
}

void loop() {
    static uint32_t last = 0;
    static uint8_t seen = 0;
    pollTouch();
    if (touches != seen) {   // a touch also sounds the speaker again
        seen = touches;
        beep();
    }
    if (millis() - last >= 5000) {
        last = millis();
        report();
    }
    delay(20);
}
