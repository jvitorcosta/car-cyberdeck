// Live mic meter for the black deck (NOT the firmware): is the ES8311 mic really weak, or were we
// reading the wrong channel / with the wrong gain? Shows the left and right channels side by side:
// the level now, the hum floor (quietest of the last 3 s) and how far speech rises above it.
// Tap the screen to change the settings live:
//   top-left: mic PGA gain 0..7 (0..42 dB)   top-right: ADC volume 0 / +12 / +24 dB
//   bottom:   the codec's ALC on/off
// The same numbers go to the serial log every 500 ms.
//   pio run -e micmeter_s3 -t upload --upload-port COM12
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cmath>
#include "../common/board/board.h"
#include "../black/board/s3/mic_es8311.h"
#include "../black/board/s3/touch_ft6336.h"
#include "pins.h"

static TFT_eSPI s_tft;
static constexpr int RATE = 16000;
static constexpr int WINDOW_MS = 100;
static constexpr int HISTORY = 30;   // 3 s of 100 ms windows
static const uint8_t ADC_STEPS[] = {0xBF, 0xD7, 0xEF};   // 0, +12, +24 dB

struct Chan {
    double sum = 0;
    int peak = 0;
    int rmsNow = 0;
    int hist[HISTORY] = {};
};
static Chan s_ch[2];
static int s_n = 0, s_histPos = 0;
static int s_adcStep = 2;

static int floorOf(const Chan& c) {
    int m = 1 << 30;
    for (int v : c.hist) if (v > 0 && v < m) m = v;
    return m == (1 << 30) ? 0 : m;
}

static int topOf(const Chan& c) {
    int m = 0;
    for (int v : c.hist) if (v > m) m = v;
    return m;
}

static void drawSettings() {
    s_tft.fillRect(0, 0, 320, 30, TFT_BLACK);
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char t[48];
    snprintf(t, sizeof t, "ganho %d (+%d dB)", mic::gain(), mic::gain() * 6);
    s_tft.drawString(t, 6, 6, 2);
    snprintf(t, sizeof t, "volume +%d dB", s_adcStep * 12);
    s_tft.drawString(t, 200, 6, 2);
    s_tft.fillRect(0, 214, 320, 26, TFT_BLACK);
    s_tft.drawString(mic::alc() ? "ALC: ligado (toque para desligar)" : "ALC: desligado (toque para ligar)",
                     6, 218, 2);
}

static void drawChannel(int i, const char* name) {
    const Chan& c = s_ch[i];
    const int y = 40 + i * 86;
    const int fl = floorOf(c), top = topOf(c);
    s_tft.fillRect(0, y, 320, 80, TFT_BLACK);
    s_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    s_tft.drawString(name, 6, y, 4);
    const int w = (int)(250.0 * log10(1 + c.rmsNow) / log10(32768.0));   // log scale
    s_tft.fillRect(60, y + 4, w, 18, c.peak > 30000 ? TFT_RED : TFT_GREEN);
    s_tft.drawRect(60, y + 4, 250, 18, TFT_DARKGREY);
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char t[64];
    snprintf(t, sizeof t, "agora %5d  pico %5d", c.rmsNow, c.peak);
    s_tft.drawString(t, 60, y + 30, 2);
    snprintf(t, sizeof t, "chiado %5d  fala %4.1fx", fl, fl ? (double)top / fl : 0.0);
    s_tft.drawString(t, 60, y + 50, 2);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n[micmeter] start");
    board::begin();
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, !AMP_ON);   // speaker amp off: no hiss from it
    s_tft.init();
    s_tft.invertDisplay(true);
    s_tft.setRotation(1);
    s_tft.fillScreen(TFT_BLACK);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_BL, 0);
    ledcWrite(0, 255);
    if (!mic::begin(RATE)) {
        s_tft.drawString("mic falhou", 10, 100, 4);
        return;
    }
    touchft::begin();
    drawSettings();
}

void loop() {
    static int16_t lr[2 * 256];
    static uint32_t lastLog = 0;
    static bool wasDown = false;
    const size_t got = mic::readStereo(lr, 256);
    for (size_t i = 0; i < got; i++) {
        for (int c = 0; c < 2; c++) {
            const int v = lr[2 * i + c];
            s_ch[c].sum += (double)v * v;
            if (abs(v) > s_ch[c].peak) s_ch[c].peak = abs(v);
        }
        s_n++;
    }
    if (s_n >= RATE * WINDOW_MS / 1000) {
        for (int c = 0; c < 2; c++) {
            s_ch[c].rmsNow = (int)sqrt(s_ch[c].sum / s_n);
            s_ch[c].hist[s_histPos] = s_ch[c].rmsNow;
        }
        s_histPos = (s_histPos + 1) % HISTORY;
        drawChannel(0, "E");
        drawChannel(1, "D");
        if (millis() - lastLog > 500) {
            lastLog = millis();
            Serial.printf("[micmeter] gain %d adc +%ddB alc %d | L rms %5d floor %5d top %5d | R rms %5d floor %5d top %5d\n",
                          mic::gain(), s_adcStep * 12, mic::alc(), s_ch[0].rmsNow, floorOf(s_ch[0]),
                          topOf(s_ch[0]), s_ch[1].rmsNow, floorOf(s_ch[1]), topOf(s_ch[1]));
        }
        for (auto& c : s_ch) {
            c.sum = 0;
            c.peak = 0;
        }
        s_n = 0;
    }
    int x, y;
    const bool down = touchft::read(&x, &y);
    if (down && !wasDown) {
        if (y > 200) {
            mic::setAlc(!mic::alc());
        } else if (y < 40 && x < 160) {
            mic::setGain((uint8_t)((mic::gain() + 1) % 8));
        } else if (y < 40) {
            s_adcStep = (s_adcStep + 1) % 3;
            mic::setAdcVolume(ADC_STEPS[s_adcStep]);
        }
        for (auto& c : s_ch) for (int& h : c.hist) h = 0;   // new settings: fresh floor
        drawSettings();
    }
    wasDown = down;
}
