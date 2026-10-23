// The yellow deck (CYD) as the car's control screen: a RobCo-style terminal (green on black) that
// drives the car's two ELK-family LED controllers. Two screens, laid out by util/deckmenu:
//   Menu (boot): the header, the menu items as [ BRACKETED ] buttons (part 1 builds one, LIGHTS),
//           a row of favourite slots F1-F4 (tap one to apply it), and a strip at the bottom with
//           the clock and each light's state (IN / OUT: ON in its colour, OFF, or -- when the deck
//           has no link to it)
//   Lights: < BACK, the pages BOTH / INSIDE / OUTSIDE (BOTH sets both lights together; INSIDE and
//           OUTSIDE each keep their own look), the status line ("IN ON 50%  OUT --  LINK 1/2"),
//           2x4 colour tiles (tap one to set it and turn on, double-tap the lit one to turn the
//           lights off), the favourite slots (tap: apply; hold: save both lights' looks into it),
//           and  -  [level: the colour at its intensity and the %, OFF, or MIX on BOTH while the
//           two lights differ]  +
// A press acts once it settles (its first ~40 ms averaged), a favourite on release (or, held on
// Lights, saves while held), and a tap in a gap snaps to the nearest button. The iPhone's Bluetooth
// must be off (Settings): each controller takes one connection.
// Each page's colour, intensity and power, the page last shown and the favourites survive a reboot
// (NVS); the deck still boots to the Menu. Touch calibration (resistive panel) runs on first boot,
// or with BOOT held (at power-up, or 2 s any time). Voice and the Garmin watch: GET /say, /state,
// /favs and /status over WiFi (see "Voice" below), on either screen. An Xbox controller (BLE)
// drives it too: the D-pad moves the focus (an inverted bar on the Menu, a green ring on Lights),
// A presses, B goes back to the Menu; on Lights X is the next page, Y on/off, LB / RB - / + (see
// "Xbox controller input" below).
// Night mode (like the black deck): 18:00-06:00 local time, once WiFi has synced the clock, the
// palette goes to termpal's dim variant and the backlight to its night level. Without REMOTE_TOKEN
// there is no WiFi, no clock, and the deck stays in day mode.
// Test hook: building with -D NIGHT_TEST (e.g. PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST") forces night
// mode whatever the time, to check the dimming without waiting for 18:00. Not set in platformio.ini.
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
#include "ui/backlight.h"
#include "companion/touch.h"
#include "net/clock.h"
#include "net/remote_config.h"
#include "net/wifi.h"
#include "util/bothlook.h"
#include "util/daynight.h"
#include "util/deckmenu.h"
#include "util/elkcmd.h"
#include "util/ledfavs.h"
#include "util/ledlook.h"
#include "util/ledvoice.h"
#include "util/lightstatus.h"
#include "util/remotegate.h"
#include "util/termpal.h"
#include "util/touchcal.h"

static TFT_eSPI tft;
static Preferences prefs;

// ---- State ----

using ledlook::Swatch;
using ledlook::SWATCHES;
using ledlook::N_SWATCH;
using ledlook::Look;

// The zone tabs are pages. BOTH is the general page: a change there goes to both lights (the
// inside and outside looks become a copy of it; while the lights differ, util/bothlook keeps each
// one's own colour and level unless the change sets a colour). INSIDE and OUTSIDE each keep their
// own look and change only their light. Voice works like the BOTH page.
enum Target { BOTH, INSIDE, OUTSIDE };
static Target target = BOTH;  // the page on screen

static Look looks[3];  // indexed by Target; controller i shows looks[i + 1]
static Look& cur() { return looks[target]; }
// The BOTH page is on screen while the lights differ (after a favourite or the INSIDE / OUTSIDE
// pages): it shows no selected colour and "MIX" for the level.
static bool bothMixed() { return target == BOTH && bothlook::mixed(looks[INSIDE], looks[OUTSIDE]); }

// Four favourites, each a pair of looks (inside, outside). NVS fav0..fav3, written on each save.
static ledfavs::Fav favs[ledfavs::N_FAV];

using deckmenu::Rect;
using deckmenu::Screen;
using deckmenu::M_ITEM0;
using deckmenu::M_PRE0;
using deckmenu::L_BACK;
using deckmenu::L_ZONE0;
using deckmenu::L_TILE0;
using deckmenu::L_PRE0;
using deckmenu::L_MINUS;
using deckmenu::L_PLUS;

static Screen screen = Screen::Menu;  // the screen shown; boot lands on the Menu
// The palette: every colour goes through termpal with this flag, so night mode only flips it and
// redraws.
static bool night = false;

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

