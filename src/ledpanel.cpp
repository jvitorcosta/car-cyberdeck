// Standalone LED panel for the car's two ELK-family controllers on the yellow deck (CYD). Not the
// firmware; a first step before LED control becomes a deck feature.
//   top:    BOTH / INSIDE / OUTSIDE tabs (dot: that controller's link, green = connected)
//   middle: red, orange, pink, purple, blue, green, white, OFF
//   bottom: intensity  -  [bar, tap or drag]  +
// The iPhone's Bluetooth must be off (Settings): each controller takes one connection at a time.
// Colour, intensity and tab survive a reboot (NVS).
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
    {"RED", 255, 0, 0},     {"ORANGE", 255, 60, 0}, {"PINK", 255, 20, 90},  {"PURPLE", 150, 0, 255},
    {"BLUE", 0, 0, 255},    {"GREEN", 0, 255, 0},   {"WHITE", 255, 255, 255},
};
static constexpr int N_SWATCH = sizeof(SWATCHES) / sizeof(SWATCHES[0]);
static constexpr int OFF_CELL = N_SWATCH;  // the grid's last cell

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

static constexpr int TAB_H = 32;
static constexpr int GRID_Y = 36, CELL_W = 80, CELL_H = 68;  // 2 rows x 4
static constexpr int BAR_Y = 180, BAR_H = 56;
static constexpr int MINUS_X = 0, PLUS_X = 264, BTN_W = 56;
static constexpr int BAR_X = 62, BAR_W = 196;

static void drawTabs() {
    static const char* names[] = {"BOTH", "INSIDE", "OUTSIDE"};
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < 3; i++) {
        const bool sel = target == static_cast<Target>(i);
        const uint16_t bg = sel ? TFT_WHITE : TFT_DARKGREY;
        const int x = i * 107 + 2;
        tft.fillRoundRect(x, 2, 103, TAB_H - 4, 6, bg);
        tft.setTextColor(sel ? TFT_BLACK : TFT_WHITE, bg);
        tft.drawString(names[i], x + 51, TAB_H / 2, 2);
        if (i > 0) tft.fillCircle(x + 92, TAB_H / 2, 4, linkUp(controllers[i - 1]) ? TFT_GREEN : TFT_RED);
    }
}

static void drawCell(int i) {
    const int x = (i % 4) * CELL_W, y = GRID_Y + (i / 4) * CELL_H;
    const bool off = i == OFF_CELL;
    const uint16_t bg = off ? TFT_BLACK : tft.color565(SWATCHES[i].r, SWATCHES[i].g, SWATCHES[i].b);
    const bool sel = off ? !lit : (lit && i == swatch);
    tft.fillRect(x, y, CELL_W, CELL_H, TFT_BLACK);
    tft.fillRoundRect(x + 3, y + 3, CELL_W - 6, CELL_H - 6, 8, bg);
    if (sel) {  // a thick ring marks what the lights show now
        for (int k = 0; k < 3; k++) tft.drawRoundRect(x + 3 + k, y + 3 + k, CELL_W - 6 - 2 * k, CELL_H - 6 - 2 * k, 8, TFT_WHITE);
    } else if (off) {
        tft.drawRoundRect(x + 3, y + 3, CELL_W - 6, CELL_H - 6, 8, TFT_DARKGREY);
    }
    const Swatch* s = off ? nullptr : &SWATCHES[i];
    const bool light = s && (s->r * 3 + s->g * 6 + s->b) / 10 > 140;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(light ? TFT_BLACK : TFT_WHITE, bg);
    tft.drawString(off ? "OFF" : s->name, x + CELL_W / 2, y + CELL_H / 2, 2);
}

static void drawGrid() {
    for (int i = 0; i <= OFF_CELL; i++) drawCell(i);
}

static void drawIntensity() {
    tft.setTextDatum(MC_DATUM);
    for (int x : {MINUS_X, PLUS_X}) {
        tft.fillRoundRect(x + 2, BAR_Y + 2, BTN_W - 4, BAR_H - 4, 8, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.drawString(x == MINUS_X ? "-" : "+", x + BTN_W / 2, BAR_Y + BAR_H / 2, 4);
    }
    const Swatch& s = SWATCHES[swatch];
    const uint16_t fill = tft.color565(elkcmd::dim(s.r, intensity), elkcmd::dim(s.g, intensity),
                                       elkcmd::dim(s.b, intensity));
    const int w = BAR_W * intensity / 100;
    tft.fillRoundRect(BAR_X, BAR_Y + 2, BAR_W, BAR_H - 4, 8, TFT_BLACK);
    tft.fillRoundRect(BAR_X, BAR_Y + 2, w, BAR_H - 4, 8, fill);
    tft.drawRoundRect(BAR_X, BAR_Y + 2, BAR_W, BAR_H - 4, 8, TFT_LIGHTGREY);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", intensity);
    tft.setTextColor(TFT_WHITE);  // transparent background over the split bar
    tft.drawString(pct, BAR_X + BAR_W / 2, BAR_Y + BAR_H / 2, 4);
}

// ---- Input ----

static void save() {
    prefs.putUChar("swatch", static_cast<uint8_t>(swatch));
    prefs.putUChar("pct", intensity);
    prefs.putUChar("target", static_cast<uint8_t>(target));
}

static uint32_t lastSend = 0;

static void setIntensity(int pct) {
    pct = pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct;
    if (pct == intensity) return;
    intensity = static_cast<uint8_t>(pct);
    drawIntensity();
    if (lit) pushTargets();
    lastSend = millis();
}

static void onTouch(int x, int y, bool fresh) {
    if (y < TAB_H) {
        if (!fresh) return;
        target = static_cast<Target>(x / 107 > 2 ? 2 : x / 107);
        drawTabs();
        save();
    } else if (y >= GRID_Y && y < GRID_Y + 2 * CELL_H) {
        if (!fresh) return;
        const int cell = ((y - GRID_Y) / CELL_H) * 4 + (x / CELL_W > 3 ? 3 : x / CELL_W);
        if (cell == OFF_CELL) {
            lit = false;
        } else {
            lit = true;
            swatch = cell;
        }
        pushTargets();
        drawGrid();
        drawIntensity();
        save();
    } else if (y >= BAR_Y) {
        if (x < MINUS_X + BTN_W) {
            if (fresh) setIntensity(intensity - 10);
        } else if (x >= PLUS_X) {
            if (fresh) setIntensity(intensity + 10);
        } else if (fresh || millis() - lastSend > 80) {  // drag: one update per 80 ms
            setIntensity((x - BAR_X) * 100 / BAR_W);
        }
        if (fresh) save();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);
    tft.init();
    tft.invertDisplay(true);  // CYD panel: colours are inverted without this
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    touch::begin();

    prefs.begin("leds", false);
    swatch = prefs.getUChar("swatch", static_cast<uint8_t>(swatch)) % N_SWATCH;
    intensity = prefs.getUChar("pct", intensity);
    if (intensity < MIN_INTENSITY || intensity > 100) intensity = 100;
    target = static_cast<Target>(prefs.getUChar("target", BOTH) % 3);

    drawTabs();
    drawGrid();
    drawIntensity();
    NimBLEDevice::init("");
    xTaskCreatePinnedToCore(linkTask, "leds-link", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
    int x, y;
    static bool wasDown = false;
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second.
    const bool down = touch::read(&x, &y) && millis() > 1000;
    if (down) onTouch(x, y, !wasDown);
    wasDown = down;

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
            drawTabs();
        }
    }
    delay(10);
}
