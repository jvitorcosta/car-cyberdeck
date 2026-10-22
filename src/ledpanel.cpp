// Standalone LED panel for the car's two ELK-family controllers on the yellow deck (CYD). Not the
// firmware; a first step before LED control becomes a deck feature. Layout D, built for fingers:
//   top:    pages BOTH / INSIDE / OUTSIDE. BOTH sets both lights together; INSIDE and OUTSIDE
//           each keep their own look. Each zone tab shows its colour, ringed green when connected
//   grid:   2x4 tiles of 75x66: red, orange, pink, purple, blue, green, white, yellow; tap one to
//           set it (and turn on), double-tap the lit one to turn the lights off
//   bottom: -  [level: the colour at its intensity and the %, or "Off"]  +
// A press acts once it settles (its first ~40 ms averaged), and a tap in a gap snaps to the nearest
// button. The iPhone's Bluetooth must be off (Settings): each controller takes one connection.
// Each page's colour, intensity and power, and the page shown, survive a reboot (NVS). Touch calibration (resistive panel)
// runs on first boot, or with BOOT held (at power-up, or 2 s any time). Voice: Siri Shortcuts call
// GET /say over WiFi (see "Voice" below). An Xbox controller (BLE) drives it too: a cyan ring
// marks the focused button (see "Xbox controller input" below).
//   pio run -e ledpanel -t upload
#include <Arduino.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // NimBLE 2's and the Xbox library's headers;
#pragma GCC diagnostic ignored "-Wdouble-promotion"  // src/ builds with -Werror
#include <NimBLEDevice.h>
#include <XboxPad.hpp>  // lib/xboxpad: asukiaaa's Xbox library, vendored with shorter timeouts
#pragma GCC diagnostic pop
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
    {"White", 255, 215, 170, 0xFFFFFF}, {"Yellow", 255, 110, 0, 0xFFD400},  // LED yellow: full green goes lime
};
static constexpr int N_SWATCH = sizeof(SWATCHES) / sizeof(SWATCHES[0]);

// The zone tabs are pages. BOTH is the general page: a change there goes to both lights (the
// inside and outside looks become a copy of it). INSIDE and OUTSIDE each keep their own look and
// change only their light. Voice works like the BOTH page.
enum Target { BOTH, INSIDE, OUTSIDE };
static Target target = BOTH;  // the page on screen

struct Look {
    int swatch = N_SWATCH - 1;  // white
    uint8_t pct = 100;          // 10..100 %, in steps of 10
    bool lit = true;
};
static Look looks[3];  // indexed by Target; controller i shows looks[i + 1]
static Look& cur() { return looks[target]; }

static constexpr uint8_t MIN_INTENSITY = 10;

// ---- BLE ----

// The link task (core 0) connects; the loop (core 1) writes. `tx` is set once, on the first
// connect, and the client keeps its attributes across reconnects (connect(.., false)), so the
// characteristic it points to is never freed under the loop's feet.
struct Controller {
    const char* label;
    const char* addr;
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* volatile tx = nullptr;  // FFF3, once discovered
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
    NimBLERemoteCharacteristic* tx = c.tx;  // one read: the link task may be reconnecting
    if (tx && c.client && c.client->isConnected() && !tx->writeValue(f.b, sizeof(f.b), false)) {
        Serial.printf("[leds] %s: write failed\n", c.label);
    }
}

// Controller i's look (power + dimmed colour), both header variants.
static void push(int i) {
    Controller& c = controllers[i];
    const Look& l = looks[i + 1];
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(c, elkcmd::power(v, l.lit));
    if (!l.lit) return;
    const Swatch& s = SWATCHES[l.swatch];
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) {
        write(c, elkcmd::color(v, elkcmd::dim(s.r, l.pct), elkcmd::dim(s.g, l.pct), elkcmd::dim(s.b, l.pct)));
    }
}

// After the page's look changed: on BOTH it becomes both zones' look; then the lights it covers
// get it.
static void pushTargets() {
    if (target == BOTH) looks[INSIDE] = looks[OUTSIDE] = looks[BOTH];
    for (int i = 0; i < N_CTRL; i++) {
        if (target == BOTH || target == static_cast<Target>(i + 1)) push(i);
    }
}