// One change on `page` (a tap, Y, voice): on BOTH it goes to both lights (bothlook), on INSIDE or
// OUTSIDE to that light's look only; then the lights it covers get their looks.
static void change(Target page, const bothlook::Change& c) {
    if (page == BOTH) bothlook::apply(c, &looks[BOTH], &looks[INSIDE], &looks[OUTSIDE]);
    else bothlook::edit(&looks[page], c);
    for (int i = 0; i < N_CTRL; i++) {
        if (page == BOTH || page == static_cast<Target>(i + 1)) push(i);
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

// ---- Buttons (320x240 landscape, laid out by util/deckmenu) ----

// The grid is all colours; there is no power tile: double-tapping the lit colour turns the lights
// off, and tapping any colour turns them back on.
static_assert(N_SWATCH == 8 && L_PRE0 - L_TILE0 == N_SWATCH, "the grid is 2x4 colours");

static bool built(int id) { return deckmenu::rect(screen, id).w > 0; }

// The button the Xbox controller points at; -1 while none is connected. On the Menu it is drawn as
// an inverted bar, on Lights as a green ring around the button.
static int focus = -1;
static bool padUp = false;  // an Xbox controller is connected and reporting
static void drawFocus(int id, bool on);

// Where the focus starts on each screen: the first item, or the colour the page shows.
static int homeFocus() { return screen == Screen::Menu ? static_cast<int>(M_ITEM0) : L_TILE0 + cur().swatch; }

// ---- Drawing ----
// GLCD (font 1) is the terminal's monospace font, scaled up where it fits; button labels use font
// 2 (and 4 for - / + and the level), as GLCD at x2 is wider than deckmenu's buttons.

static uint16_t c565(uint32_t rgb) { return tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static uint16_t pal(termpal::Role r) { return c565(termpal::rgb(r, night)); }
// A real colour (a tile, a light preview, the pressed yellow), dimmed with the palette at night.
static uint16_t tint(uint32_t rgb) { return c565(termpal::tile(rgb, night)); }
static constexpr uint32_t PRESSED = 0xFFFF00;  // under the finger, until release
// The screen colour at a share of its brightness (for the level box).
static uint16_t tintAt(uint32_t rgb, int pct) {
    const uint32_t c = termpal::tile(rgb, night);
    return tft.color565(((c >> 16) & 0xFF) * pct / 100, ((c >> 8) & 0xFF) * pct / 100, (c & 0xFF) * pct / 100);
}
static bool bright(uint32_t rgb, int pct = 100) {
    return (((rgb >> 16) & 0xFF) * 3 + ((rgb >> 8) & 0xFF) * 6 + (rgb & 0xFF)) / 10 * pct / 100 > 140;
}
// Text on a real colour: black on a bright one, white on a dark one. Judged on the colour actually
// drawn (dimmed at night), not the day one, or a tile that dimmed below the threshold keeps black ink.
static uint16_t inkOn(uint32_t rgb, int pct = 100) {
    return tint(bright(termpal::tile(rgb, night), pct) ? 0x000000 : 0xFFFFFF);
}

// drawString at a text size, back to size 1 after (fonts 2 and 4 scale with it too).
static void label(const char* s, int x, int y, uint8_t fontNo, uint8_t size = 1) {
    tft.setTextSize(size);
    tft.drawString(s, x, y, fontNo);
    tft.setTextSize(1);
}

// A terminal button: a MID border with TEXT inside; inverted (TEXT fill, BG text) when selected,
// yellow when pressed. The fill leaves a 1 px BG edge so a focus ring around it stays apart.
static void drawChrome(const Rect& r, const char* s, uint8_t fontNo, bool pressed, bool inverted) {
    const uint16_t bg = pal(termpal::BG);
    tft.fillRect(r.x, r.y, r.w, r.h, bg);
    uint16_t ink = pal(termpal::TEXT), fill = bg;
    if (pressed || inverted) {
        fill = pressed ? tint(PRESSED) : pal(termpal::TEXT);
        ink = bg;
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, fill);
    } else {
        tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::MID));
        tft.drawRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, pal(termpal::MID));
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink, fill);
    label(s, r.x + r.w / 2, r.y + r.h / 2, fontNo);
}

// The slot (0..3) shown TEXT-green for FAV_FLASH_MS after a save, since favFlashAt (loop ends it).
static constexpr uint32_t FAV_FLASH_MS = 300;
static int favFlash = -1;
static uint32_t favFlashAt = 0;

