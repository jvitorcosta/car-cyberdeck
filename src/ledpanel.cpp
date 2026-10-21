// Standalone LED panel for the car's two ELK-family controllers on the yellow deck (CYD). Not the
// firmware; a first step before LED control becomes a deck feature. Layout D, built for fingers:
//   top:    BOTH / INSIDE / OUTSIDE (dot: that controller's link, green = connected)
//   grid:   2x4 tiles of 75x66: red, orange, pink, purple, blue, green, white, power
//   bottom: -  [level: the colour at its intensity, and the %]  +
// A press acts once it settles (its first ~40 ms averaged), and a tap in a gap snaps to the nearest
// button. The iPhone's Bluetooth must be off (Settings): each controller takes one connection.
// Colour, intensity, power and zone survive a reboot (NVS). Touch calibration (resistive panel)
// runs on first boot, or with BOOT held (at power-up, or 2 s any time). Voice: Siri Shortcuts call
// GET /say over WiFi (see "Voice" below).
//   pio run -e ledpanel -t upload
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include "pins.h"
#include "companion/touch.h"
#include "net/remote_config.h"
#include "net/wifi.h"
#include "util/elkcmd.h"
#include "util/ledvoice.h"
#include "util/remotegate.h"
#include "util/touchcal.h"

static TFT_eSPI tft;
static Preferences prefs;

// ---- State ----

// Each colour has two values. `led` is what the strip gets: RGB LEDs don't mix like a screen (green
// and blue emitters look far brighter than red), so orange, pink and purple need much less green or
// blue than their screen colours, and white is warmed to offset the strips' blue cast. `screen` is
// what the tile shows: fully saturated, because this panel washes colours out (soft "LED-like"
// tints looked dead on it).
struct Swatch {
    const char* name;
    uint8_t r, g, b;   // led
    uint32_t screen;   // 0xRRGGBB
};
static const Swatch SWATCHES[] = {
    {"Red", 255, 0, 0, 0xFF0000},       {"Orange", 255, 40, 0, 0xFF6A00},
    {"Pink", 255, 0, 70, 0xFF0090},     {"Purple", 120, 0, 255, 0x9000FF},
    {"Blue", 0, 0, 255, 0x0040FF},      {"Green", 0, 255, 0, 0x00FF30},
    {"White", 255, 215, 170, 0xFFFFFF},
};
static constexpr int N_SWATCH = sizeof(SWATCHES) / sizeof(SWATCHES[0]);

enum Target { BOTH, INSIDE, OUTSIDE };
static Target target = BOTH;
static int swatch = N_SWATCH - 1;  // white
static uint8_t intensity = 100;    // 10..100 %, in steps of 10
static bool lit = true;

static constexpr uint8_t MIN_INTENSITY = 10;

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
// (core 1) never waits. A connect attempt keeps the radio, which WiFi shares, busy for its whole
// timeout: with the lights out of range (the deck at home), 5 s retries starved WiFi until the
// deck didn't even answer ping. So the wait doubles after each failed round, 5 s up to 60 s, and
// drops back to 5 s once everything is connected or a link is lost.
static void linkTask(void*) {
    uint32_t waitMs = 5000;
    for (;;) {
        bool missing = false;
        for (Controller& c : controllers) {
            if (!linkUp(c) && !connect(c)) missing = true;
        }
        waitMs = missing ? (waitMs * 2 > 60000 ? 60000 : waitMs * 2) : 5000;
        // wake early when a connected light drops, so it comes back quickly
        for (uint32_t t = 0; t < waitMs; t += 500) {
            vTaskDelay(pdMS_TO_TICKS(500));
            bool lost = false;
            for (const Controller& c : controllers) lost = lost || (c.tx && !linkUp(c));
            if (lost) {
                waitMs = 5000;
                break;
            }
        }
    }
}

// ---- Buttons (320x240 landscape) ----

enum : int { ZONE0 = 0, TILE0 = 3, POWER = TILE0 + N_SWATCH, MINUS, PLUS, N_BTN };
struct Rect { int x, y, w, h; };