static bool connect(Controller& c) {
    if (!c.client) {
        c.client = NimBLEDevice::createClient();
        c.client->setConnectTimeout(4000);  // NimBLE 2: milliseconds
    }
    // false: keep the discovered attributes, so `tx` stays valid for the loop across reconnects
    if (!c.client->isConnected() && !c.client->connect(NimBLEAddress(std::string(c.addr), BLE_ADDR_PUBLIC), false)) {
        Serial.printf("[leds] %s: connect failed\n", c.label);
        return false;
    }
    if (!c.tx) {
        NimBLERemoteService* svc = c.client->getService(NimBLEUUID(static_cast<uint16_t>(0xFFF0)));
        if (svc) c.tx = svc->getCharacteristic(NimBLEUUID(static_cast<uint16_t>(0xFFF3)));
    }
    if (!c.tx) {  // connected but discovery failed (e.g. a GATT timeout): drop it, try again later
        Serial.printf("[leds] %s: no FFF3, disconnecting\n", c.label);
        c.client->disconnect();
        return false;
    }
    Serial.printf("[leds] %s: connected\n", c.label);
    c.fresh = true;
    return true;
}

// The Xbox controller (Series X|S / One 1708+, over BLE): the first one found in pairing mode,
// or the bonded one when it's switched on.
static XboxSeriesXControllerESP32_asukiaaa::Core xbox;

// All connecting runs here on core 0, so the touch loop (core 1) never waits on it. Connects and
// scans keep the radio, which WiFi shares, busy: with the lights out of range (the deck at home),
// 5 s retries starved WiFi until the deck didn't even answer ping. So:
//  - lights: only while one is missing, rounds of connects 5 s apart, doubling after each failed
//    round up to 60 s, back to 5 s (at once) when a connected light drops. A connect pauses a
//    running controller scan by itself (NimBLE stops it on EBUSY);
//  - controller: while connected (or mid-pairing) the library runs every tick; otherwise one of
//    its 4 s scans every 15 s (it scans flat out).
static constexpr uint32_t PAD_SCAN_EVERY_MS = 15000;
static TaskHandle_t linkHandle = nullptr;

