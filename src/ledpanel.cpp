// Standalone LED panel for the car's two ELK-family controllers on the yellow deck (CYD). Not the
// firmware; a first step before LED control becomes a deck feature. Layout B:
//   header: painted in what the lights show now (colour name, zone, intensity) + power button
//   zones:  BOTH / INSIDE / OUTSIDE (dot: that controller's link, green = connected)
//   strip:  red, orange, pink, purple, blue, green, white
//   bottom: intensity  -  [10 rising bars, tap or drag]  +
// The iPhone's Bluetooth must be off (Settings): each controller takes one connection at a time.
// Colour, intensity, power and zone survive a reboot (NVS). Touch is the pen on a resistive panel;
// its calibration screen runs on first boot, or with BOOT held (at power-up, or 2 s any time).
//   pio run -e ledpanel -t upload
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "companion/touch.h"
#include "util/elkcmd.h"

static TFT_eSPI tft;
static Preferences prefs;

// ---- State ----

struct Swatch { const char* name; uint8_t r, g, b; };
// LED strips run green and blue hot, so orange, pink and purple lean on red.
static const Swatch SWATCHES[] = {
    {"Red", 255, 0, 0},   {"Orange", 255, 60, 0}, {"Pink", 255, 20, 90}, {"Purple", 150, 0, 255},
    {"Blue", 0, 0, 255},  {"Green", 0, 255, 0},   {"White", 255, 255, 255},
};
static constexpr int N_SWATCH = sizeof(SWATCHES) / sizeof(SWATCHES[0]);

enum Target { BOTH, INSIDE, OUTSIDE };
static Target target = BOTH;
static int swatch = N_SWATCH - 1;  // white
static uint8_t intensity = 100;    // 5..100 %
static bool lit = true;

static constexpr uint8_t MIN_INTENSITY = 5;

// ---- BLE ----

struct Controller {
    const char* label;
    const char* addr;
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* volatile tx = nullptr;  // set by the link task, read by the loop
    volatile bool fresh = false;                         // just connected: needs the current state
};

// Which is inside and which outside is a guess from signal strength; swap the addresses if wrong.
static Controller controllers[] = {
    {"INSIDE", "be:16:b6:00:9b:ff"},   // MELK-OA25PQ
    {"OUTSIDE", "be:68:93:1d:22:00"},  // ELK-BLEDDM F8
};
static constexpr int N_CTRL = sizeof(controllers) / sizeof(controllers[0]);

static bool linkUp(const Controller& c) { return c.client && c.client->isConnected() && c.tx; }

static void write(Controller& c, const elkcmd::Frame& f) {
    if (linkUp(c)) c.tx->writeValue(f.b, sizeof(f.b), false);
}

// The current state (power + dimmed colour) to one controller, both header variants.
static void push(Controller& c) {
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(c, elkcmd::power(v, lit));
    if (!lit) return;
    const Swatch& s = SWATCHES[swatch];
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) {
        write(c, elkcmd::color(v, elkcmd::dim(s.r, intensity), elkcmd::dim(s.g, intensity),
                               elkcmd::dim(s.b, intensity)));
    }
}

static bool targeted(int i) { return target == BOTH || target == static_cast<Target>(i + 1); }

static void pushTargets() {
    for (int i = 0; i < N_CTRL; i++) {
        if (targeted(i)) push(controllers[i]);
    }
}

static bool connect(Controller& c) {
    if (!c.client) {
        c.client = NimBLEDevice::createClient();
        c.client->setConnectTimeout(4);
    }
    c.tx = nullptr;
    if (!c.client->connect(NimBLEAddress(std::string(c.addr), BLE_ADDR_PUBLIC))) {
        Serial.printf("[leds] %s: connect failed\n", c.label);
        return false;
    }
    NimBLERemoteService* svc = c.client->getService(NimBLEUUID(static_cast<uint16_t>(0xFFF0)));
    if (svc) c.tx = svc->getCharacteristic(NimBLEUUID(static_cast<uint16_t>(0xFFF3)));
    Serial.printf("[leds] %s: %s\n", c.label, c.tx ? "connected" : "no FFF3");
    c.fresh = c.tx != nullptr;
    return c.fresh;
}