static Rect rectOf(int id) {
    if (id < TILE0) return {4 + id * 106, 4, 100, 36};
    if (id <= POWER) {
        const int i = id - TILE0;  // the power button is the grid's 8th tile
        return {4 + (i % 4) * 79, 46 + (i / 4) * 70, 75, 66};
    }
    if (id == MINUS) return {4, 188, 96, 48};
    return {220, 188, 96, 48};  // PLUS
}
static constexpr Rect LEVEL{106, 188, 108, 48};  // display only

// The button under (x, y), or the nearest within SNAP px when the tap lands in a gap; -1 if none.
static constexpr int SNAP = 10;
static int hit(int x, int y) {
    int best = -1, bestD = SNAP + 1;
    for (int id = 0; id < N_BTN; id++) {
        const Rect r = rectOf(id);
        const int dx = x < r.x ? r.x - x : x >= r.x + r.w ? x - (r.x + r.w - 1) : 0;
        const int dy = y < r.y ? r.y - y : y >= r.y + r.h ? y - (r.y + r.h - 1) : 0;
        const int d = dx > dy ? dx : dy;
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

// ---- Drawing ----

static constexpr uint16_t PRESSED = TFT_YELLOW;  // under the finger, until release
static uint16_t grey(uint8_t v) { return tft.color565(v, v, v); }

static uint16_t c565(uint32_t rgb) { return tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
// The screen colour at a share of its brightness (for the level box).
static uint16_t c565At(uint32_t rgb, int pct) {
    return tft.color565(((rgb >> 16) & 0xFF) * pct / 100, ((rgb >> 8) & 0xFF) * pct / 100, (rgb & 0xFF) * pct / 100);
}
static bool bright(uint32_t rgb, int pct = 100) {
    return (((rgb >> 16) & 0xFF) * 3 + ((rgb >> 8) & 0xFF) * 6 + (rgb & 0xFF)) / 10 * pct / 100 > 140;
}

static void drawPowerIcon(int cx, int cy, uint16_t fg, uint16_t bg) {
    for (int r = 11; r <= 13; r++) tft.drawCircle(cx, cy + 1, r, fg);
    tft.fillRect(cx - 6, cy - 14, 13, 10, bg);
    tft.fillRect(cx - 1, cy - 15, 3, 15, fg);
}

static void drawButton(int id, bool pressed = false) {
    const Rect r = rectOf(id);
    tft.setTextDatum(MC_DATUM);
    const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (id < TILE0) {
        static const char* names[] = {"BOTH", "INSIDE", "OUTSIDE"};
        const bool sel = target == static_cast<Target>(id);
        const uint16_t bg = pressed ? PRESSED : sel ? TFT_WHITE : grey(51);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
        tft.setTextColor(sel || pressed ? TFT_BLACK : TFT_LIGHTGREY, bg);
        tft.drawString(names[id], cx - (id > 0 ? 6 : 0), cy, 2);
        if (id > 0) tft.fillCircle(r.x + r.w - 12, cy, 5, linkUp(controllers[id - 1]) ? TFT_GREEN : TFT_RED);
        return;
    }
    if (id < POWER) {
        const Swatch& s = SWATCHES[id - TILE0];
        const bool sel = lit && id - TILE0 == swatch;
        const int in = pressed ? 5 : 0;  // pressed: the colour shrinks inside a yellow ring
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        tft.fillRoundRect(r.x + in, r.y + in, r.w - 2 * in, r.h - 2 * in, 10, c565(s.screen));
        if (pressed || sel) {  // white ring: what the lights show
            for (int k = 0; k < 3; k++) tft.drawRoundRect(r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k, 10, pressed ? PRESSED : TFT_WHITE);
            if (!pressed) tft.drawRoundRect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, 8, TFT_BLACK);
        }
        const uint16_t fill = c565(s.screen);
        tft.setTextColor(bright(s.screen) ? TFT_BLACK : TFT_WHITE, fill);
        tft.drawString(s.name, cx, cy, 2);
        return;
    }
    if (id == POWER) {
        const uint16_t bg = pressed ? PRESSED : lit ? grey(51) : TFT_RED;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 10, bg);
        drawPowerIcon(cx, cy - 6, pressed ? TFT_BLACK : TFT_WHITE, bg);
        tft.setTextColor(pressed ? TFT_BLACK : TFT_WHITE, bg);
        tft.drawString(lit ? "Off" : "On", cx, r.y + r.h - 12, 2);
        return;
    }
    const uint16_t bg = pressed ? PRESSED : grey(51);  // MINUS / PLUS
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 10, bg);
    tft.setTextColor(pressed ? TFT_BLACK : TFT_WHITE, bg);
    tft.drawString(id == MINUS ? "-" : "+", cx, cy, 4);
}