static void linkTask(void*) {
    uint32_t waitMs = 5000, nextLights = 0, lastPadScan = 0;
    for (;;) {
        const uint32_t now = millis();
        const bool padBusy = xbox.isConnected() || XboxSeriesXControllerESP32_asukiaaa::advDevice != nullptr ||
                             NimBLEDevice::getScan()->isScanning();
        if (padBusy) {
            xbox.onLoop();
        } else if (now - lastPadScan > PAD_SCAN_EVERY_MS) {
            lastPadScan = now;
            xbox.onLoop();  // starts a scan
        }

        // lost: a light that was up last tick and isn't now (`tx` outlives the link, so it can't tell)
        static bool wasUp[N_CTRL] = {};
        bool lost = false, missing = false;
        for (int i = 0; i < N_CTRL; i++) {
            const bool up = linkUp(controllers[i]);
            lost = lost || (wasUp[i] && !up);
            missing = missing || !up;
            wasUp[i] = up;
        }
        if (lost) {
            waitMs = 5000;
            nextLights = now;
        }
        if (missing && static_cast<int32_t>(now - nextLights) >= 0) {
            bool still = false;
            for (Controller& c : controllers) {
                if (!linkUp(c) && !connect(c)) still = true;
            }
            nextLights = millis() + waitMs;
            waitMs = still ? (waitMs * 2 > 60000 ? 60000 : waitMs * 2) : 5000;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---- Buttons (320x240 landscape) ----

// The grid is all colours; there is no power tile: double-tapping the lit colour turns the lights
// off, and tapping any colour turns them back on.
enum : int { ZONE0 = 0, TILE0 = 3, TILE_END = TILE0 + N_SWATCH, MINUS = TILE_END, PLUS, N_BTN };
static_assert(N_SWATCH == 8, "the grid is 2x4");
struct Rect { int x, y, w, h; };

static Rect rectOf(int id) {
    if (id < TILE0) return {4 + id * 106, 4, 100, 36};
    if (id < TILE_END) {
        const int i = id - TILE0;
        return {4 + (i % 4) * 79, 46 + (i / 4) * 70, 75, 66};
    }
    if (id == MINUS) return {4, 188, 96, 48};
    return {220, 188, 96, 48};  // PLUS
}
static constexpr Rect LEVEL{106, 188, 108, 48};  // display only

// The button the Xbox controller points at (a cyan ring around it); -1 while none is connected.
static int focus = -1;
static void drawFocus(int id, bool on);

// The button under (x, y), or the nearest within SNAP px when the tap lands in a gap; -1 if none,
// and for the level box (display only: snapping from it pressed Green, White or -).
static constexpr int SNAP = 10;
static int hit(int x, int y) {
    if (x >= LEVEL.x && x < LEVEL.x + LEVEL.w && y >= LEVEL.y && y < LEVEL.y + LEVEL.h) return -1;
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

static void drawButtonBody(int id, bool pressed) {
    const Rect r = rectOf(id);
    tft.setTextDatum(MC_DATUM);
    const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (id < TILE0) {
        static const char* names[] = {"BOTH", "INSIDE", "OUTSIDE"};
        const bool sel = target == static_cast<Target>(id);
        const uint16_t bg = pressed ? PRESSED : sel ? TFT_WHITE : grey(51);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
        tft.setTextColor(sel || pressed ? TFT_BLACK : TFT_LIGHTGREY, bg);
        tft.drawString(names[id], cx - (id > 0 ? 9 : 0), cy, 2);
        if (id > 0) {  // that zone's colour (grey when off), ringed green when its light is connected
            const Look& l = looks[id];
            const int chipX = r.x + r.w - 24, chipY = cy - 9;
            tft.fillRoundRect(chipX, chipY, 18, 18, 4, linkUp(controllers[id - 1]) ? TFT_GREEN : TFT_RED);
            tft.fillRoundRect(chipX + 3, chipY + 3, 12, 12, 2, l.lit ? c565(SWATCHES[l.swatch].screen) : grey(70));
        }
        return;
    }
    if (id < TILE_END) {
        const Swatch& s = SWATCHES[id - TILE0];
        const bool sel = cur().lit && id - TILE0 == cur().swatch;
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
    const uint16_t bg = pressed ? PRESSED : grey(51);  // MINUS / PLUS
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 10, bg);
    tft.setTextColor(pressed ? TFT_BLACK : TFT_WHITE, bg);
    tft.drawString(id == MINUS ? "-" : "+", cx, cy, 4);
}

static void drawButton(int id, bool pressed = false) {
    drawButtonBody(id, pressed);
    if (id == focus) drawFocus(id, true);  // its corners fall inside the rect just drawn over
}

// The colour at its intensity (shown no darker than 25 % so it stays readable) and the %.
static void drawLevel() {
    const Look& l = cur();
    const Swatch& s = SWATCHES[l.swatch];
    const int shade = l.pct < 25 ? 25 : l.pct;
    const uint16_t bg = l.lit ? c565At(s.screen, shade) : grey(26);
    tft.fillRoundRect(LEVEL.x, LEVEL.y, LEVEL.w, LEVEL.h, 10, bg);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", l.pct);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(l.lit ? (bright(s.screen, shade) ? TFT_BLACK : TFT_WHITE) : TFT_DARKGREY, bg);
    tft.drawString(l.lit ? pct : "Off", LEVEL.x + LEVEL.w / 2, LEVEL.y + LEVEL.h / 2, 4);
}

static void drawGrid() {
    for (int id = TILE0; id < TILE_END; id++) drawButton(id);
}

static void drawZones() {
    for (int id = ZONE0; id < TILE0; id++) drawButton(id);
}

// The focus ring sits in the gap around the button (white is "what the lights show", yellow
// "pressed"); its corners dip into the button's rect, so drawButton() redraws it.
static void drawFocus(int id, bool on) {
    if (id < 0) return;
    const Rect r = rectOf(id);
    const uint16_t c = on ? TFT_CYAN : TFT_BLACK;
    tft.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, 11, c);
    tft.drawRoundRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, 10, c);
}

static void drawAll() {
    tft.fillScreen(TFT_BLACK);
    for (int id = 0; id < N_BTN; id++) drawButton(id);
    drawLevel();
    drawFocus(focus, true);
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

// NVS keys per look: sw0/pct0/lit0 (BOTH), ...1 (INSIDE), ...2 (OUTSIDE), and the page.
// save() only marks the state dirty; flushSave() (from loop) writes what changed, SAVE_AFTER_MS
// after the last change: a burst of +/- taps is one write, not 10 commits per tap. A change in
// the last SAVE_AFTER_MS before the power goes is lost.
static constexpr uint32_t SAVE_AFTER_MS = 1500;
static Look savedLooks[3];
static Target savedTarget = BOTH;
static bool dirty = false;
static uint32_t dirtyAt = 0;

static void save() {
    dirty = true;
    dirtyAt = millis();
}

static void flushSave() {
    if (!dirty || millis() - dirtyAt < SAVE_AFTER_MS) return;
    dirty = false;
    char key[8];
    for (int i = 0; i < 3; i++) {
        const Look& l = looks[i];
        Look& s = savedLooks[i];
        if (l.swatch != s.swatch) {
            snprintf(key, sizeof(key), "sw%d", i);
            prefs.putUChar(key, static_cast<uint8_t>(l.swatch));
        }
        if (l.pct != s.pct) {
            snprintf(key, sizeof(key), "pct%d", i);
            prefs.putUChar(key, l.pct);
        }
        if (l.lit != s.lit) {
            snprintf(key, sizeof(key), "lit%d", i);
            prefs.putBool(key, l.lit);
        }
        s = l;
    }
    if (target != savedTarget) {
        prefs.putUChar("target", static_cast<uint8_t>(target));
        savedTarget = target;
    }
}

// Builds before the pages kept one look under swatch/pct/lit: it seeds all three.
static void load() {
    Look old;
    old.swatch = prefs.getUChar("swatch", static_cast<uint8_t>(old.swatch)) % N_SWATCH;
    old.pct = static_cast<uint8_t>((prefs.getUChar("pct", old.pct) + 5) / 10 * 10);  // older builds stored any %
    old.lit = prefs.getBool("lit", true);
    char key[8];
    for (int i = 0; i < 3; i++) {
        Look& l = looks[i];
        snprintf(key, sizeof(key), "sw%d", i);
        l.swatch = prefs.getUChar(key, static_cast<uint8_t>(old.swatch)) % N_SWATCH;
        snprintf(key, sizeof(key), "pct%d", i);
        l.pct = prefs.getUChar(key, old.pct);
        if (l.pct < MIN_INTENSITY || l.pct > 100) l.pct = 100;
        snprintf(key, sizeof(key), "lit%d", i);
        l.lit = prefs.getBool(key, old.lit);
        savedLooks[i] = l;
    }
    target = savedTarget = static_cast<Target>(prefs.getUChar("target", BOTH) % 3);
}

// One settled tap on a button.
static void press(int id) {
    Look& l = cur();
    if (id < TILE0) {  // another page: show its look
        target = static_cast<Target>(id);
        drawZones();
        drawGrid();
        drawLevel();
    } else if (id < TILE_END) {
        // A second tap on the lit colour within DOUBLE_MS turns the lights off; any other tap sets
        // the colour and turns them on.
        static constexpr uint32_t DOUBLE_MS = 450;
        static int lastTile = -1;
        static uint32_t lastTileMs = 0;
        const int tile = id - TILE0;
        const bool doubleTap = l.lit && tile == l.swatch && tile == lastTile && millis() - lastTileMs < DOUBLE_MS;
        lastTile = doubleTap ? -1 : tile;  // a third tap starts over
        lastTileMs = millis();
        if (doubleTap) {
            l.lit = false;
        } else {
            l.swatch = tile;
            l.lit = true;
        }
        pushTargets();
        drawGrid();
        drawLevel();
        drawZones();  // the tab chips follow
    } else {
        const int pct = l.pct + (id == PLUS ? 10 : -10);
        l.pct = static_cast<uint8_t>(pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct);
        pushTargets();
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

// ---- Xbox controller input ----
//   D-pad: move the focus ring (held: repeats; the sticks are ignored)    A: press the focused button
//   B: the BOTH page    X: next page    Y: this page's lights on/off    LB / RB: - / +

// The nearest button from `from` in direction (dx, dy), by centres; straying sideways costs double.
static int neighbour(int from, int dx, int dy) {
    const Rect a = rectOf(from);
    const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int best = from, bestScore = 1 << 30;
    for (int id = 0; id < N_BTN; id++) {
        if (id == from) continue;
        const Rect b = rectOf(id);
        const int vx = b.x + b.w / 2 - ax, vy = b.y + b.h / 2 - ay;
        const int along = dx ? vx * dx : vy * dy;
        if (along <= 0) continue;
        const int across = dx ? (vy < 0 ? -vy : vy) : (vx < 0 ? -vx : vx);
        if (along + 2 * across < bestScore) {
            bestScore = along + 2 * across;
            best = id;
        }
    }
    return best;
}

// The button a controller press is showing yellow, and since when (see padPress).
static constexpr uint32_t PAD_FLASH_MS = 150;
static int padFlash = -1;
static uint32_t padFlashAt = 0;

// Moves the ring to `to` (-1: hides it). Erasing the old ring also notches its button's corners,
// so that button is redrawn.
static void setFocus(int to) {
    const int old = focus;
    if (to == old) return;
    focus = to;
    if (old >= 0) {
        drawFocus(old, false);
        drawButton(old, padFlash == old);
    }
    if (to >= 0) drawFocus(to, true);
}

static void moveFocus(int dx, int dy) { setFocus(neighbour(focus, dx, dy)); }

// A button press on the controller: the press looks (yellow) for PAD_FLASH_MS. One at a time: a
// second press inside that window first restores the earlier button (else -/+ stayed yellow).
static void padPress(int id) {
    if (padFlash >= 0 && padFlash != id) drawButton(padFlash);
    press(id);
    drawButton(id, true);
    padFlash = id;
    padFlashAt = millis();
}

static void handlePad() {
    static bool was = false;
    const bool is = xbox.isConnected() && !xbox.isWaitingForFirstNotification();
    if (is != was) {
        was = is;
        Serial.printf("[pad] %s\n", is ? "connected" : "gone");
        setFocus(is ? TILE0 + cur().swatch : -1);
    }
    if (padFlash >= 0 && millis() - padFlashAt > PAD_FLASH_MS) {
        drawButton(padFlash);
        padFlash = -1;
    }
    if (!is) return;

    const XboxControllerNotificationParser& n = xbox.xboxNotif;
    // D-pad only: João's controller has left-stick drift, which wandered the ring on its own.
    int dx = (n.btnDirRight ? 1 : 0) - (n.btnDirLeft ? 1 : 0);
    int dy = (n.btnDirDown ? 1 : 0) - (n.btnDirUp ? 1 : 0);
    if (dx && dy) dy = 0;  // diagonals: horizontal wins
    static int lastDx = 0, lastDy = 0;
    static uint32_t repeatAt = 0;
    if (dx || dy) {
        if (dx != lastDx || dy != lastDy) {
            moveFocus(dx, dy);
            repeatAt = millis() + 400;
        } else if (static_cast<int32_t>(millis() - repeatAt) >= 0) {
            moveFocus(dx, dy);
            repeatAt = millis() + 150;
        }
    }
    lastDx = dx;
    lastDy = dy;

    // buttons act on the press, once
    static bool pA = false, pB = false, pX = false, pY = false, pLB = false, pRB = false;
    auto edge = [](bool now, bool& prev) {
        const bool down = now && !prev;
        prev = now;
        return down;
    };
    if (edge(n.btnA, pA)) padPress(focus);
    if (edge(n.btnB, pB)) padPress(ZONE0 + BOTH);
    if (edge(n.btnX, pX)) padPress(ZONE0 + (target + 1) % 3);
    if (edge(n.btnLB, pLB)) padPress(MINUS);
    if (edge(n.btnRB, pRB)) padPress(PLUS);
    if (edge(n.btnY, pY)) {
        cur().lit = !cur().lit;
        pushTargets();
        drawGrid();
        drawLevel();
        drawZones();
        save();
    }
}

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"). Without a zone it acts like the BOTH page (Siri: spoken zone words are
// ignored); &zone=inside|outside (the Garmin app) acts on that zone's page only.
// GET /state?k=[&zone=] answers that zone's line (level included) without changing anything.
// The server runs in its own task; commands go to loop() through a queue (it owns the screen and
// the state) and the handler waits for loop()'s reply. Each job carries a sequence number that the
// reply echoes, so a request that timed out (loop busy, e.g. calibrating) can't answer the next
// one; a timed-out job is dropped from the queue rather than applied late.

struct VoiceJob {
    ledvoice::Command c;
    Target zone;  // BOTH unless the request named one (zone=inside|outside)
    uint32_t seq;
};

static WebServer* server = nullptr;
static TaskHandle_t voiceHandle = nullptr;
static remotegate::Gate gate(REMOTE_TOKEN);
static QueueHandle_t voiceQueue = nullptr;
static SemaphoreHandle_t voiceDone = nullptr;
static portMUX_TYPE voiceMux = portMUX_INITIALIZER_UNLOCKED;
static char voiceReply[64];  // guarded by voiceMux, with replySeq
static uint32_t replySeq = 0;

// A zone's look as one line ("Purple, 70 percent", "Inside: Lights off"). withLevel: say the %
// too; a voice reply only does when the command changed it ("Purple, 20 percent" after a plain
// "purple" sounded like the deck wanted a percentage).
static void describe(char* out, size_t n, Target zone = BOTH, bool withLevel = true) {
    static const char* prefix[] = {"", "Inside: ", "Outside: "};
    const Look& l = looks[zone];
    if (!l.lit) snprintf(out, n, "%sLights off", prefix[zone]);
    else if (withLevel) snprintf(out, n, "%s%s, %u percent", prefix[zone], SWATCHES[l.swatch].name, l.pct);
    else snprintf(out, n, "%s%s", prefix[zone], SWATCHES[l.swatch].name);
}

// In loop(): applies one voice command to its zone's page, like a tap there: BOTH (no zone given)
// copies to both lights; INSIDE or OUTSIDE changes only that light. The page on screen stays.
// Then answers the waiting handler.
static void applyVoice(const VoiceJob& job) {
    const ledvoice::Command& c = job.c;
    Look& l = looks[job.zone];
    if (c.color >= 0) {
        l.swatch = c.color;
        l.lit = true;
    }
    if (c.power >= 0) l.lit = c.power == 1;
    int pct = c.level >= 0 ? c.level : l.pct + 10 * c.levelStep;
    l.pct = static_cast<uint8_t>(pct < MIN_INTENSITY ? MIN_INTENSITY : pct > 100 ? 100 : pct);
    if (job.zone == BOTH) {
        looks[INSIDE] = looks[OUTSIDE] = l;
        for (int i = 0; i < N_CTRL; i++) push(i);
    } else {
        push(job.zone - 1);
    }
    save();
    drawAll();
    char line[64];
    describe(line, sizeof(line), job.zone, c.level >= 0 || c.levelStep != 0);
    Serial.printf("[voice] -> %s\n", line);
    portENTER_CRITICAL(&voiceMux);
    memcpy(voiceReply, line, sizeof(line));
    replySeq = job.seq;
    portEXIT_CRITICAL(&voiceMux);
    xSemaphoreGive(voiceDone);
}

static void handleSay() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    const String text = server->arg("text");
    Serial.printf("[voice] \"%s\"\n", text.c_str());
    if (text.length() == 0) { server->send(200, "text/plain", "I didn't hear anything"); return; }  // no text from the Shortcut
    VoiceJob job;
    if (!ledvoice::parse(text.c_str(), &job.c)) { server->send(200, "text/plain", "Sorry, I didn't get a colour or a command"); return; }
    static uint32_t nextSeq = 0;
    const String zone = server->arg("zone");
    job.zone = zone == "inside" ? INSIDE : zone == "outside" ? OUTSIDE : BOTH;
    job.seq = ++nextSeq;
    xQueueReset(voiceQueue);        // a job left by a timed-out request is not applied late
    xSemaphoreTake(voiceDone, 0);   // nor is its stale "done"
    if (xQueueSend(voiceQueue, &job, 0) != pdTRUE) {
        server->send(503, "text/plain", "The deck is busy, try again");
        return;
    }
    char line[64] = "";
    const uint32_t until = millis() + 1500;
    bool answered = false;
    while (!answered && static_cast<int32_t>(until - millis()) > 0 &&
           xSemaphoreTake(voiceDone, pdMS_TO_TICKS(until - millis())) == pdTRUE) {
        portENTER_CRITICAL(&voiceMux);
        answered = replySeq == job.seq;
        if (answered) memcpy(line, voiceReply, sizeof(line));
        portEXIT_CRITICAL(&voiceMux);
    }
    if (!answered) {
        xQueueReset(voiceQueue);
        server->send(503, "text/plain", "The deck is busy, try again");
        return;
    }
    server->send(200, "text/plain", line);
}

// The zone's current look, level always included: the Garmin remote syncs its screen from it.
static void handleState() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    const String zone = server->arg("zone");
    char line[64];
    describe(line, sizeof(line), zone == "inside" ? INSIDE : zone == "outside" ? OUTSIDE : BOTH);
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
    // The deck stays on the network it joined (the decks' rule since 2026-10-14; docs/BACKLOG.md):
    // in the car only the hotspot exists. At home, a deck that booted while the iPhone hid its
    // hotspot sits on home WiFi, out of the iPhone's reach: open the hotspot screen and restart it.
    // (An every-20 s hunt for the hotspot was tried here on 2026-10-22 and removed: the hidden
    // hotspot was rarely seen, and the scans cost the server and the radio.)
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
    voiceQueue = xQueueCreate(1, sizeof(VoiceJob));
    voiceDone = xSemaphoreCreateBinary();
    server = new WebServer(80);
    server->on("/say", HTTP_ANY, handleSay);
    server->on("/state", HTTP_GET, handleState);
    xTaskCreatePinnedToCore(voiceTask, "voice", 6144, nullptr, 0, &voiceHandle, 1);
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
    load();

    pinMode(PIN_BOOT, INPUT_PULLUP);
    if (!loadCalibration() || digitalRead(PIN_BOOT) == LOW) calibrate();
    drawAll();
    // NimBLE 2 (the Xbox library needs it): init, bonding on for the controller, +9 dBm. Its bonds
    // live in NVS "nimble_bond"; an older NimBLE 1.4 build boot-loops on them (blescan/bletest).
    xbox.begin();
    // 8 KB: the Xbox library's connect, discovery and subscribe (std::function, std::string) run on
    // this task too; the [mem] line shows what's left.
    xTaskCreatePinnedToCore(linkTask, "leds-link", 8192, nullptr, 1, &linkHandle, 0);
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

    VoiceJob voice;
    if (voiceQueue && xQueueReceive(voiceQueue, &voice, 0) == pdTRUE) applyVoice(voice);
    flushSave();

    // every minute: free heap and the least stack each task has had to spare
    static uint32_t memAt = 0;
    if (millis() - memAt > 60000) {
        memAt = millis();
        Serial.printf("[mem] heap %u (min %u)  stack left: link %u, voice %u, loop %u\n", static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getMinFreeHeap()),
                      linkHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(linkHandle)) : 0u,
                      voiceHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(voiceHandle)) : 0u,
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    handlePad();

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

    // A controller that just came up gets its zone's look, so it matches what the deck remembers.
    static bool shown[N_CTRL] = {};
    for (int i = 0; i < N_CTRL; i++) {
        Controller& c = controllers[i];
        const bool up = linkUp(c);
        if (up && c.fresh) {
            c.fresh = false;
            push(i);
        }
        if (up != shown[i]) {
            shown[i] = up;
            drawButton(ZONE0 + i + 1, tap.down && tap.id == ZONE0 + i + 1);
        }
    }
    delay(10);
}