// Connecting blocks up to 4 s per controller, so it runs here on core 0 and the touch loop
// (core 1) never waits. Retries a missing controller every 5 s.
static void linkTask(void*) {
    for (;;) {
        for (Controller& c : controllers) {
            if (!linkUp(c)) connect(c);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---- Screen (320x240 landscape) ----

static constexpr int HEAD_H = 72;                           // y 0..71
static constexpr int PWR_X = 280, PWR_Y = 36, PWR_R = 28;   // power button, in the header
static constexpr int ZONE_Y = 76, ZONE_H = 26;              // y 76..101
static constexpr int STRIP_Y = 108, STRIP_H = 64;           // y 108..171
static constexpr int SW_X0 = 4, SW_STEP = 45, SW_W = 42;
static constexpr int BOT_Y = 182, BOT_H = 54;               // y 182..235
static constexpr int MINUS_X = 4, PLUS_X = 260, STEP_W = 56;
static constexpr int BARS_X = 66, BAR_STEP = 19, BAR_W = 15;

// Pressed feedback: whatever is under the finger turns yellow (or gets a yellow ring) until it is
// released, so a press that registered is always visible.
static constexpr uint16_t PRESSED = TFT_YELLOW;

static uint16_t shown565(const Swatch& s) {  // what the lights show: the colour at this intensity
    return tft.color565(elkcmd::dim(s.r, intensity), elkcmd::dim(s.g, intensity), elkcmd::dim(s.b, intensity));
}

static bool bright(uint8_t r, uint8_t g, uint8_t b) { return (r * 3 + g * 6 + b) / 10 > 140; }

static void drawPower(bool pressed = false) {
    const Swatch& s = SWATCHES[swatch];
    const uint16_t head = lit ? shown565(s) : TFT_BLACK;
    const uint16_t bg = pressed ? PRESSED : lit ? TFT_BLACK : TFT_RED;
    const uint16_t fg = pressed ? TFT_BLACK : TFT_WHITE;
    tft.fillCircle(PWR_X, PWR_Y, PWR_R + 2, head);
    tft.fillCircle(PWR_X, PWR_Y, PWR_R, bg);
    // power icon: an open ring with a bar through the gap
    for (int r = 11; r <= 13; r++) tft.drawCircle(PWR_X, PWR_Y + 1, r, fg);
    tft.fillRect(PWR_X - 6, PWR_Y - 14, 13, 10, bg);
    tft.fillRect(PWR_X - 1, PWR_Y - 15, 3, 15, fg);
}

static void drawHeader() {
    const Swatch& s = SWATCHES[swatch];
    const uint16_t bg = lit ? shown565(s) : tft.color565(21, 21, 21);
    const uint8_t pr = elkcmd::dim(s.r, intensity), pg = elkcmd::dim(s.g, intensity), pb = elkcmd::dim(s.b, intensity);
    const uint16_t ink = lit && bright(pr, pg, pb) ? TFT_BLACK : TFT_WHITE;
    static const char* zones[] = {"Both", "Inside", "Outside"};
    tft.fillRect(0, 0, 320, HEAD_H, bg);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(ink, bg);
    tft.drawString(lit ? s.name : "Lights off", 14, 12, 4);
    char sub[32];
    snprintf(sub, sizeof(sub), "%s  -  %u%%", zones[target], intensity);
    tft.drawString(sub, 14, 44, 2);
    drawPower();
}

static void drawZone(int i, bool pressed = false) {
    static const char* names[] = {"BOTH", "INSIDE", "OUTSIDE"};
    const bool sel = target == static_cast<Target>(i);
    const uint16_t bg = pressed ? PRESSED : sel ? TFT_WHITE : tft.color565(51, 51, 51);
    const int x = 4 + i * 104;
    tft.fillRoundRect(x, ZONE_Y, 100, ZONE_H, 6, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(sel || pressed ? TFT_BLACK : TFT_LIGHTGREY, bg);
    tft.drawString(names[i], x + 46, ZONE_Y + ZONE_H / 2, 2);
    if (i > 0) tft.fillCircle(x + 88, ZONE_Y + ZONE_H / 2, 4, linkUp(controllers[i - 1]) ? TFT_GREEN : TFT_RED);
}

static void drawZones() {
    for (int i = 0; i < 3; i++) drawZone(i);
}

static void drawSwatch(int i, bool pressed = false) {
    const Swatch& s = SWATCHES[i];
    const int x = SW_X0 + i * SW_STEP;
    const bool sel = lit && i == swatch;
    const int inset = pressed ? 5 : 0;  // pressed: the colour shrinks inside a yellow ring
    tft.fillRect(x, STRIP_Y, SW_W, STRIP_H, TFT_BLACK);
    tft.fillRoundRect(x + inset, STRIP_Y + inset, SW_W - 2 * inset, STRIP_H - 2 * inset, 8,
                      tft.color565(s.r, s.g, s.b));
    if (pressed || sel) {  // white ring: what the lights show; yellow: under the finger
        const uint16_t ring = pressed ? PRESSED : TFT_WHITE;
        for (int k = 0; k < 3; k++) tft.drawRoundRect(x + k, STRIP_Y + k, SW_W - 2 * k, STRIP_H - 2 * k, 8, ring);
        if (!pressed) tft.drawRoundRect(x + 3, STRIP_Y + 3, SW_W - 6, STRIP_H - 6, 6, TFT_BLACK);
    }
}

static void drawStrip() {
    for (int i = 0; i < N_SWATCH; i++) drawSwatch(i);
}

static void drawStep(int x, bool pressed = false) {
    const uint16_t bg = pressed ? PRESSED : tft.color565(51, 51, 51);
    tft.fillRoundRect(x, BOT_Y, STEP_W, BOT_H, 8, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(pressed ? TFT_BLACK : TFT_WHITE, bg);
    tft.drawString(x == MINUS_X ? "-" : "+", x + STEP_W / 2, BOT_Y + BOT_H / 2, 4);
}

// Ten bars rising left to right, one per 10 %; the lit ones take the colour (grey while off).
static void drawBars() {
    const Swatch& s = SWATCHES[swatch];
    const uint16_t on = lit ? tft.color565(s.r, s.g, s.b) : TFT_DARKGREY;
    const int litBars = (intensity + 5) / 10;
    tft.fillRect(BARS_X - 2, BOT_Y, 10 * BAR_STEP + 2, BOT_H, TFT_BLACK);
    for (int k = 0; k < 10; k++) {
        const int h = 16 + k * 4;
        tft.fillRoundRect(BARS_X + k * BAR_STEP, BOT_Y + BOT_H - h, BAR_W, h, 3,
                          k < litBars ? on : tft.color565(42, 42, 42));
    }
}

static void drawAll() {
    tft.fillScreen(TFT_BLACK);
    drawHeader();
    drawZones();
    drawStrip();
    drawStep(MINUS_X);
    drawBars();
    drawStep(PLUS_X);
}

// ---- Touch calibration: tap five crosses with the pen, fit, keep it in NVS ("touch"/"hom") ----
// Runs on first boot, when BOOT is held at power-up, or when BOOT is held for 2 s.

static constexpr int PIN_BOOT = 0;
static constexpr int N_TARGET = 5;
static const int CAL_SX[N_TARGET] = {24, 296, 296, 24, 160};
static const int CAL_SY[N_TARGET] = {24, 24, 216, 216, 120};

static void drawTarget(int x, int y, uint16_t c) {
    tft.drawFastHLine(x - 14, y, 29, c);
    tft.drawFastVLine(x, y - 14, 29, c);
    tft.drawCircle(x, y, 8, c);
}

// One pen tap: waits for the panel to read released, then averages the press's readings.
static void waitTap(int* rx, int* ry) {
    int x, y;
    for (uint32_t freeSince = millis(); millis() - freeSince < 150;) {
        if (touch::readRaw(&x, &y)) freeSince = millis();
        delay(5);
    }
    for (;;) {
        while (!touch::readRaw(&x, &y)) delay(5);
        delay(60);  // let the pen settle
        long sx = 0, sy = 0;
        int n = 0;
        for (uint32_t t0 = millis(); millis() - t0 < 250 && touch::readRaw(&x, &y); n++) {
            sx += x;
            sy += y;
            delay(10);
        }
        if (n >= 5) {
            *rx = static_cast<int>(sx / n);
            *ry = static_cast<int>(sy / n);
            return;
        }
    }
}

static void calibrate() {
    for (;;) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Touch calibration", 160, 82, 4);
        tft.drawString("Tap each cross with the pen", 160, 160, 2);
        int rx[N_TARGET], ry[N_TARGET];
        for (int i = 0; i < N_TARGET; i++) {
            drawTarget(CAL_SX[i], CAL_SY[i], TFT_RED);
            waitTap(&rx[i], &ry[i]);
            drawTarget(CAL_SX[i], CAL_SY[i], TFT_GREEN);
            Serial.printf("[cal] target %d,%d <- raw %d,%d\n", CAL_SX[i], CAL_SY[i], rx[i], ry[i]);
        }
        // The four corners fit the map exactly; the centre tap checks it. Pen taps on this panel
        // scatter ~10 px, so 25 px still means "hit the cross".
        touchcal::Homography k;
        const bool ok = touchcal::fit(rx, ry, CAL_SX, CAL_SY, &k);
        const int err = ok ? touchcal::error(k, rx[4], ry[4], CAL_SX[4], CAL_SY[4]) : 999;
        Serial.printf("[cal] fit %s, centre off by %d px\n", ok ? "ok" : "failed", err);
        if (ok && err <= 25) {
            Preferences p;
            p.begin("touch", false);
            p.putBytes("hom", &k, sizeof(k));
            p.end();
            touch::setCalibration(k);
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString("Calibrated", 160, 120, 4);
            delay(800);
            return;
        }
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Missed a cross - again", 160, 120, 4);
        delay(1500);
    }
}

static bool loadCalibration() {
    Preferences p;
    p.begin("touch", true);
    touchcal::Homography k;
    const bool ok = p.getBytesLength("hom") == sizeof(k) && p.getBytes("hom", &k, sizeof(k)) == sizeof(k);
    p.end();
    if (ok) touch::setCalibration(k);
    return ok;
}

// ---- Input ----

static void save() {
    prefs.putUChar("swatch", static_cast<uint8_t>(swatch));
    prefs.putUChar("pct", intensity);
    prefs.putUChar("target", static_cast<uint8_t>(target));
    prefs.putBool("lit", lit);
}

static uint32_t lastSend = 0;

static void setIntensity(int pct) {
    pct = pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct;
    if (pct == intensity) return;
    intensity = static_cast<uint8_t>(pct);
    if (lit) pushTargets();
    lastSend = millis();
    drawHeader();
    drawBars();
}

// The button held down right now (drawn pressed until release). The bars have no pressed look:
// their fill follows the finger.
enum PressKind { NONE, POWER, ZONE, SWATCH, STEP };
static PressKind pressKind = NONE;
static int pressIdx = 0;

static void drawPress(bool pressed) {
    if (pressKind == POWER) drawPower(pressed);
    else if (pressKind == ZONE) drawZone(pressIdx, pressed);
    else if (pressKind == SWATCH) drawSwatch(pressIdx, pressed);
    else if (pressKind == STEP) drawStep(pressIdx, pressed);
}

static void onTouch(int x, int y, bool fresh) {
    if (y < HEAD_H) {
        const int dx = x - PWR_X, dy = y - PWR_Y;
        if (!fresh || dx * dx + dy * dy > (PWR_R + 8) * (PWR_R + 8)) return;
        lit = !lit;
        pushTargets();
        drawHeader();
        drawStrip();
        drawBars();
        pressKind = POWER;
    } else if (y >= ZONE_Y - 2 && y < ZONE_Y + ZONE_H + 2) {
        if (!fresh) return;
        target = static_cast<Target>(x / 107 > 2 ? 2 : x / 107);
        drawZones();
        drawHeader();
        pressKind = ZONE;
        pressIdx = target;
    } else if (y >= STRIP_Y - 2 && y < STRIP_Y + STRIP_H + 4) {
        if (!fresh) return;
        const int i = (x - SW_X0) / SW_STEP;
        swatch = i < 0 ? 0 : i >= N_SWATCH ? N_SWATCH - 1 : i;
        lit = true;
        pushTargets();
        drawHeader();
        drawStrip();
        drawBars();
        pressKind = SWATCH;
        pressIdx = swatch;
    } else if (y >= BOT_Y - 6) {
        if (x < MINUS_X + STEP_W) {
            if (!fresh) return;
            setIntensity(intensity - 10);
            pressKind = STEP;
            pressIdx = MINUS_X;
        } else if (x >= PLUS_X) {
            if (!fresh) return;
            setIntensity(intensity + 10);
            pressKind = STEP;
            pressIdx = PLUS_X;
        } else if (fresh || millis() - lastSend > 80) {  // bars: tap or drag, one update per 80 ms
            const int k = (x - BARS_X) / BAR_STEP;
            setIntensity((k < 0 ? 0 : k > 9 ? 9 : k) * 10 + 10);
        }
    } else {
        return;
    }
    if (fresh) {
        save();
        drawPress(true);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);
    tft.init();
    tft.invertDisplay(true);  // CYD panel: colours are inverted without this
    tft.setRotation(1);
    touch::begin();

    prefs.begin("leds", false);
    swatch = prefs.getUChar("swatch", static_cast<uint8_t>(swatch)) % N_SWATCH;
    intensity = prefs.getUChar("pct", intensity);
    if (intensity < MIN_INTENSITY || intensity > 100) intensity = 100;
    target = static_cast<Target>(prefs.getUChar("target", BOTH) % 3);
    lit = prefs.getBool("lit", true);

    pinMode(PIN_BOOT, INPUT_PULLUP);
    if (!loadCalibration() || digitalRead(PIN_BOOT) == LOW) calibrate();
    drawAll();
    NimBLEDevice::init("");
    xTaskCreatePinnedToCore(linkTask, "leds-link", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
    int x, y;
    static bool wasDown = false;
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second.
    const bool down = touch::read(&x, &y) && millis() > 1000;
    if (down) onTouch(x, y, !wasDown);
    if (!down && wasDown) {  // released: back to the normal look
        drawPress(false);
        pressKind = NONE;
    }
    wasDown = down;

    // BOOT held for 2 s: calibrate the touch again.
    static uint32_t bootSince = 0;
    if (digitalRead(PIN_BOOT) == LOW) {
        if (bootSince == 0) bootSince = millis();
        else if (millis() - bootSince > 2000) {
            calibrate();
            drawAll();
            bootSince = 0;
        }
    } else {
        bootSince = 0;
    }

    // A controller that just came up gets the current state, so it matches the screen.
    static bool shown[N_CTRL] = {};
    for (int i = 0; i < N_CTRL; i++) {
        Controller& c = controllers[i];
        const bool up = linkUp(c);
        if (up && c.fresh) {
            c.fresh = false;
            if (targeted(i)) push(c);
        }
        if (up != shown[i]) {
            shown[i] = up;
            drawZone(i + 1, pressKind == ZONE && pressIdx == i + 1);
        }
    }
    delay(10);
}