// A favourite slot: "F1", then per light (inside, outside) a square in its colour (an outline when
// off) over its level ("50", or OFF). An empty one reads "F3 --" in DIM. Inverted under the Menu's
// focus bar, yellow when pressed, green while flashing after a save.
static void drawFavSlot(const Rect& r, int slot, bool pressed, bool inverted) {
    const ledfavs::Fav& f = favs[slot];
    const uint16_t bg = pal(termpal::BG);
    tft.fillRect(r.x, r.y, r.w, r.h, bg);
    uint16_t fill = bg, ink = pal(f.set ? termpal::TEXT : termpal::DIM);
    const bool filled = slot == favFlash || pressed || inverted;
    if (filled) {
        fill = pressed && slot != favFlash ? tint(PRESSED) : pal(termpal::TEXT);
        ink = bg;
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, fill);
    } else {
        tft.drawRect(r.x, r.y, r.w, r.h, pal(f.set ? termpal::MID : termpal::DIM));
    }
    const int cy = r.y + r.h / 2;
    char s[16];
    snprintf(s, sizeof(s), "F%d%s", slot + 1, f.set ? "" : " --");
    tft.setTextColor(ink, fill);
    if (!f.set) {
        tft.setTextDatum(MC_DATUM);
        label(s, r.x + r.w / 2, cy, 1, 2);
        return;
    }
    tft.setTextDatum(ML_DATUM);
    label(s, r.x + 5, cy, 1, 2);
    // two 20 px columns right of the name; "100" in GLCD is 18 px
    tft.setTextDatum(MC_DATUM);
    const Look* sides[] = {&f.in, &f.out};
    for (int k = 0; k < 2; k++) {
        const Look& l = *sides[k];
        const int cx = r.x + 42 + k * 21;
        if (l.lit) {
            tft.fillRect(cx - 5, cy - 11, 10, 10, tint(SWATCHES[l.swatch].screen));
            if (filled) tft.drawRect(cx - 6, cy - 12, 12, 12, bg);  // apart from the fill (green on green)
            snprintf(s, sizeof(s), "%u", l.pct);
        } else {
            tft.drawRect(cx - 5, cy - 11, 10, 10, filled ? ink : pal(termpal::DIM));
            snprintf(s, sizeof(s), "OFF");
        }
        tft.setTextColor(filled || l.lit ? ink : pal(termpal::DIM), fill);
        label(s, cx, cy + 7, 1);
    }
}

static void drawTile(const Rect& r, int swatch, bool pressed) {
    const Swatch& s = SWATCHES[swatch];
    const bool sel = !bothMixed() && cur().lit && swatch == cur().swatch;
    const int in = pressed ? 5 : 1;  // pressed: the colour shrinks inside a yellow ring
    const uint16_t fill = tint(s.screen);
    tft.fillRect(r.x, r.y, r.w, r.h, pal(termpal::BG));
    tft.fillRoundRect(r.x + in, r.y + in, r.w - 2 * in, r.h - 2 * in, 8, fill);
    if (pressed || sel) {  // a green ring: what the lights show
        const uint16_t ring = pressed ? tint(PRESSED) : pal(termpal::TEXT);
        for (int k = 0; k < 3; k++) tft.drawRoundRect(r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k, 8, ring);
        if (!pressed) tft.drawRoundRect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, 6, pal(termpal::BG));
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(inkOn(s.screen), fill);
    label(s.name, r.x + r.w / 2, r.y + r.h / 2, 2);
}

static void drawButtonBody(int id, bool pressed) {
    const Rect r = deckmenu::rect(screen, id);
    if (r.w <= 0) return;
    if (screen == Screen::Menu) {
        static const char* const ITEM[deckmenu::N_ITEMS] = {"[ LIGHTS ]"};  // in deckmenu::Item order
        if (id < M_PRE0) drawChrome(r, ITEM[id - M_ITEM0], 2, pressed, id == focus);
        else drawFavSlot(r, deckmenu::presetSlot(screen, id), pressed, id == focus);
        return;
    }
    if (id == L_BACK) {
        drawChrome(r, "< BACK", 2, pressed, false);
    } else if (id < L_TILE0) {
        static const char* const ZONE[] = {"BOTH", "INSIDE", "OUTSIDE"};
        drawChrome(r, ZONE[id - L_ZONE0], 2, pressed, target == static_cast<Target>(id - L_ZONE0));
    } else if (id < L_PRE0) {
        drawTile(r, id - L_TILE0, pressed);
    } else if (id < L_MINUS) {
        drawFavSlot(r, deckmenu::presetSlot(screen, id), pressed, false);  // focus: the ring
    } else {
        drawChrome(r, id == L_MINUS ? "-" : "+", 4, pressed, false);
    }
}

static void drawButton(int id, bool pressed = false) {
    drawButtonBody(id, pressed);
    if (id == focus) drawFocus(id, true);  // on Lights its ring hugs the rect just drawn over
}

// The colour at its intensity (shown no darker than 25 % so it stays readable) and the %; "MIX"
// on BOTH while the lights differ (the STATUS line above has each light's %).
static void drawLevel() {
    const Rect& r = deckmenu::LEVEL;
    const Look& l = cur();
    const Swatch& s = SWATCHES[l.swatch];
    const int shade = l.pct < 25 ? 25 : l.pct;
    const uint16_t bg = pal(termpal::BG);
    tft.fillRect(r.x, r.y, r.w, r.h, bg);
    tft.setTextDatum(MC_DATUM);
    if (bothMixed()) {
        tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::MID));
        tft.setTextColor(pal(termpal::TEXT), bg);
        label("MIX", r.x + r.w / 2, r.y + r.h / 2, 4);
        return;
    }
    uint16_t fill = bg, ink = pal(termpal::DIM);
    if (l.lit) {
        fill = tintAt(s.screen, shade);
        ink = inkOn(s.screen, shade);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
    } else {
        tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::DIM));
    }
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", l.pct);
    tft.setTextColor(ink, fill);
    label(l.lit ? pct : "OFF", r.x + r.w / 2, r.y + r.h / 2, 4);
}