// The colour at its intensity (shown no darker than 25 % so it stays readable) and the %.
static void drawLevel() {
    const Swatch& s = SWATCHES[swatch];
    const int shade = intensity < 25 ? 25 : intensity;
    const uint16_t bg = lit ? c565At(s.screen, shade) : grey(26);
    tft.fillRoundRect(LEVEL.x, LEVEL.y, LEVEL.w, LEVEL.h, 10, bg);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", intensity);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(lit ? (bright(s.screen, shade) ? TFT_BLACK : TFT_WHITE) : TFT_DARKGREY, bg);
    tft.drawString(lit ? pct : "Off", LEVEL.x + LEVEL.w / 2, LEVEL.y + LEVEL.h / 2, 4);
}

static void drawGrid() {
    for (int id = TILE0; id <= POWER; id++) drawButton(id);
}

static void drawAll() {
    tft.fillScreen(TFT_BLACK);
    for (int id = 0; id < N_BTN; id++) drawButton(id);
    drawLevel();
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

// One settled tap on a button.
static void press(int id) {
    if (id < TILE0) {
        target = static_cast<Target>(id);
        for (int z = 0; z < TILE0; z++) drawButton(z);
    } else if (id < POWER) {
        swatch = id - TILE0;
        lit = true;
        pushTargets();
        drawGrid();
        drawLevel();
    } else if (id == POWER) {
        lit = !lit;
        pushTargets();
        drawGrid();
        drawLevel();
    } else {
        const int pct = intensity + (id == PLUS ? 10 : -10);
        intensity = static_cast<uint8_t>(pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct);
        if (lit) pushTargets();
        drawLevel();
    }
    save();
}

// A press is averaged over its first SETTLE_MS (a fingertip's first readings jump around) and acts
// once; a tap shorter than that acts on release with what it had.
static constexpr uint32_t SETTLE_MS = 40;
static struct {
    bool down = false, acted = false;
    uint32_t t0 = 0;
    long sx = 0, sy = 0;
    int n = 0, id = -1;  // id: the button drawn pressed
} tap;

static void act() {
    tap.acted = true;
    const int x = static_cast<int>(tap.sx / tap.n), y = static_cast<int>(tap.sy / tap.n);
    tap.id = hit(x, y);
    Serial.printf("[tap] %d,%d -> button %d\n", x, y, tap.id);
    if (tap.id < 0) return;
    press(tap.id);
    drawButton(tap.id, true);
}

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"); voice always changes both lights. GET /state?k= answers the same line without changing anything.
// The server runs in its own task; commands go to loop() through a queue (it owns the screen and
// the state) and the handler waits for loop()'s reply.

static WebServer* server = nullptr;
static remotegate::Gate gate(REMOTE_TOKEN);
static QueueHandle_t voiceQueue = nullptr;
static SemaphoreHandle_t voiceDone = nullptr;
static char voiceReply[64];

static void describe(char* out, size_t n) {
    if (!lit) snprintf(out, n, "Lights off");
    else snprintf(out, n, "%s, %u percent", SWATCHES[swatch].name, intensity);
}

// In loop(): applies one voice command to both lights (voice doesn't pick a zone, and leaves the
// screen's zone as it was), then answers the waiting handler.
static void applyVoice(const ledvoice::Command& c) {
    if (c.color >= 0) {
        swatch = c.color;
        lit = true;
    }
    if (c.power >= 0) lit = c.power == 1;
    int pct = c.level >= 0 ? c.level : intensity + 10 * c.levelStep;
    intensity = static_cast<uint8_t>(pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct);
    for (Controller& ctrl : controllers) push(ctrl);
    save();
    drawAll();
    describe(voiceReply, sizeof(voiceReply));
    Serial.printf("[voice] -> %s\n", voiceReply);
    xSemaphoreGive(voiceDone);
}

static void handleSay() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    const String text = server->arg("text");
    Serial.printf("[voice] \"%s\"\n", text.c_str());
    ledvoice::Command c;
    if (!ledvoice::parse(text.c_str(), &c)) { server->send(200, "text/plain", "Sorry, I didn't get a colour or a command"); return; }
    xSemaphoreTake(voiceDone, 0);  // drop a stale answer
    if (xQueueSend(voiceQueue, &c, pdMS_TO_TICKS(200)) != pdTRUE || xSemaphoreTake(voiceDone, pdMS_TO_TICKS(1500)) != pdTRUE) {
        server->send(503, "text/plain", "The deck is busy, try again");
        return;
    }
    server->send(200, "text/plain", voiceReply);
}