// Lights: the line under the tabs, "IN ON 50%  OUT --  LINK 1/2".
static void drawStatus() {
    const Rect& r = deckmenu::STATUS;
    char line[48];
    lightstatus::line(looks[INSIDE], linkUp(controllers[0]), looks[OUTSIDE], linkUp(controllers[1]), line, sizeof(line));
    tft.fillRect(r.x, r.y, r.w, r.h, pal(termpal::BG));
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT), pal(termpal::BG));
    label(line, r.x + 2, r.y + r.h / 2, 1);
}

// Menu strip: one light's name and its state in a box: ON on the light's colour, OFF or -- in DIM.
static void drawStripLight(int i, int x, int boxX) {
    static const char* const NAME[] = {"IN", "OUT"};
    const Rect& r = deckmenu::MENU_STRIP;
    const int cy = r.y + r.h / 2;
    const uint16_t bg = pal(termpal::BG);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::MID), bg);
    label(NAME[i], x, cy, 1, 2);
    const Look& l = looks[i + 1];
    const lightstatus::State st = lightstatus::of(linkUp(controllers[i]), l.lit);
    const int bw = 52, bh = 28, by = cy - bh / 2;
    uint16_t fill = bg, ink = pal(termpal::DIM);
    tft.fillRect(boxX, by, bw, bh, bg);
    if (st == lightstatus::State::On) {
        const uint32_t c = SWATCHES[l.swatch].screen;
        fill = tint(c);
        ink = inkOn(c);
        tft.fillRect(boxX, by, bw, bh, fill);
    } else {
        tft.drawRect(boxX, by, bw, bh, pal(termpal::DIM));
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink, fill);
    label(st == lightstatus::State::On ? "ON" : st == lightstatus::State::Off ? "OFF" : "--", boxX + bw / 2, cy, 1, 2);
}

static void drawStripLights() {
    drawStripLight(0, 124, 154);
    drawStripLight(1, 216, 258);
}

// The clock's minute of the day on screen (-1: not synced, "--:--"); updateDayNight keeps it.
static int clockMin = -1;

static void drawClock() {
    const Rect& r = deckmenu::MENU_STRIP;
    char s[16];  // room for any int, so -Wformat-truncation stays quiet
    if (clockMin < 0) snprintf(s, sizeof(s), "--:--");
    else snprintf(s, sizeof(s), "%02d:%02d", clockMin / 60, clockMin % 60);
    tft.fillRect(r.x + 4, r.y + 2, 110, r.h - 2, pal(termpal::BG));  // left of the IN label, under the rule
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT), pal(termpal::BG));
    label(s, r.x + 8, r.y + r.h / 2, 1, 3);
}

static void drawStrip() {
    const Rect& r = deckmenu::MENU_STRIP;
    tft.fillRect(r.x, r.y, r.w, r.h, pal(termpal::BG));
    tft.drawFastHLine(r.x + 4, r.y + 1, r.w - 8, pal(termpal::MID));
    drawClock();
    drawStripLights();
}

static void drawHeader() {
    const Rect& r = deckmenu::HEADER;
    tft.fillRect(r.x, r.y, r.w, r.h, pal(termpal::BG));
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(pal(termpal::TEXT), pal(termpal::BG));
    label("ROBCO INDUSTRIES (TM) TERMLINK", r.x + r.w / 2, r.y + 6, 1);
    tft.setTextColor(pal(termpal::MID), pal(termpal::BG));
    label("HONDA CITY // CARDECK OS", r.x + r.w / 2, r.y + 18, 1);
    tft.drawFastHLine(r.x + 4, r.y + r.h - 3, r.w - 8, pal(termpal::MID));
}

static void drawGrid() {
    for (int id = L_TILE0; id < L_PRE0; id++) drawButton(id);
}

static void drawZones() {
    for (int id = L_ZONE0; id < L_TILE0; id++) drawButton(id);
}

// What shows the lights' state on the current screen: Lights' STATUS line, the Menu's IN / OUT.
static void drawLightState() {
    if (screen == Screen::Lights) drawStatus();
    else drawStripLights();
}

// After a look changed (a tap, voice, the watch, Y): everything that shows it.
static void drawLooks() {
    if (screen == Screen::Lights) {
        drawGrid();
        drawLevel();
    }
    drawLightState();
}

// Lights only (the Menu's focus is the inverted bar, drawn by the button itself). The ring sits in
// the gap around the button (green inside it marks what the lights show, yellow "pressed").
static void drawFocus(int id, bool on) {
    if (id < 0 || screen != Screen::Lights) return;
    const Rect r = deckmenu::rect(screen, id);
    const uint16_t c = on ? pal(termpal::TEXT) : pal(termpal::BG);
    tft.drawRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, c);
    tft.drawRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, c);
}