static void handleState() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    char line[64];
    describe(line, sizeof(line));
    server->send(200, "text/plain", line);
}

static void voiceTask(void*) {
    net::connectAny();
    while (!net::isOnline()) {
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    server->begin();
    Serial.printf("[voice] on: http://%s/say?text=purple&k=<REMOTE_TOKEN>  (heap %u)\n",
                  net::deviceIp().c_str(), static_cast<unsigned>(ESP.getFreeHeap()));
    for (;;) {
        server->handleClient();
        net::loop();  // rejoins after a drop
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void beginVoice() {
    if (!gate.enabled()) {
        Serial.println("[voice] off (REMOTE_TOKEN empty)");
        return;
    }
    voiceQueue = xQueueCreate(2, sizeof(ledvoice::Command));
    voiceDone = xSemaphoreCreateBinary();
    server = new WebServer(80);
    server->on("/say", HTTP_ANY, handleSay);
    server->on("/state", HTTP_GET, handleState);
    xTaskCreatePinnedToCore(voiceTask, "voice", 6144, nullptr, 0, nullptr, 1);
}

// TFT_eSPI's ILI9341_2_DRIVER init leaves a flat gamma that makes this panel look washed out; the
// standard ILI9341 tables (as in Adafruit's driver) give saturated colours and deeper blacks.
static void vividGamma() {
    static const uint8_t POS[15] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                                    0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
    static const uint8_t NEG[15] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                                    0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
    tft.writecommand(0x26);  // gamma curve 1
    tft.writedata(0x01);
    tft.writecommand(0xE0);  // positive gamma correction
    for (uint8_t v : POS) tft.writedata(v);
    tft.writecommand(0xE1);  // negative gamma correction
    for (uint8_t v : NEG) tft.writedata(v);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);
    tft.init();
    tft.invertDisplay(true);  // CYD panel: colours are inverted without this
    tft.setRotation(1);
    vividGamma();
    touch::begin();

    prefs.begin("leds", false);
    swatch = prefs.getUChar("swatch", static_cast<uint8_t>(swatch)) % N_SWATCH;
    intensity = prefs.getUChar("pct", intensity);
    intensity = static_cast<uint8_t>((intensity + 5) / 10 * 10);  // older builds stored any %
    if (intensity < MIN_INTENSITY || intensity > 100) intensity = 100;
    target = static_cast<Target>(prefs.getUChar("target", BOTH) % 3);
    lit = prefs.getBool("lit", true);

    pinMode(PIN_BOOT, INPUT_PULLUP);
    if (!loadCalibration() || digitalRead(PIN_BOOT) == LOW) calibrate();
    drawAll();
    NimBLEDevice::init("");
    xTaskCreatePinnedToCore(linkTask, "leds-link", 4096, nullptr, 1, nullptr, 0);
    beginVoice();
}

void loop() {
    int x, y;
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second.
    const bool down = touch::read(&x, &y) && millis() > 1000;
    if (down) {
        if (!tap.down) tap = {true, false, millis(), 0, 0, 0, -1};
        if (!tap.acted) {
            tap.sx += x;
            tap.sy += y;
            tap.n++;
            if (millis() - tap.t0 >= SETTLE_MS && tap.n >= 3) act();
        }
    } else if (tap.down) {
        if (!tap.acted && tap.n > 0) act();  // a quick tap
        if (tap.id >= 0) drawButton(tap.id);  // released: back to the normal look
        tap.down = false;
    }

    ledvoice::Command voice;
    if (voiceQueue && xQueueReceive(voiceQueue, &voice, 0) == pdTRUE) applyVoice(voice);

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
            drawButton(ZONE0 + i + 1, tap.down && tap.id == ZONE0 + i + 1);
        }
    }
    delay(10);
}