// Redraws the current screen in the current palette.
static void drawAll() {
    tft.fillScreen(pal(termpal::BG));
    if (screen == Screen::Menu) drawHeader();
    for (int id = 0; id < deckmenu::count(screen); id++) {
        if (built(id)) drawButton(id);
    }
    if (screen == Screen::Menu) {
        drawStrip();
    } else {
        drawLevel();
        drawStatus();
    }
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
        if (l.pct < ledlook::MIN_PCT || l.pct > 100) l.pct = 100;
        snprintf(key, sizeof(key), "lit%d", i);
        l.lit = prefs.getBool(key, old.lit);
        savedLooks[i] = l;
    }
    target = savedTarget = static_cast<Target>(prefs.getUChar("target", BOTH) % 3);
    for (int i = 0; i < ledfavs::N_FAV; i++) {  // missing or invalid: an empty slot
        snprintf(key, sizeof(key), "fav%d", i);
        uint8_t b[ledfavs::BYTES];
        favs[i] = {};
        if (prefs.isKey(key) && prefs.getBytesLength(key) == sizeof(b) && prefs.getBytes(key, b, sizeof(b)) == sizeof(b)) {
            ledfavs::decode(b, &favs[i]);
        }
    }
}

// The favourite slot's button on the current screen.
static int favId(int slot0) { return (screen == Screen::Menu ? static_cast<int>(M_PRE0) : L_PRE0) + slot0; }

// Favourite slot0 (0..3) onto the lights: each light gets its saved look. The BOTH page copies
// them when they match; otherwise it takes the inside look, lit if either light is (bothlook::seed),
// shows "MIX", and the Lights page's STATUS line tells the truth. An empty slot does nothing.
static void applyFav(int slot0) {
    const ledfavs::Fav& f = favs[slot0];
    if (!f.set) return;
    looks[INSIDE] = f.in;
    looks[OUTSIDE] = f.out;
    looks[BOTH] = bothlook::seed(f.in, f.out);
    for (int i = 0; i < N_CTRL; i++) push(i);
    save();
    drawLooks();
}

// Both lights' looks into slot0, written at once (saves are rare, unlike +/-); the slot flashes.
static void saveFav(int slot0) {
    favs[slot0] = {true, looks[INSIDE], looks[OUTSIDE]};
    uint8_t b[ledfavs::BYTES];
    ledfavs::encode(favs[slot0], b);
    char key[8];
    snprintf(key, sizeof(key), "fav%d", slot0);
    prefs.putBytes(key, b, sizeof(b));
    Serial.printf("[fav] saved F%d\n", slot0 + 1);
    favFlash = slot0;
    favFlashAt = millis();
    drawButton(favId(slot0));
}

// A press is averaged over its first SETTLE_MS (a fingertip's first readings jump around) and acts
// once; a tap shorter than that acts on release with what it had.
// A favourite acts on release instead (see loop), and `held` marks one that saved while held.
static constexpr uint32_t SETTLE_MS = 40;
static struct {
    bool down = false, acted = false, held = false;
    uint32_t t0 = 0;
    long sx = 0, sy = 0;
    int n = 0, id = -1;  // id: the button drawn pressed
} tap;

// The button a controller press is showing yellow, and since when (see padPress).
static constexpr uint32_t PAD_FLASH_MS = 150;
static int padFlash = -1;
static uint32_t padFlashAt = 0;
// A favourite under a held A (it acts on release, see handlePad), since padFavAt; padFavSaved once
// the hold saved it.
static int padFav = -1;
static uint32_t padFavAt = 0;
static bool padFavSaved = false;

// Whether button `id` is under a finger or a controller press right now (drawn yellow).
static bool pressedNow(int id) { return (tap.down && tap.id == id) || padFlash == id || padFav == id; }

// Switches to screen `s`: the focus goes to its first button (none without a controller), and a
// press still drawn yellow is forgotten (its id means another button there).
static void show(Screen s) {
    screen = s;
    tap.id = -1;
    padFlash = -1;
    padFav = -1;
    focus = padUp ? homeFocus() : -1;
    drawAll();
}

// One settled tap on a Lights button.
static void pressLights(int id) {
    Look& l = cur();
    if (id == L_BACK) {
        show(Screen::Menu);
        return;
    }
    if (deckmenu::isPreset(screen, id)) return;  // favourites act on release (loop, handlePad)
    if (id < L_TILE0) {  // another page: show its look
        target = static_cast<Target>(id - L_ZONE0);
        drawZones();
        drawGrid();
        drawLevel();
    } else if (id < L_PRE0) {
        // A second tap on the lit colour within DOUBLE_MS turns the lights off; any other tap sets
        // the colour and turns them on.
        static constexpr uint32_t DOUBLE_MS = 450;
        static int lastTile = -1;
        static uint32_t lastTileMs = 0;
        const int tile = id - L_TILE0;
        const bool doubleTap = l.lit && tile == l.swatch && tile == lastTile && millis() - lastTileMs < DOUBLE_MS;
        lastTile = doubleTap ? -1 : tile;  // a third tap starts over
        lastTileMs = millis();
        bothlook::Change c;
        if (doubleTap) c.power = 0;
        else c.swatch = tile;
        change(target, c);
        drawLooks();
    } else {  // L_MINUS / L_PLUS
        bothlook::Change c;
        c.step = id == L_PLUS ? 1 : -1;
        const bool wasMixed = bothMixed();
        change(target, c);
        if (bothMixed() != wasMixed) drawGrid();  // a step can bring mixed lights to one look
        drawLevel();
        drawStatus();
    }
    save();
}

// One settled tap on a button of the current screen.
static void press(int id) {
    if (screen == Screen::Lights) {
        pressLights(id);
        return;
    }
    if (id < M_ITEM0 || id >= M_ITEM0 + deckmenu::N_ITEMS) return;  // favourites act on release
    switch (static_cast<deckmenu::Item>(id - M_ITEM0)) {
    case deckmenu::LIGHTS: show(Screen::Lights); break;
    }
}

static void act() {
    tap.acted = true;
    const int x = static_cast<int>(tap.sx / tap.n), y = static_cast<int>(tap.sy / tap.n);
    tap.id = deckmenu::hit(screen, x, y);
    Serial.printf("[tap] %d,%d -> button %d\n", x, y, tap.id);
    if (tap.id < 0) return;
    if (deckmenu::isPreset(screen, tap.id)) {  // pressed look only: it acts on release, or saves when held
        drawButton(tap.id, true);
        return;
    }
    const Screen was = screen;
    press(tap.id);
    if (screen == was) drawButton(tap.id, true);  // else show() redrew all and cleared tap.id
}

// ---- Xbox controller input ----
//   D-pad: move the focus (held: repeats; the sticks are ignored)    A: press the focused button
//   (a favourite applies when A comes up; held PAD_HOLD_MS on Lights, it saves instead)
//   B: back to the Menu (on the Menu: nothing)
//   Lights only: X next page    Y this page's lights on/off    LB / RB: - / +

// Moves the focus to `to` (-1: hides it). On the Menu the bar is part of the button, so both are
// redrawn. On Lights, erasing the old ring can notch its button's edge, so that button is redrawn.
static void setFocus(int to) {
    const int old = focus;
    if (to == old) return;
    focus = to;
    if (screen == Screen::Menu) {
        if (old >= 0) drawButton(old, pressedNow(old));
        if (to >= 0) drawButton(to, pressedNow(to));
        return;
    }
    if (old >= 0) {
        drawFocus(old, false);
        drawButton(old, pressedNow(old));
    }
    if (to >= 0) drawFocus(to, true);
}

// focus is always a built id of the current screen (deckmenu::neighbour can't start from another).
static void moveFocus(int dx, int dy) {
    if (focus >= 0) setFocus(deckmenu::neighbour(screen, focus, dx, dy));
}

// A button press on the controller: the press looks (yellow) for PAD_FLASH_MS. One at a time: a
// second press inside that window first restores the earlier button (else -/+ stayed yellow).
static void padPress(int id) {
    if (id < 0 || deckmenu::isPreset(screen, id)) return;  // favourites: padFavDown, on release
    if (padFlash >= 0 && padFlash != id) drawButton(padFlash);
    const Screen was = screen;
    press(id);
    if (screen != was) return;  // show() redrew the new screen
    drawButton(id, true);
    padFlash = id;
    padFlashAt = millis();
}

// A on a favourite: drawn pressed; it acts when A comes up (handlePad), or saves once held
// PAD_HOLD_MS on Lights.
static constexpr uint32_t PAD_HOLD_MS = 1000;
static constexpr uint32_t HOLD_MS = 800;  // touch hold on a Lights favourite saves it (removed with the save logic)
static void padFavDown(int id) {
    if (padFlash >= 0) {
        drawButton(padFlash);
        padFlash = -1;
    }
    padFav = id;
    padFavAt = millis();
    padFavSaved = false;
    drawButton(id, true);
}

static void handlePad() {
    const bool is = xbox.isConnected() && !xbox.isWaitingForFirstNotification();
    if (is != padUp) {
        padUp = is;
        Serial.printf("[pad] %s\n", is ? "connected" : "gone");
        if (!is && padFav >= 0) {  // gone with A held: nothing applies
            const int id = padFav;
            padFav = -1;
            drawButton(id);
        }
        setFocus(is ? homeFocus() : -1);
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

    // buttons act on the press, once; every edge is taken on both screens, so a button held across
    // a screen change doesn't fire on the next one
    static bool pA = false, pB = false, pX = false, pY = false, pLB = false, pRB = false;
    auto edge = [](bool now, bool& prev) {
        const bool down = now && !prev;
        prev = now;
        return down;
    };
    const bool a = edge(n.btnA, pA), b = edge(n.btnB, pB), x = edge(n.btnX, pX), y = edge(n.btnY, pY);
    const bool lb = edge(n.btnLB, pLB), rb = edge(n.btnRB, pRB);
    const Screen was = screen;
    if (a) {
        if (deckmenu::isPreset(screen, focus)) padFavDown(focus);
        else padPress(focus);
    }
    if (padFav >= 0) {  // show() clears it, so it is a favourite of this screen
        const int slot = deckmenu::presetSlot(screen, padFav);
        if (!n.btnA) {
            const int id = padFav;
            padFav = -1;
            if (!padFavSaved) applyFav(slot);
            drawButton(id);
        } else if (!padFavSaved && screen == Screen::Lights && millis() - padFavAt >= PAD_HOLD_MS) {
            padFavSaved = true;  // once; the release then does nothing
            saveFav(slot);
        }
    }
    if (was != Screen::Lights || screen != Screen::Lights) return;  // the rest is the Lights page's
    if (b) {
        show(Screen::Menu);
        return;
    }
    if (x) padPress(L_ZONE0 + (target + 1) % 3);
    if (lb) padPress(L_MINUS);
    if (rb) padPress(L_PLUS);
    if (y) {  // on / off; on BOTH while the lights differ, off if either is on
        bothlook::Change c;
        c.power = (bothMixed() ? looks[INSIDE].lit || looks[OUTSIDE].lit : cur().lit) ? 0 : 1;
        change(target, c);
        drawLooks();
        save();
    }
}

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"). Without a zone it acts like the BOTH page (Siri: spoken zone words are
// ignored); &zone=inside|outside (the Garmin app) acts on that zone's page only.
// GET /state?k=[&zone=] answers that zone's line (level included) without changing anything.
// "favourite 2" applies that favourite to both lights and answers its line. GET /favs?k= lists the
// four favourites (one ledfavs::wireLine each), GET /status?k= each light's state.
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
// 112: the longest favourite line ("Favourite 1: inside Orange 100 percent, outside ...") fits.
static char voiceReply[112];  // guarded by voiceMux, with replySeq
static uint32_t replySeq = 0;

// What /state, /favs and /status read (they run on the server task): loop() copies it from its own
// state after every pass (and before each voice reply), under stateMux, so a handler never sees a
// half-written look or favourite.
struct Published {
    Look looks[3];  // indexed by Target
    ledfavs::Fav favs[ledfavs::N_FAV];
    bool up[N_CTRL];
};
static Published published;
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

static void publish() {
    Published p;
    for (int i = 0; i < 3; i++) p.looks[i] = looks[i];
    for (int i = 0; i < ledfavs::N_FAV; i++) p.favs[i] = favs[i];
    for (int i = 0; i < N_CTRL; i++) p.up[i] = linkUp(controllers[i]);
    portENTER_CRITICAL(&stateMux);
    published = p;
    portEXIT_CRITICAL(&stateMux);
}

static Published snapshot() {
    portENTER_CRITICAL(&stateMux);
    const Published p = published;
    portEXIT_CRITICAL(&stateMux);
    return p;
}

// Zone `zone`'s look `l` as one line ("Purple, 70 percent", "Inside: Lights off"). withLevel: say
// the % too; a voice reply only does when the command changed it ("Purple, 20 percent" after a
// plain "purple" sounded like the deck wanted a percentage).
static void describe(char* out, size_t n, const Look& l, Target zone, bool withLevel = true) {
    static const char* prefix[] = {"", "Inside: ", "Outside: "};
    if (!l.lit) snprintf(out, n, "%sLights off", prefix[zone]);
    else if (withLevel) snprintf(out, n, "%s%s, %u percent", prefix[zone], SWATCHES[l.swatch].name, l.pct);
    else snprintf(out, n, "%s%s", prefix[zone], SWATCHES[l.swatch].name);
}

// Hands loop()'s reply line to the waiting handler, tagged with its job's seq.
static void answerVoice(const char (&line)[sizeof(voiceReply)], uint32_t seq) {
    Serial.printf("[voice] -> %s\n", line);
    publish();  // a /state right after this reply already sees the change
    portENTER_CRITICAL(&voiceMux);
    memcpy(voiceReply, line, sizeof(line));
    replySeq = seq;
    portEXIT_CRITICAL(&voiceMux);
    xSemaphoreGive(voiceDone);
}

// In loop(): applies one voice command to its zone's page, like a tap there: BOTH (no zone given)
// goes to both lights (bothlook: while they differ each keeps its own look unless a colour is
// given); INSIDE or OUTSIDE changes only that light; a favourite sets both lights
// whatever the zone. The page on screen stays. Then answers the waiting handler.
static void applyVoice(const VoiceJob& job) {
    const ledvoice::Command& c = job.c;
    char line[sizeof(voiceReply)];
    if (c.preset >= 1 && c.preset <= ledfavs::N_FAV) {  // "favourite 2": both lights, whatever zone was asked
        applyFav(c.preset - 1);
        ledfavs::describe(favs[c.preset - 1], c.preset, line, sizeof(line));
        answerVoice(line, job.seq);
        return;
    }
    bothlook::Change ch;
    ch.swatch = c.color;
    ch.power = c.power;
    ch.level = c.level;
    ch.step = c.levelStep;
    change(job.zone, ch);
    save();
    drawLooks();  // on either screen; the page on Lights stays
    describe(line, sizeof(line), looks[job.zone], job.zone, c.level >= 0 || c.levelStep != 0);
    answerVoice(line, job.seq);
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
    char line[sizeof(voiceReply)] = "";
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
    const Target z = zone == "inside" ? INSIDE : zone == "outside" ? OUTSIDE : BOTH;
    const Published p = snapshot();
    char line[64];
    describe(line, sizeof(line), p.looks[z], z);
    server->send(200, "text/plain", line);
}

// The four favourites, one wireLine each ("1 pink 50 pink 100", "3 -"), joined by \n.
static void handleFavs() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    const Published p = snapshot();
    char body[ledfavs::N_FAV * 32] = "";
    size_t len = 0;
    for (int i = 0; i < ledfavs::N_FAV && len < sizeof(body); i++) {
        char l[32];
        ledfavs::wireLine(p.favs[i], i + 1, l, sizeof(l));
        const int k = snprintf(body + len, sizeof(body) - len, "%s%s", i ? "\n" : "", l);
        if (k > 0) len += static_cast<size_t>(k);
    }
    server->send(200, "text/plain", body);
}

// Each light on, off or out of reach: "inside=on outside=nolink".
static void handleStatus() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    const Published p = snapshot();
    char line[48];
    lightstatus::reply(lightstatus::of(p.up[0], p.looks[INSIDE].lit), lightstatus::of(p.up[1], p.looks[OUTSIDE].lit),
                       line, sizeof(line));
    server->send(200, "text/plain", line);
}

static void voiceTask(void*) {
    net::connectAny();
    while (!net::isOnline()) {
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    server->begin();
    netclock::begin();  // SNTP, for night mode and the Menu's clock
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
    server->on("/favs", HTTP_GET, handleFavs);
    server->on("/status", HTTP_GET, handleStatus);
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

// Night mode and the Menu's clock. Polled each second (a time() call) so the clock turns over on the
// minute; the mode follows the clock. On a change the backlight drops before the repaint, so no
// frame shows at day brightness.
static void updateDayNight() {
    static uint32_t checkedAt = 0;
    static bool first = true;
    if (!first && millis() - checkedAt < 1000) return;
    first = false;
    checkedAt = millis();
    int m = 0;
    const bool synced = netclock::minuteOfDay(&m);
    const int minute = synced ? m : -1;
    bool want = synced && daynight::isNight(m);
#ifdef NIGHT_TEST
    want = true;
#endif
    if (minute != clockMin) {
        if (minute >= 0 && clockMin < 0) Serial.printf("[clock] synced, local %02d:%02d\n", m / 60, m % 60);
        clockMin = minute;
        if (want == night && screen == Screen::Menu) drawClock();
    }
    if (want == night) return;
    night = want;
    backlight::set(night);  // before the repaint
    Serial.printf("[ui] night mode %s\n", night ? "on" : "off");
    drawAll();
}

void setup() {
    Serial.begin(115200);
    tft.init();
    backlight::begin();  // PWM, at the day level
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
    publish();
    beginVoice();
}

void loop() {
    int x, y;
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second.
    const bool down = touch::read(&x, &y) && millis() > 1000;
    if (down) {
        if (!tap.down) tap = {true, false, false, millis(), 0, 0, 0, -1};
        if (!tap.acted) {
            tap.sx += x;
            tap.sy += y;
            tap.n++;
            if (millis() - tap.t0 >= SETTLE_MS && tap.n >= 3) act();
        } else if (!tap.held && screen == Screen::Lights && tap.id >= 0 && deckmenu::isPreset(screen, tap.id) &&
                   millis() - tap.t0 >= HOLD_MS) {
            tap.held = true;  // saved once; the release then does nothing
            saveFav(deckmenu::presetSlot(screen, tap.id));
        }
    } else if (tap.down) {
        if (!tap.acted && tap.n > 0) act();  // a quick tap
        tap.down = false;
        if (tap.id >= 0 && !tap.held && deckmenu::isPreset(screen, tap.id)) applyFav(deckmenu::presetSlot(screen, tap.id));
        if (tap.id >= 0) drawButton(tap.id);  // released: back to the normal look
    }

    if (favFlash >= 0 && millis() - favFlashAt > FAV_FLASH_MS) {
        const int id = favId(favFlash);
        favFlash = -1;
        drawButton(id, pressedNow(id));
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
    updateDayNight();

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
            drawLightState();  // STATUS on Lights, IN / OUT on the Menu: "--" while it's gone
        }
    }
    publish();  // for /favs and /status
    delay(10);
}
