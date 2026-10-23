// The yellow deck (CYD) as the car's control screen: a RobCo-style terminal (green on black, amber
// at night, faint scanlines, all caps) that drives the car's two ELK-family LED controllers. Four
// screens, laid out by util/deckmenu:
//   Menu (boot, after the header types out): the header, the menu items side by side on one row
//           ("> [LIGHTS]", "> [WEATHER]", "> [KARAOKE]", the last dimmed while the black deck plays nothing
//           with synced lyrics: a tap then only says why), the four presets ROSE / FIRE / CALM / PURPURA (tap one
//           to apply it), and a strip at the bottom with the clock (the outside temperature under
//           it, "24C" or "--C") and each light's state ("IN PINK 40% OUT --": its colour, a chip of
//           it and its %, OFF, or -- when the deck has no link to it) ending in a blinking block
//           cursor
//   Lights: < BACK, the pages BOTH / INSIDE / OUTSIDE as Pip-Boy tabs (BOTH sets both lights
//           together; INSIDE and OUTSIDE each keep their own look), the status line ("IN ON 50%
//           OUT --  LINK 1/2"), 2x4 colour buttons (tap one to set it and turn on, double-tap the
//           lit one to turn the lights off), the four presets (tap: apply),
//           and  [-]  [level: a 10-cell block bar and the %, OFF, or MIX on BOTH while the two
//           lights differ]  [+]
//   Weather: < BACK, the title tab WEATHER, the temperature big, the sky in words ("PARTLY CLOUDY"),
//           today's HI / LO, the city, and "UPDATED 12:30" (or NO DATA YET / >OFFLINE). Where the
//           car is comes from the hotspot's public IP (ip-api.com, approximate), the weather from
//           Open-Meteo, both over plain http every 15 min (see "Weather" below)
//   Karaoke: < BACK and "TITLE - ARTIST" of the song the black deck (the S3) is playing, its synced
//           lyric line big, the next line under it, and a thin progress line (or >NO SYNCED LYRICS /
//           >STANDBY / >NO SIGNAL / >WRONG KEY big in place of the lyrics). Read from the black
//           deck's GET /state every second while this screen shows, every 5 s while the Menu shows
//           (see "Lyrics" below)
// A press acts once it settles (its first ~40 ms averaged), and a tap in a gap snaps to the nearest
// button. The iPhone's Bluetooth must be off (Settings): each controller takes one connection.
// The STATUS line on Lights and the strip's text on the Menu show a terminal message for 2.5 s
// (">ROSE APPLIED", ">INSIDE: NO LINK"), then their usual line again.
// Each page's colour, intensity and power and the page last shown survive a reboot (NVS); the deck
// still boots to the Menu. Touch calibration (resistive panel) runs on first boot,
// or with BOOT held (at power-up, or 2 s any time). Voice and the Garmin watch: GET /say, /state,
// /presets and /status over WiFi (see "Voice" below), on either screen. An Xbox controller (BLE)
// drives it too: the D-pad moves the focus (an inverted bar on the Menu, a TEXT ring on Lights,
// Weather and Karaoke), A presses, B goes back to the Menu; on Lights X is the next page, Y on/off, LB / RB - / + (see
// "Xbox controller input" below).
// Night mode (like the black deck): 18:00-06:00 local time, once WiFi has synced the clock, the
// palette goes to termpal's amber and the backlight to its night level. Without REMOTE_TOKEN
// there is no WiFi, no clock, and the deck stays in day mode.
// Test hook: building with -D NIGHT_TEST (e.g. PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST") forces night
// mode whatever the time, to check the dimming without waiting for 18:00. Not set in platformio.ini.
//   pio run -e ledpanel -t upload
#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
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
#include "net/ipaddr.h"
#include "net/remote_config.h"
#include "net/wifi.h"
#include "util/bothlook.h"
#include "util/daynight.h"
#include "util/deckaddr.h"
#include "util/deckmenu.h"
#include "util/elkcmd.h"
#include "util/geoip.h"
#include "util/hotspotip.h"
#include "util/ledlook.h"
#include "util/ledpresets.h"
#include "util/ledvoice.h"
#include "util/lightstatus.h"
#include "util/pollpace.h"
#include "util/karaoke.h"
#include "util/readguard.h"
#include "util/remotegate.h"
#include "util/statejson.h"
#include "util/stateparse.h"
#include "util/termpal.h"
#include "util/text.h"
#include "util/touchcal.h"
#include "util/tripclock.h"
#include "util/tripmeter.h"
#include "util/weather.h"

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
// The BOTH page is on screen while the lights differ (after a preset or the INSIDE / OUTSIDE
// pages): it shows no selected colour and "MIX" for the level.
static bool bothMixed() { return target == BOTH && bothlook::mixed(looks[INSIDE], looks[OUTSIDE]); }

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
using deckmenu::W_BACK;
using deckmenu::K_BACK;

static Screen screen = Screen::Menu;  // the screen shown; boot lands on the Menu
// The palette: every colour goes through termpal with this flag, so night mode only flips it and
// redraws.
static bool night = false;

// The outside weather (see "Weather" below): the weather task publishes each try under weatherMux
// and raises weatherFresh; loop() copies it into `wx`, which is all the drawing reads.
struct WeatherReading {
    bool ok = false;      // a reading came (the fields below hold the last good one)
    bool failed = false;  // the last try failed (no network, no answer)
    double tempC = 0;
    int code = 0;
    bool day = true;
    bool daily = false;   // maxC / minC came too
    double maxC = 0, minC = 0;
    char city[24] = "";
    int at = -1;          // local minute of the day it came; -1: the clock wasn't synced
};
static WeatherReading wx;
static bool weatherOff = false;  // no REMOTE_TOKEN: no WiFi, so no weather (">OFFLINE")

// What the black deck (the S3) is playing (see "Lyrics" below): the lyrics task publishes each poll
// under lyricsMux and raises lyricsFresh; loop() copies it into `song`, which is all the drawing reads.
struct LyricsReading {
    bool wrongKey = false;    // the last poll was turned down (403: the decks' REMOTE_TOKEN differ)
    stateparse::Deck deck{};  // the last state that came (a miss keeps it)
    uint32_t at = 0;          // millis() it came; 0: none yet
};
static LyricsReading song;

// A state is believed for a while after it came, so the black deck's odd dropped poll doesn't blank
// the KARAOKE screen or dim its Menu item: on KARAOKE (a poll a second) for LYRICS_HOLD_MS after it
// came or after the screen opened (the Menu's last poll carries it until the screen's own first
// one), on the Menu (a poll every MENU_POLL_MS) for MENU_HOLD_MS, a dropped poll and some.
static constexpr uint32_t MENU_POLL_MS = 5000;
static constexpr uint32_t LYRICS_HOLD_MS = 5000, MENU_HOLD_MS = 3 * MENU_POLL_MS;
static uint32_t karaokeAt = 0;  // millis() the KARAOKE screen was shown

static bool lyricsHeard() {
    if (song.at == 0) return false;
    const uint32_t now = millis(), age = now - song.at;
    if (age >= MENU_HOLD_MS) return false;
    if (screen == Screen::Menu) return true;
    return age < LYRICS_HOLD_MS || now - karaokeAt < LYRICS_HOLD_MS;
}

// The black deck plays a song with synced lyrics: the Menu's KARAOKE item is lit and opens.
static bool karaokeReady() { return karaoke::available(lyricsHeard(), song.deck.playing, song.deck); }
static bool karaokeShownReady = false;  // as the Menu item was last drawn (lit or dimmed)

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
// an inverted bar, on Lights as a TEXT ring around the button (the lit colour is the bar there).
static int focus = -1;
static bool padUp = false;  // an Xbox controller is connected and reporting
static void drawFocus(int id, bool on);

// Where the focus starts on each screen: the first item, or the colour the page shows.
static int homeFocus() {
    if (screen == Screen::Menu) return M_ITEM0;
    if (screen == Screen::Weather) return W_BACK;
    if (screen == Screen::Karaoke) return K_BACK;
    return L_TILE0 + cur().swatch;
}

// ---- Drawing ----
// GLCD (font 1) is the terminal's monospace font, for small text; button labels use font 2 (and 4
// for [-] / [+]), as GLCD at x2 is wider than deckmenu's buttons. Every label is upper case.
// The background is black with a faint scanline on every 3rd screen row: every clear goes through
// clearBg(), which repaints a rect's lines on absolute rows, so a partial redraw stays aligned.
// Text over it is drawn transparent (no black box punching through the lines); the solid fills
// (the inverted selection bar, the pressed yellow, the chips) are the design.

static uint16_t c565(uint32_t rgb) { return tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static uint16_t pal(termpal::Role r) { return c565(termpal::rgb(r, night)); }
// A real colour (a chip, the pressed yellow), dimmed with the palette at night.
static uint16_t tint(uint32_t rgb) { return c565(termpal::tile(rgb, night)); }
static constexpr uint32_t PRESSED = 0xFFFF00;  // under the finger, until release

static void clearBg(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.fillRect(x, y, w, h, pal(termpal::BG));
    const uint16_t line = c565(termpal::scan(night));
    for (int row = y + (3 - y % 3) % 3; row < y + h; row += 3) tft.drawFastHLine(x, row, w, line);
}
static void clearBg(const Rect& r) { clearBg(r.x, r.y, r.w, r.h); }

// drawString at a text size, back to size 1 after (fonts 2 and 4 scale with it too).
static void label(const char* s, int x, int y, uint8_t fontNo, uint8_t size = 1) {
    tft.setTextSize(size);
    tft.drawString(s, x, y, fontNo);
    tft.setTextSize(1);
}

// A swatch name ("Orange") as the screen shows it ("ORANGE").
static void upper(const char* s, char* out, size_t n) {
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) out[i] = static_cast<char>(toupper(static_cast<unsigned char>(s[i])));
    out[i] = 0;
}

// The box of a terminal button: the scanlined bg inside a MID border (`border` px), or the solid
// bar when selected (TEXT) or pressed (yellow), 1 px in from the edge so a Lights focus ring around
// it stays apart. Returns the ink for what goes on it (BG on a bar, else `ink`).
static uint16_t drawBox(const Rect& r, bool pressed, bool selected, int border, uint16_t ink) {
    clearBg(r);
    if (pressed || selected) {
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, pressed ? tint(PRESSED) : pal(termpal::TEXT));
        return pal(termpal::BG);
    }
    for (int k = 0; k < border; k++) tft.drawRect(r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k, pal(termpal::MID));
    return ink;
}

// A bordered button (< BACK, [-], [+]): the label centred.
static void drawChrome(const Rect& r, const char* s, uint8_t fontNo, bool pressed, bool inverted) {
    const uint16_t ink = drawBox(r, pressed, inverted, 2, pal(termpal::TEXT));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink);
    label(s, r.x + r.w / 2, r.y + r.h / 2, fontNo);
}

// A Menu list row ("> [LIGHTS]"): no border, the text left; the inverted bar when focused. Dimmed
// (an item that won't open now): DIM text in a DIM border, the bar in DIM.
static void drawRow(const Rect& r, const char* s, bool pressed, bool inverted, bool dim = false) {
    uint16_t ink;
    if (dim && !pressed) {
        clearBg(r);
        if (inverted) {
            tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, pal(termpal::DIM));
            ink = pal(termpal::BG);
        } else {
            tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::DIM));
            ink = pal(termpal::DIM);
        }
    } else {
        ink = drawBox(r, pressed, inverted, 0, pal(termpal::TEXT));
    }
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ink);
    label(s, r.x + 8, r.y + r.h / 2, 2);
}

// A colour chip of `l` (dimmed at night like any real colour): filled when lit, a DIM outline when
// off. On a bar, a 1 px BG edge keeps it apart from the fill (pink on yellow, white on green).
static void drawChip(int x, int y, int size, const Look& l, bool onBar) {
    if (l.lit) tft.fillRect(x, y, size, size, tint(SWATCHES[l.swatch].screen));
    else tft.drawRect(x, y, size, size, pal(termpal::DIM));
    if (onBar) tft.drawRect(x - 1, y - 1, size + 2, size + 2, pal(termpal::BG));
}

// A preset block. Menu (154x42): the name left, and right of it two big chips (inside, outside)
// with each light's level under it. Lights (75x36): the name over two small chips. Inverted under
// the Menu's focus bar, yellow when pressed.
static void drawPresetSlot(const Rect& r, int slot, bool pressed, bool inverted) {
    const ledpresets::Preset& p = ledpresets::PRESETS[slot];
    const bool bar = pressed || inverted;
    const uint16_t ink = drawBox(r, pressed, inverted, 1, pal(termpal::TEXT));
    const Look* sides[] = {&p.in, &p.out};
    tft.setTextColor(ink);
    if (r.w >= 120) {
        tft.setTextDatum(ML_DATUM);
        label(p.name, r.x + 8, r.y + r.h / 2, 2);  // font 2: "PURPURA" is 56 px
        tft.setTextDatum(MC_DATUM);
        for (int k = 0; k < 2; k++) {
            const int x = r.x + r.w - 50 + k * 24;
            drawChip(x, r.y + 5, 18, *sides[k], bar);
            char lv[8];
            snprintf(lv, sizeof(lv), "%u", static_cast<unsigned>(sides[k]->pct));
            label(sides[k]->lit ? lv : "OFF", x + 9, r.y + 32, 1);
        }
        return;
    }
    const int cx = r.x + r.w / 2;
    tft.setTextDatum(TC_DATUM);
    label(p.name, cx, r.y + 4, 2);  // fits the 75 px slot
    for (int k = 0; k < 2; k++) drawChip(cx - 11 + k * 14, r.y + r.h - 12, 8, *sides[k], bar);
}

// A colour button: a box with a MID border, a big chip of the colour (left, centred) and its name.
// The lit colour is the inverted bar, chip kept; the Xbox focus is a separate ring around it.
static void drawTile(const Rect& r, int swatch, bool pressed) {
    const Swatch& s = SWATCHES[swatch];
    const bool sel = !bothMixed() && cur().lit && swatch == cur().swatch;
    const uint16_t ink = drawBox(r, pressed, sel, 1, pal(termpal::TEXT));
    constexpr int CHIP = 28;
    Look l;
    l.swatch = swatch;
    drawChip(r.x + 5, r.y + (r.h - CHIP) / 2, CHIP, l, pressed || sel);
    char name[12];
    upper(s.name, name, sizeof(name));
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ink);
    label(name, r.x + 37, r.y + r.h / 2, 1);  // GLCD: "ORANGE" is 36 px, the room left of the border
}

// Lights' zone tabs are Pip-Boy tabs: a TEXT rule under the tab row, the current tab a TEXT bar
// that runs down into the rule, the others MID text. TAB_RULE_Y is just under the tab row (rects
// end at y 37) and over STATUS (y 40).
static constexpr int TAB_RULE_Y = 39;
static constexpr int RULE_X0 = 4, RULE_X1 = 316;

// The rule's rows (and the gap row above it) over [x0, x1).
static void drawTabRule(int x0, int x1) {
    clearBg(x0, TAB_RULE_Y - 1, x1 - x0, 2);
    const int a = x0 < RULE_X0 ? RULE_X0 : x0, b = x1 > RULE_X1 ? RULE_X1 : x1;
    if (b > a) tft.drawFastHLine(a, TAB_RULE_Y, b - a, pal(termpal::TEXT));
}

static void drawTab(const Rect& r, const char* s, bool pressed, bool current) {
    clearBg(r);
    drawTabRule(r.x - 2, r.x + r.w + 2);  // under it and its focus ring
    uint16_t ink = pal(termpal::MID);
    if (pressed || current) {
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, TAB_RULE_Y - r.y, pressed ? tint(PRESSED) : pal(termpal::TEXT));
        ink = pal(termpal::BG);
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink);
    label(s, r.x + r.w / 2, r.y + r.h / 2, 2);
}

static void drawButtonBody(int id, bool pressed) {
    const Rect r = deckmenu::rect(screen, id);
    if (r.w <= 0) return;
    if (screen == Screen::Menu) {
        static const char* const ITEM[deckmenu::N_ITEMS] = {"> [LIGHTS]", "> [WEATHER]", "> [KARAOKE]"};  // in deckmenu::Item order
        bool dim = false;
        if (id == M_ITEM0 + deckmenu::KARAOKE) {
            karaokeShownReady = karaokeReady();
            dim = !karaokeShownReady;
        }
        if (id < M_PRE0) drawRow(r, ITEM[id - M_ITEM0], pressed, id == focus, dim);
        else drawPresetSlot(r, deckmenu::presetSlot(screen, id), pressed, id == focus);
        return;
    }
    if (screen == Screen::Weather || screen == Screen::Karaoke || id == L_BACK) {  // W_BACK / K_BACK: their only button
        drawChrome(r, "< BACK", 2, pressed, false);
        drawTabRule(r.x - 2, r.x + r.w + 2);  // its focus ring crosses the rule too
    } else if (id < L_TILE0) {
        static const char* const ZONE[] = {"BOTH", "INSIDE", "OUTSIDE"};
        drawTab(r, ZONE[id - L_ZONE0], pressed, target == static_cast<Target>(id - L_ZONE0));
    } else if (id < L_PRE0) {
        drawTile(r, id - L_TILE0, pressed);
    } else if (id < L_MINUS) {
        drawPresetSlot(r, deckmenu::presetSlot(screen, id), pressed, false);  // focus: the ring
    } else {
        drawChrome(r, id == L_MINUS ? "[-]" : "[+]", 4, pressed, false);
    }
}

static void drawButton(int id, bool pressed = false) {
    drawButtonBody(id, pressed);
    if (id == focus) drawFocus(id, true);  // on Lights its ring hugs the rect just drawn over
}

// The block bar: 10 cells, one filled per 10 % (the terminal's full block), the rest outlined (its
// shade block; TFT_eSPI's fonts have neither glyph), and the % under it. "MIX" on BOTH while the
// lights differ (the STATUS line above has each light's %), "OFF" in DIM when off.
static void drawLevel() {
    const Rect& r = deckmenu::LEVEL;
    const Look& l = cur();
    const bool mixed = bothMixed(), live = mixed || l.lit;
    clearBg(r);
    tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::MID));
    constexpr int N_CELL = 10, CELL = 8, GAP = 2;
    const int full = live && !mixed ? l.pct / 10 : 0;
    const int x0 = r.x + (r.w - (N_CELL * (CELL + GAP) - GAP)) / 2;
    for (int k = 0; k < N_CELL; k++) {
        const int x = x0 + k * (CELL + GAP);
        if (k < full) tft.fillRect(x, r.y + 7, CELL, 14, pal(termpal::TEXT));
        else tft.drawRect(x, r.y + 7, CELL, 14, pal(live ? termpal::MID : termpal::DIM));
    }
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", static_cast<unsigned>(l.pct));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(pal(live ? termpal::TEXT : termpal::DIM));
    label(mixed ? "MIX" : l.lit ? pct : "OFF", r.x + r.w / 2, r.y + 34, 2);
}

// A terminal message (">ROSE APPLIED", ">INSIDE: NO LINK") in place of the Lights page's STATUS line
// or the Menu strip's text for MESSAGE_MS since messageAt; loop() then puts the usual line back. A
// newer message replaces it, and so does a screen change (show() drops it), so none outlives its
// screen. Only loop() touches it (and setup(), before loop() starts).
static constexpr uint32_t MESSAGE_MS = 2500;
static constexpr uint32_t LAST_TRIP_MS = 6000;  // the boot's ">LAST TRIP ..." is worth longer to read
static char message[32] = "";  // "" when none
static uint32_t messageAt = 0;
static uint32_t messageMs = MESSAGE_MS;

// Sets the message; the caller redraws what shows it (drawLightState, or drawLooks that ends in it).
static void setMessage(const char* m, uint32_t ms = MESSAGE_MS) {
    snprintf(message, sizeof(message), "%s", m);
    messageAt = millis();
    messageMs = ms;
}

// Lights: the line under the tabs, "IN ON 50%  OUT --  LINK 1/2", or the message.
static void drawStatus() {
    const Rect& r = deckmenu::STATUS;
    char line[48];
    lightstatus::line(looks[INSIDE], linkUp(controllers[0]), looks[OUTSIDE], linkUp(controllers[1]), line, sizeof(line));
    clearBg(r);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    label(message[0] ? message : line, r.x + 2, r.y + r.h / 2, 1);
}

// The Menu strip's block cursor: one GLCD cell at the end of the text line, blinking at ~1 Hz.
// Only its cell is redrawn. -1 when it would pass the strip's edge (x 316); the longest line still fits.
static constexpr uint32_t BLINK_MS = 500;
static int cursorX = -1;
static bool cursorOn = false;

static void drawCursor(bool on) {
    cursorOn = on;
    if (cursorX < 0) return;
    const int y = deckmenu::MENU_STRIP.y + deckmenu::MENU_STRIP.h / 2 - 4;
    if (on) tft.fillRect(cursorX, y, 6, 8, pal(termpal::TEXT));
    else clearBg(cursorX, y, 6, 8);
}

static bool blinkPhase() { return (millis() / BLINK_MS) % 2 == 0; }

// The cursor follows the text's end: a message's, or the strip line's.
static void placeCursor(int x) {
    const Rect& r = deckmenu::MENU_STRIP;
    cursorX = x + 6 <= r.x + r.w - 4 ? x : -1;
    drawCursor(blinkPhase());
}

// The Menu strip's text right of the clock: lightstatus::strip ("IN PINK 40%  OUT --"), with a chip
// of the light's colour before each colour name, or the message, then the cursor. GLCD x1: the
// longest line ("IN ORANGE 100%  OUT ORANGE 100%" plus two chips) is 202 px.
static constexpr int STRIP_TEXT_X = 108;
static constexpr int TRIP_Y = 226;  // the trip line's middle: under the colours line (y 212), above the strip's foot (240)
static void drawStripText() {
    const Rect& r = deckmenu::MENU_STRIP;
    const int cy = r.y + r.h / 2;
    clearBg(STRIP_TEXT_X, r.y + 2, r.x + r.w - STRIP_TEXT_X, TRIP_Y - 5 - (r.y + 2));  // under the rule, above the trip line
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    if (message[0]) {
        placeCursor(STRIP_TEXT_X + tft.drawString(message, STRIP_TEXT_X, cy, 1));
        return;
    }
    char line[48];
    const bool up[N_CTRL] = {linkUp(controllers[0]), linkUp(controllers[1])};
    lightstatus::strip(looks[INSIDE], up[0], looks[OUTSIDE], up[1], line, sizeof(line));
    // "IN <state>  OUT <state>": each half is its name, a space, then the state; the chip goes in
    // that space when the light is on (the state then starts with its colour name).
    char* halves[2] = {line, strstr(line, "  ")};
    if (!halves[1]) {  // not the expected shape: plain text
        placeCursor(STRIP_TEXT_X + tft.drawString(line, STRIP_TEXT_X, cy, 1));
        return;
    }
    *halves[1] = 0;
    halves[1] += 2;
    int x = STRIP_TEXT_X;
    for (int i = 0; i < 2; i++) {
        if (i) x += 12;  // the two spaces between the lights
        char* state = strchr(halves[i], ' ');
        if (state) *state++ = 0;
        x += tft.drawString(halves[i], x, cy, 1);
        const Look& l = looks[i + 1];
        if (state && lightstatus::of(up[i], l.lit) == lightstatus::State::On) {
            tft.fillRect(x + 4, cy - 4, 7, 7, tint(SWATCHES[l.swatch].screen));
            x += 14;
        } else {
            x += 6;  // a space
        }
        if (state) x += tft.drawString(state, x, cy, 1);
    }
    placeCursor(x);
}

// Each loop pass on the Menu: the cursor's cell when its phase turned.
static void blinkCursor() {
    if (screen == Screen::Menu && blinkPhase() != cursorOn) drawCursor(!cursorOn);
}

// The clock's minute of the day on screen (-1: not synced, "--:--"); updateDayNight keeps it.
static int clockMin = -1;

// The temperature as shown: "24C", "--C" before the first reading (GLCD has no degree sign).
static void tempText(char* out, size_t n) {
    if (wx.ok) snprintf(out, n, "%dC", static_cast<int>(lround(wx.tempC)));
    else snprintf(out, n, "--C");
}

// The Menu strip's temperature: GLCD under the clock (its digits end at y 224), right edge under the
// clock's (x 98). stripTemp is the text on screen: a new reading redraws it only when it differs.
static char stripTemp[16] = "";
static void drawStripTemp() {
    const Rect& r = deckmenu::MENU_STRIP;
    tempText(stripTemp, sizeof(stripTemp));
    clearBg(r.x + 40, 227, 66, 11);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    tft.drawString(stripTemp, r.x + 98, 232, 1);
}

static void drawClock() {
    const Rect& r = deckmenu::MENU_STRIP;
    char s[16];  // room for any int, so -Wformat-truncation stays quiet
    if (clockMin < 0) snprintf(s, sizeof(s), "--:--");
    else snprintf(s, sizeof(s), "%02d:%02d", clockMin / 60, clockMin % 60);
    clearBg(r.x + 4, r.y + 2, STRIP_TEXT_X - r.x - 4, r.h - 2);  // left of the text, under the rule
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    label(s, r.x + 8, r.y + r.h / 2, 1, 3);
    drawStripTemp();  // the clear above took it
}

// The trip meter: time since the first loop() pass (the car started the deck), kilometres once a
// speed feeds it. The Menu strip's second line shows it; tripTick() redraws that line when its
// text would change (every minute, or every 0.1 km).
static tripmeter::Meter tripMeter;

static void drawTrip() {
    const Rect& r = deckmenu::MENU_STRIP;
    char s[40];
    tripmeter::line(tripMeter.trip(), s, sizeof(s));
    clearBg(STRIP_TEXT_X, TRIP_Y - 5, r.x + r.w - STRIP_TEXT_X, 10);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    tft.drawString(s, STRIP_TEXT_X, TRIP_Y, 1);
}

static void drawStrip() {
    const Rect& r = deckmenu::MENU_STRIP;
    clearBg(r);
    tft.drawFastHLine(r.x + 4, r.y + 1, r.w - 8, pal(termpal::MID));
    drawClock();
    drawStripText();
    drawTrip();
}

// The Menu header: three terminal lines, the last in MID, and a rule. The boot types them out.
static const char* const HEADER_LINES[] = {"WELCOME TO ROBCO INDUSTRIES (TM) TERMLINK", ">SET TERMINAL/INQUIRE",
                                           "HONDA CITY // CARDECK OS"};
static constexpr int N_HEADER = sizeof(HEADER_LINES) / sizeof(HEADER_LINES[0]);
static constexpr int HEADER_X = 8, HEADER_Y0 = 4, HEADER_DY = 12;
static uint16_t headerInk(int i) { return pal(i == N_HEADER - 1 ? termpal::MID : termpal::TEXT); }

static void drawHeaderRule() {
    const Rect& r = deckmenu::HEADER;
    tft.drawFastHLine(r.x + 4, r.y + r.h - 3, r.w - 8, pal(termpal::MID));
}

static void drawHeader() {
    const Rect& r = deckmenu::HEADER;
    clearBg(r);
    tft.setTextDatum(TL_DATUM);
    for (int i = 0; i < N_HEADER; i++) {
        tft.setTextColor(headerInk(i));
        tft.drawString(HEADER_LINES[i], HEADER_X, HEADER_Y0 + i * HEADER_DY, 1);
    }
    drawHeaderRule();
}

// The RobCo boot: on a blank screen the header's lines type out one character every TYPE_MS behind
// a block cursor (85 characters, ~0.5 s), then drawAll() shows the Menu. It runs in setup(), so
// nothing else draws meanwhile: every other drawing path runs from loop(), and the link and voice
// tasks start after it (a voice job or a link change that comes later waits in the queue / the
// link flags for loop() as always).
static constexpr uint32_t TYPE_MS = 6;
static void typeHeader() {
    clearBg(0, 0, tft.width(), tft.height());
    tft.setTextDatum(TL_DATUM);
    for (int i = 0; i < N_HEADER; i++) {
        tft.setTextColor(headerInk(i));
        const int y = HEADER_Y0 + i * HEADER_DY;
        int x = HEADER_X;
        for (const char* p = HEADER_LINES[i]; *p; p++) {
            const char ch[2] = {*p, 0};
            clearBg(x, y, 6, 8);  // the cursor
            x += tft.drawString(ch, x, y, 1);
            tft.fillRect(x, y, 6, 8, pal(termpal::TEXT));
            delay(TYPE_MS);
        }
        clearBg(x, y, 6, 8);
    }
    drawHeaderRule();  // drawAll(true) keeps the typed header, so the rule is part of it
}

static void drawGrid() {
    for (int id = L_TILE0; id < L_PRE0; id++) drawButton(id);
}

static void drawZones() {
    for (int id = L_ZONE0; id < L_TILE0; id++) drawButton(id);
}

// What shows the lights' state (or the message) on the current screen: Lights' STATUS line, the
// Menu strip's text.
static void drawLightState() {
    if (screen == Screen::Lights) drawStatus();
    else if (screen == Screen::Menu) drawStripText();
}

// After a look changed (a tap, voice, the watch, Y): everything that shows it.
static void drawLooks() {
    if (screen == Screen::Lights) {
        drawGrid();
        drawLevel();
    }
    drawLightState();
}

// Lights, Weather and Karaoke (the Menu's focus is the inverted bar, drawn by the button itself): a TEXT ring in
// the 2 px gap around the button, apart from the lit colour's bar inside it. Off: the gap's
// scanlined bg again (the caller redraws the button, and with it any rule the ring crossed).
static void drawFocus(int id, bool on) {
    if (id < 0 || screen == Screen::Menu) return;
    const Rect r = deckmenu::rect(screen, id);
    if (on) {
        tft.drawRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, pal(termpal::TEXT));
        tft.drawRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, pal(termpal::TEXT));
        return;
    }
    clearBg(r.x - 2, r.y - 2, r.w + 4, 2);
    clearBg(r.x - 2, r.y + r.h, r.w + 4, 2);
    clearBg(r.x - 2, r.y, 2, r.h);
    clearBg(r.x + r.w, r.y, 2, r.h);
}

// The Weather screen's title: the current Pip-Boy tab right of < BACK (display only, no button).
static constexpr Rect WEATHER_TAB{80, 2, 100, 36};

// The Weather screen's body (deckmenu::WEATHER_BODY), from `wx`: the temperature big (font 4 at x2,
// 52 px), the sky in words, today's HI / LO, the city in MID, and when it came (or why nothing did).
static void drawWeatherBody() {
    const Rect& r = deckmenu::WEATHER_BODY;
    clearBg(r);
    constexpr int X = 16;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    char s[40];
    tempText(s, sizeof(s));
    label(s, X, r.y + 10, 4, 2);
    if (wx.ok) label(weather::words(wx.code), X, r.y + 70, 4);
    if (wx.ok && wx.daily)
        snprintf(s, sizeof(s), "HI %dC  LO %dC", static_cast<int>(lround(wx.maxC)), static_cast<int>(lround(wx.minC)));
    else snprintf(s, sizeof(s), "HI --C  LO --C");
    label(s, X, r.y + 104, 2);
    if (wx.city[0]) {
        char folded[sizeof(wx.city)], city[sizeof(wx.city)];
        txt::asciiFold(wx.city, folded, sizeof(folded));  // "Sao Paulo": the fonts are ASCII only
        upper(folded, city, sizeof(city));
        tft.setTextColor(pal(termpal::MID));
        label(city, X, r.y + 126, 2);
    }
    if (wx.ok && wx.at >= 0) snprintf(s, sizeof(s), "UPDATED %02d:%02d", wx.at / 60, wx.at % 60);
    else if (wx.ok) snprintf(s, sizeof(s), "UPDATED --:--");
    else snprintf(s, sizeof(s), "%s", wx.failed || weatherOff ? ">OFFLINE" : "NO DATA YET");
    tft.setTextColor(pal(wx.ok ? termpal::DIM : termpal::TEXT));
    label(s, X, r.y + 168, 1);
}

static void drawWeather() {
    drawTab(WEATHER_TAB, "WEATHER", false, true);
    drawWeatherBody();
}

// ---- Karaoke screen: the black deck's song, its synced lyrics big ----
// The top row: < BACK and right of it the song as "TITLE - ARTIST" (font 2, MID). The body
// (deckmenu::KARAOKE_BODY): the lyric line due now big (font 4, TEXT, centred, word-wrapped over up
// to 3 lines, ".." on the 3rd; a line too long for that in font 2 over up to 4), the next line under
// it (font 2, DIM, up to 2 lines), and a thin progress line at the foot (TEXT while playing). Texts
// keep their case (lyrics read better than caps) and are accent-folded (the fonts are ASCII only).
// Without a song to sing along the big text is the state word in DIM (>NO SYNCED LYRICS, >STANDBY,
// >NO SIGNAL, >WRONG KEY) and the screen stays. The line switches at its start time: karaoke
// interpolates the position between polls and lyricsTick looks every KARAOKE_TICK_MS. Each part is
// redrawn only when what it shows changed.
static constexpr uint32_t KARAOKE_TICK_MS = 50;
static constexpr int KARAOKE_X = 4, KARAOKE_W = 312, KARAOKE_CX = 160;
static constexpr int KARAOKE_SONG_X = 80, KARAOKE_SONG_W = 236;  // right of < BACK (x 4..74), on the top row
static constexpr int KARAOKE_BIG_Y = 56, KARAOKE_BIG_H = 90;     // the line due now: 3 lines of font 4
static constexpr int KARAOKE_BIG_DY = 30, KARAOKE_SMALL_DY = 20; // font 4 / font 2 line pitches
static constexpr int KARAOKE_NEXT_Y = 156, KARAOKE_NEXT_H = 40;  // the next line: 2 lines of font 2
static constexpr int KARAOKE_BAR_Y = 224, KARAOKE_BAR_H = 3;

// A text's width in px in a TFT_eSPI font, for karaoke::wrap.
static int textPx(const char* s, int fontNo) { return tft.textWidth(s, static_cast<uint8_t>(fontNo)); }

// The texts are accent-folded into karaokeFolded and wrapped into karaokeLines (only loop draws).
static char karaokeFolded[sizeof(stateparse::Deck::line)];
static karaoke::Line karaokeLines[4];

// karaokeLines[0..n) centred, from y every dy px.
static void drawLines(int n, int fontNo, int y, int dy, uint16_t ink) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(ink);
    for (int i = 0; i < n; i++) label(karaokeLines[i].s, KARAOKE_CX, y + i * dy, static_cast<uint8_t>(fontNo));
}

// The big text: font 4 over up to 3 lines, or font 2 over up to 4 when font 4 would cut it; the
// block centred in its area.
static void drawBig(const char* s, uint16_t ink) {
    txt::asciiFold(s, karaokeFolded, sizeof(karaokeFolded));
    bool cut = false;
    int n = karaoke::wrap(karaokeFolded, 4, textPx, KARAOKE_W, 3, karaokeLines, &cut);
    int fontNo = 4, dy = KARAOKE_BIG_DY;
    if (cut) {
        n = karaoke::wrap(karaokeFolded, 2, textPx, KARAOKE_W, 4, karaokeLines);
        fontNo = 2;
        dy = KARAOKE_SMALL_DY;
    }
    drawLines(n, fontNo, KARAOKE_BIG_Y + (KARAOKE_BIG_H - n * dy) / 2, dy, ink);
}

// The next line: font 2 over up to 2 lines, DIM.
static void drawNext(const char* s) {
    txt::asciiFold(s, karaokeFolded, sizeof(karaokeFolded));
    const int n = karaoke::wrap(karaokeFolded, 2, textPx, KARAOKE_W, 2, karaokeLines);
    drawLines(n, 2, KARAOKE_NEXT_Y, KARAOKE_SMALL_DY, pal(termpal::DIM));
}

// What is on screen now (the texts as the deck sent them), so a redraw paints only the part that
// changed. drawKaraokeBody(true) forgets it.
static char karaokeShownSong[sizeof(stateparse::Deck::track) + sizeof(stateparse::Deck::artist) + 4];
static char karaokeShownBig[sizeof(stateparse::Deck::line)], karaokeShownNext[sizeof(stateparse::Deck::line)];
static bool karaokeShownWord = false;  // the big text is a state word
static int karaokeShownBar = -1;
static bool karaokeShownLive = false;

static void drawKaraokeBody(bool all) {
    const uint32_t now = millis();
    const bool heard = lyricsHeard();
    const stateparse::Deck& d = song.deck;
    const bool hasTrack = heard && d.track[0];
    if (all) {
        clearBg(deckmenu::KARAOKE_BODY);
        karaokeShownSong[0] = karaokeShownBig[0] = karaokeShownNext[0] = 0;
        karaokeShownWord = false;
        karaokeShownBar = -1;
    }

    static char songText[sizeof(karaokeShownSong)];
    if (!hasTrack) songText[0] = 0;
    else if (d.artist[0]) snprintf(songText, sizeof(songText), "%s - %s", d.track, d.artist);
    else snprintf(songText, sizeof(songText), "%s", d.track);
    if (all || strcmp(songText, karaokeShownSong) != 0) {
        snprintf(karaokeShownSong, sizeof(karaokeShownSong), "%s", songText);
        static char folded[sizeof(songText)];
        static karaoke::Line fit[1];
        txt::asciiFold(songText, folded, sizeof(folded));
        const int n = karaoke::wrap(folded, 2, textPx, KARAOKE_SONG_W, 1, fit);
        clearBg(KARAOKE_SONG_X, 2, KARAOKE_SONG_W, 36);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(pal(termpal::MID));
        if (n > 0) label(fit[0].s, KARAOKE_SONG_X, 20, 2);
    }

    const uint32_t pos = hasTrack ? static_cast<uint32_t>(karaoke::progressMs(d, now - song.at)) : 0;
    const char* stateWord = karaoke::state(heard, song.wrongKey, d.playing, d);
    const bool isWord = stateWord[0] != 0;
    const char* big = isWord ? stateWord : karaoke::lyric(d, pos);
    if (all || isWord != karaokeShownWord || strcmp(big, karaokeShownBig) != 0) {
        snprintf(karaokeShownBig, sizeof(karaokeShownBig), "%s", big);
        karaokeShownWord = isWord;
        clearBg(KARAOKE_X, KARAOKE_BIG_Y, KARAOKE_W, KARAOKE_BIG_H);
        drawBig(big, pal(isWord ? termpal::DIM : termpal::TEXT));
    }
    const char* next = isWord ? "" : karaoke::nextLine(d, pos);
    if (all || strcmp(next, karaokeShownNext) != 0) {
        snprintf(karaokeShownNext, sizeof(karaokeShownNext), "%s", next);
        clearBg(KARAOKE_X, KARAOKE_NEXT_Y, KARAOKE_W, KARAOKE_NEXT_H);
        drawNext(next);
    }

    // the progress line: the played part a solid bar, the rest a DIM hairline through its middle
    const int px = karaoke::cells(pos, hasTrack ? d.durationMs : 0, KARAOKE_W);
    const bool live = hasTrack && d.playing;
    if (all || px != karaokeShownBar || live != karaokeShownLive) {
        karaokeShownBar = px;
        karaokeShownLive = live;
        if (px > 0) tft.fillRect(KARAOKE_X, KARAOKE_BAR_Y, px, KARAOKE_BAR_H, pal(live ? termpal::TEXT : termpal::MID));
        if (px < KARAOKE_W) {
            clearBg(KARAOKE_X + px, KARAOKE_BAR_Y, KARAOKE_W - px, KARAOKE_BAR_H);
            tft.drawFastHLine(KARAOKE_X + px, KARAOKE_BAR_Y + KARAOKE_BAR_H / 2, KARAOKE_W - px, pal(termpal::DIM));
        }
    }
}

// Redraws the current screen in the current palette. keepHeader (boot, right after typeHeader):
// the Menu's header rect is left as typed, so it doesn't blink off for the redraw.
static void drawAll(bool keepHeader = false) {
    if (keepHeader && screen == Screen::Menu) {
        const int top = deckmenu::HEADER.y + deckmenu::HEADER.h;
        clearBg(0, top, tft.width(), tft.height() - top);
    } else {
        clearBg(0, 0, tft.width(), tft.height());
        if (screen == Screen::Menu) drawHeader();
    }
    if (screen != Screen::Menu) drawTabRule(0, tft.width());
    for (int id = 0; id < deckmenu::count(screen); id++) {
        if (built(id)) drawButton(id);
    }
    if (screen == Screen::Menu) {
        drawStrip();
    } else if (screen == Screen::Weather) {
        drawWeather();
    } else if (screen == Screen::Karaoke) {
        drawKaraokeBody(true);
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
    // fav0..fav3 (the saved favourites of earlier builds) stay in NVS, unread.
}

// ---- Trip meter: the clock, the OBD hook and the NVS record ("trip", 9 bytes) ----
static tripclock::Clock tripClock;
static uint32_t tripStartMs = 0;  // millis() of the first tick, 0: not started
static volatile uint16_t tripKmh = 0;       // the latest reading, written by tripSpeed()
static volatile uint32_t tripSpeedAt = 0;   // millis() of that reading, never 0; 0: none yet
static uint32_t tripIntegratedAt = 0;       // millis() tripTick() last integrated the speed
static constexpr uint32_t TRIP_MAX_STEP_MS = 3000;  // one integration step counts at most this (a stalled loop)
static constexpr uint32_t TRIP_FRESH_MS = 3000;     // a reading older than this is stale (the link dropped): no distance
static uint8_t tripSaved[tripmeter::RECORD_BYTES];
static bool tripSavedValid = false;

// The OBD link will call this with each speed reading (km/h). It may run on another core, so it
// only stores the latest reading; tripTick() on the loop task integrates it once a second (the
// Meter isn't thread-safe). Nothing calls it yet and no speed is faked, so the strip shows "--" km.
static void tripSpeed(uint16_t kmh) __attribute__((unused));
static void tripSpeed(uint16_t kmh) {
    // The speed first, then the time that makes it count: 32-bit and 16-bit writes are atomic on the
    // ESP32, so loop() sees the old pair or the new speed with either time. A new speed with the old
    // time is merely a fraction of a second early.
    tripKmh = kmh;
    const uint32_t now = millis();
    tripSpeedAt = now ? now : 1;
}

// The previous trip from NVS, as a message when it lasted a minute or more (shorter ones are a
// bench test or a failed start). Called once, before the first draw.
static void showLastTrip() {
    uint8_t buf[tripmeter::RECORD_BYTES + 1];
    const size_t len = prefs.getBytesLength("trip");
    if (len != tripmeter::RECORD_BYTES || prefs.getBytes("trip", buf, sizeof(buf)) != len) return;
    tripmeter::Trip t;
    if (!tripmeter::decode(buf, len, &t) || t.seconds < 60) return;
    char m[sizeof(message)];
    tripmeter::last(t, m, sizeof(m));
    setMessage(m, LAST_TRIP_MS);
}

// Each loop pass: once a second the seconds, and the strip line if its text changed; once a minute
// the NVS record (when it differs from the last one written).
static void tripTick() {
    const uint32_t now = millis();
    if (!tripStartMs) {
        tripStartMs = now ? now : 1;  // not now|1: that can be 1 ms ahead of now
        tripClock = tripclock::Clock(tripStartMs);
    }
    static uint32_t checkedAt = 0;
    if (checkedAt && now - checkedAt < 1000) return;
    checkedAt = now ? now : 1;  // not now|1: that can be 1 ms ahead of now, and now - it wraps
    const uint32_t seenAt = tripSpeedAt;
    if (seenAt && static_cast<int32_t>(now - seenAt) <= static_cast<int32_t>(TRIP_FRESH_MS)) {  // signed: core 0 may stamp it after our now; a silent link stops the distance
        const uint32_t dt = tripIntegratedAt ? now - tripIntegratedAt : 0;
        tripMeter.addSpeed(tripKmh, dt < TRIP_MAX_STEP_MS ? dt : TRIP_MAX_STEP_MS);
    }
    tripIntegratedAt = checkedAt;
    tripMeter.setSeconds(tripClock.seconds(now));
    const tripmeter::Trip t = tripMeter.trip();
    // the line shows minutes and tenths of a km: redraw only when one of them moved
    static bool shown = false;
    static tripmeter::Trip on;
    if (!shown || t.seconds / 60 != on.seconds / 60 || t.meters / 100 != on.meters / 100 || t.hasDistance != on.hasDistance) {
        shown = true;
        on = t;
        if (screen == Screen::Menu) drawTrip();
    }
    static uint32_t savedAt = 0;
    if (!savedAt) savedAt = now ? now : 1;
    if (now - savedAt >= 60000) {
        savedAt = now ? now : 1;
        uint8_t buf[tripmeter::RECORD_BYTES];
        tripmeter::encode(t, buf);
        if (!tripSavedValid || memcmp(buf, tripSaved, sizeof(buf)) != 0) {
            prefs.putBytes("trip", buf, sizeof(buf));
            memcpy(tripSaved, buf, sizeof(buf));
            tripSavedValid = true;
        }
    }
}

// Preset slot0 (0..3) onto the lights: each light gets the preset's look. The BOTH page copies them
// when they match; otherwise it takes the inside look, lit if either light is (bothlook::seed),
// shows "MIX", and the Lights page's STATUS line tells the truth. From a tap, A, voice or the watch.
static void applyPreset(int slot0) {
    const ledpresets::Preset& p = ledpresets::PRESETS[slot0];
    looks[INSIDE] = p.in;
    looks[OUTSIDE] = p.out;
    looks[BOTH] = bothlook::seed(p.in, p.out);
    for (int i = 0; i < N_CTRL; i++) push(i);
    save();
    char m[sizeof(message)];
    lightstatus::applied(p.name, m, sizeof(m));
    setMessage(m);
    drawLooks();  // ends in drawLightState: the message shows
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

// The button a controller press is showing yellow, and since when (see padPress).
static constexpr uint32_t PAD_FLASH_MS = 150;
static int padFlash = -1;
static uint32_t padFlashAt = 0;

// Whether button `id` is under a finger or a controller press right now (drawn yellow).
static bool pressedNow(int id) { return (tap.down && tap.id == id) || padFlash == id; }

// Switches to screen `s`: the focus goes to its first button (none without a controller), a press
// still drawn yellow is forgotten (its id means another button there), and so is a message (it was
// about the screen left; the new screen shows its usual line).
// Back from Karaoke, the focus lands on the KARAOKE item it came from.
static void show(Screen s) {
    const Screen from = screen;
    screen = s;
    tap.id = -1;
    padFlash = -1;
    message[0] = 0;
    if (s == Screen::Karaoke) karaokeAt = millis();
    focus = padUp ? homeFocus() : -1;
    if (focus >= 0 && s == Screen::Menu && from == Screen::Karaoke) focus = M_ITEM0 + deckmenu::KARAOKE;
    drawAll();
}

// One settled tap on a Lights button.
static void pressLights(int id) {
    Look& l = cur();
    if (id == L_BACK) {
        show(Screen::Menu);
        return;
    }
    if (deckmenu::isPreset(screen, id)) {
        applyPreset(deckmenu::presetSlot(screen, id));
        return;  // applyPreset saved and drew
    }
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
    if (screen == Screen::Weather) {
        if (id == W_BACK) show(Screen::Menu);
        return;
    }
    if (screen == Screen::Karaoke) {
        if (id == K_BACK) show(Screen::Menu);
        return;
    }
    if (deckmenu::isPreset(screen, id)) {
        applyPreset(deckmenu::presetSlot(screen, id));
        return;
    }
    if (id < M_ITEM0 || id >= M_ITEM0 + deckmenu::N_ITEMS) return;
    switch (static_cast<deckmenu::Item>(id - M_ITEM0)) {
    case deckmenu::LIGHTS: show(Screen::Lights); break;
    case deckmenu::WEATHER: show(Screen::Weather); break;
    case deckmenu::KARAOKE:
        // dimmed: no song to sing along, so only the strip says why
        if (karaokeReady()) {
            show(Screen::Karaoke);
        } else {
            setMessage(karaoke::whyNot(lyricsHeard(), song.wrongKey, song.deck.playing, song.deck));
            drawStripText();
        }
        break;
    }
}

static void act() {
    tap.acted = true;
    const int x = static_cast<int>(tap.sx / tap.n), y = static_cast<int>(tap.sy / tap.n);
    tap.id = deckmenu::hit(screen, x, y);
    Serial.printf("[tap] %d,%d -> button %d\n", x, y, tap.id);
    if (tap.id < 0) return;
    const Screen was = screen;
    press(tap.id);
    if (screen == was) drawButton(tap.id, true);  // else show() redrew all and cleared tap.id
}

// ---- Xbox controller input ----
//   D-pad: move the focus (held: repeats; the sticks are ignored)    A: press the focused button
//   B: back to the Menu (from Lights, Weather or Karaoke; on the Menu: nothing)
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
    if (id < 0) return;
    if (padFlash >= 0 && padFlash != id) drawButton(padFlash);
    const Screen was = screen;
    press(id);
    if (screen != was) return;  // show() redrew the new screen
    drawButton(id, true);
    padFlash = id;
    padFlashAt = millis();
}

static void handlePad() {
    const bool is = xbox.isConnected() && !xbox.isWaitingForFirstNotification();
    if (is != padUp) {
        padUp = is;
        Serial.printf("[pad] %s\n", is ? "connected" : "gone");
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
    if (a) padPress(focus);
    if (screen != was) return;  // A changed the screen: the other buttons wait for their next press
    if (b && screen != Screen::Menu) {
        show(Screen::Menu);
        return;
    }
    if (screen != Screen::Lights) return;  // the rest is the Lights page's
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

// ---- Weather: where the car is (ip-api.com) and its weather (Open-Meteo), over the hotspot ----
// Its own task (core 1, idle priority like the voice server, which it never blocks): once WiFi is
// up, at start and every 15 min, GET the place (once per boot, again while it fails), then the
// weather, both plain http (no TLS on a heap shared with NimBLE, WiFi and the server), 5 s
// timeouts, the body into a fixed 2 KB buffer. Each try is published under weatherMux; a failed
// try keeps the last good reading and comes again in 2 min. loop() polls weatherFresh to redraw.
// The place is the phone's public IP's: approximate (the operator's city, sometimes a
// neighbour's), which is fine for the weather.
static constexpr uint32_t WEATHER_EVERY_MS = 15 * 60 * 1000, WEATHER_RETRY_MS = 2 * 60 * 1000;
static TaskHandle_t weatherHandle = nullptr;
static portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
static WeatherReading weatherPub;           // guarded by weatherMux
static volatile bool weatherFresh = false;  // a try was published; loop() clears it
static char weatherBody[2048];              // only the weather task touches it

// GET `url` into weatherBody (cut at 2 KB, always terminated); the HTTP status, negative without an
// answer. HTTP/1.0, so the body is never chunked.
static int weatherGet(const char* url) {
    weatherBody[0] = 0;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(5000);
    h.setConnectTimeout(5000);
    h.useHTTP10(true);
    if (!h.begin(c, url)) return -1;
    const int rc = h.GET();
    size_t len = 0;
    if (rc == 200) {
        WiFiClient* s = h.getStreamPtr();
        readguard::Guard g(millis(), 5000, 8000);
        while (len < sizeof(weatherBody) - 1 && (h.connected() || s->available())) {
            const int a = s->available();
            if (a <= 0) {
                if (g.check(millis()) != readguard::State::Ok) break;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            size_t want = sizeof(weatherBody) - 1 - len;
            if (static_cast<size_t>(a) < want) want = static_cast<size_t>(a);
            len += s->readBytes(weatherBody + len, want);
            g.onData(millis());
        }
    }
    weatherBody[len] = 0;
    h.end();
    return rc;
}

static void publishWeather(const WeatherReading& r) {
    portENTER_CRITICAL(&weatherMux);
    weatherPub = r;
    portEXIT_CRITICAL(&weatherMux);
    weatherFresh = true;
}

static void weatherTask(void*) {
    geoip::Place place{};
    bool placed = false;
    WeatherReading r;  // the last good reading, with this try's outcome
    for (;;) {
        bool ok = false;
        if (!net::isOnline()) {
            Serial.println("[weather] offline");
        } else {
            if (!placed) {
                const int rc = weatherGet(geoip::URL);
                placed = rc == 200 && geoip::parse(weatherBody, &place);
                if (placed) Serial.printf("[weather] place %s (%s, %s)\n", place.city, place.lat, place.lon);
                else Serial.printf("[weather] place failed (http %d)\n", rc);
            }
            if (placed) {
                char url[256];
                weather::url(place.lat, place.lon, url, sizeof(url));
                const int rc = weatherGet(url);
                weather::Now now;
                if (rc == 200 && weather::parse(weatherBody, &now)) {
                    ok = true;
                    r.ok = true;
                    r.tempC = now.tempC;
                    r.code = now.code;
                    r.day = now.day;
                    r.daily = weather::parseDaily(weatherBody, &r.maxC, &r.minC);
                    snprintf(r.city, sizeof(r.city), "%s", place.city);
                    int m = 0;
                    r.at = netclock::minuteOfDay(&m) ? m : -1;
                    Serial.printf("[weather] %s %.1fC %s, hi %.1f lo %.1f%s  (heap %u)\n", r.city, r.tempC,
                                  weather::words(r.code), r.maxC, r.minC, r.daily ? "" : " (no daily)",
                                  static_cast<unsigned>(ESP.getFreeHeap()));
                } else {
                    Serial.printf("[weather] forecast failed (http %d)\n", rc);
                }
            }
        }
        r.failed = !ok;
        publishWeather(r);
        if (!ok) Serial.println("[weather] retry in 2 min");
        vTaskDelay(pdMS_TO_TICKS(ok ? WEATHER_EVERY_MS : WEATHER_RETRY_MS));
    }
}

// From the voice task, once WiFi is up. 6 KB: HTTPClient and its Strings; the [mem] line shows
// what's left.
static void beginWeather() {
    if (weatherHandle) return;
    xTaskCreatePinnedToCore(weatherTask, "weather", 6144, nullptr, 0, &weatherHandle, 1);
}

// In loop(): takes a published try and redraws what shows it, if on screen. The strip's
// temperature only when its text changed.
static void takeWeather() {
    if (!weatherFresh) return;
    weatherFresh = false;  // before the copy: a try published meanwhile raises it again
    portENTER_CRITICAL(&weatherMux);
    wx = weatherPub;
    portEXIT_CRITICAL(&weatherMux);
    if (screen == Screen::Weather) {
        drawWeatherBody();
    } else if (screen == Screen::Menu) {
        char t[sizeof(stripTemp)];
        tempText(t, sizeof(t));
        if (strcmp(t, stripTemp) != 0) drawStripTemp();
    }
}

// ---- Lyrics: what the black deck (the S3) is playing, from its GET /state ----
// Its own task (core 1, idle priority like the weather; core 0 is the lights' and the controller's
// link), started with the weather once WiFi is up. It polls only while a screen that shows it is up
// (loop() keeps lyricsWant): the KARAOKE screen every second, every 5 s after ten misses in a row
// (util/pollpace); the Menu every MENU_POLL_MS (enough to light or dim its KARAOKE item); at once
// when either opens. Otherwise it sleeps. The black deck is at its fixed hotspot
// address (COMPANION_DECK) on the iPhone hotspot, else looked up by its mDNS name (DECK_MDNS_NAME)
// and kept through blips (util/deckaddr). HTTP/1.0, 1.5 s timeouts, the body into a fixed
// statejson::MAX_JSON buffer, util/stateparse. Each poll is published under lyricsMux; loop() polls
// lyricsFresh to redraw. The log tells when the deck is found, lost and back, not every poll.
static TaskHandle_t lyricsHandle = nullptr;
static portMUX_TYPE lyricsMux = portMUX_INITIALIZER_UNLOCKED;
static LyricsReading lyricsPub;             // guarded by lyricsMux
static volatile bool lyricsFresh = false;  // a poll was published; loop() clears it
enum LyricsWant : uint8_t { WANT_NONE, WANT_MENU, WANT_KARAOKE };
static volatile uint8_t lyricsWant = WANT_NONE;  // what the screen shown needs; loop() keeps it
static char lyricsBody[statejson::MAX_JSON];  // only the lyrics task touches these three
static deckaddr::Keep lyricsAddr;
static bool lyricsMdns = false;

// Where the black deck is: its fixed address on the hotspot, else its mDNS name on whatever network
// both are on. False when it can't be found.
static bool lyricsHost(char* out, size_t n) {
    if (hotspotip::onHotspot(hostOrder(WiFi.localIP()), hostOrder(WiFi.gatewayIP()), hostOrder(WiFi.subnetMask()))) {
        snprintf(out, n, "%s", COMPANION_DECK);
        return true;
    }
    if (!lyricsMdns) lyricsMdns = MDNS.begin("cardeck-leds");
    if (lyricsAddr.addr() == 0) {
        const IPAddress a = MDNS.queryHost(DECK_MDNS_NAME, 1500);
        if (static_cast<uint32_t>(a) == 0) return false;
        lyricsAddr.found(static_cast<uint32_t>(a));
        Serial.printf("[lyrics] deck found at %s (mDNS)\n", a.toString().c_str());
    }
    snprintf(out, n, "%s", IPAddress(lyricsAddr.addr()).toString().c_str());
    return true;
}

// GET /state into lyricsBody (cut at MAX_JSON, always terminated); the HTTP status, negative without
// an answer. Short timeouts: the deck is one hop away, and a missing deck must not hold the polls up.
static int lyricsGet() {
    lyricsBody[0] = 0;
    if (!net::isOnline()) return -1;
    char host[40], url[128];
    if (!lyricsHost(host, sizeof(host))) return -1;
    snprintf(url, sizeof(url), "http://%s/state?k=%s", host, REMOTE_TOKEN);
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(1500);
    h.setConnectTimeout(1500);
    h.useHTTP10(true);
    if (!h.begin(c, url)) return -1;
    const int rc = h.GET();
    size_t len = 0;
    if (rc == 200) {
        WiFiClient* s = h.getStreamPtr();
        readguard::Guard g(millis(), 1500, 3000);
        while (len < sizeof(lyricsBody) - 1 && (h.connected() || s->available())) {
            const int a = s->available();
            if (a <= 0) {
                if (g.check(millis()) != readguard::State::Ok) break;
                vTaskDelay(pdMS_TO_TICKS(2));
                continue;
            }
            size_t want = sizeof(lyricsBody) - 1 - len;
            if (static_cast<size_t>(a) < want) want = static_cast<size_t>(a);
            len += s->readBytes(lyricsBody + len, want);
            g.onData(millis());
        }
    }
    lyricsBody[len] = 0;
    h.end();
    return rc;
}

static void lyricsTask(void*) {
    static stateparse::Deck deck;  // static: ~650 B, off the task stack
    pollpace::Pace pace;
    uint32_t lastPoll = 0;
    uint8_t was = WANT_NONE;  // what was wanted at the last poll
    for (;;) {
        const uint8_t want = lyricsWant;
        if (want == WANT_NONE) {
            was = WANT_NONE;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const uint32_t now = millis();
        const uint32_t every = want == WANT_MENU ? MENU_POLL_MS : pace.intervalMs();
        if (want != was || now - lastPoll >= every) {
            if (want != was) pace = pollpace::Pace();  // just opened: poll at once, at the fast pace
            was = want;
            lastPoll = now;
            const int rc = lyricsGet();
            const bool ok = rc == 200 && stateparse::parse(lyricsBody, deck);
            if (pace.misses() == 0 && !ok) Serial.printf("[lyrics] deck lost (rc=%d)\n", rc);
            if (pace.misses() > 0 && ok) Serial.println("[lyrics] deck back");
            pace.onResult(ok);
            lyricsAddr.onPoll(rc > 0);  // 3 misses in a row: look the name up again
            const uint32_t at = millis();
            portENTER_CRITICAL(&lyricsMux);
            lyricsPub.wrongKey = rc == 403;
            if (ok) {
                lyricsPub.deck = deck;
                lyricsPub.at = at ? at : 1;
            }
            portEXIT_CRITICAL(&lyricsMux);
            lyricsFresh = true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// From the voice task, once WiFi is up. 6 KB: HTTPClient, its Strings and the mDNS query; the [mem]
// line shows what's left.
static void beginLyrics() {
    if (lyricsHandle) return;
    xTaskCreatePinnedToCore(lyricsTask, "lyrics", 6144, nullptr, 0, &lyricsHandle, 1);
}

// In loop(): tells the task how to poll, takes a published poll, and redraws what changed: on the
// Menu the KARAOKE item when it turns lit or dimmed (a state also goes stale with time), on KARAOKE
// the parts of the screen that changed (the line and the bar move between polls, so this looks
// every KARAOKE_TICK_MS).
static void lyricsTick() {
    lyricsWant = screen == Screen::Karaoke ? WANT_KARAOKE : screen == Screen::Menu ? WANT_MENU : WANT_NONE;
    bool fresh = false;
    if (lyricsFresh) {
        lyricsFresh = false;  // before the copy: a poll published meanwhile raises it again
        portENTER_CRITICAL(&lyricsMux);
        song = lyricsPub;
        portEXIT_CRITICAL(&lyricsMux);
        fresh = true;
    }
    if (screen == Screen::Menu) {
        const int item = M_ITEM0 + deckmenu::KARAOKE;
        if (karaokeReady() != karaokeShownReady) drawButton(item, pressedNow(item));
        return;
    }
    if (screen != Screen::Karaoke) return;
    static uint32_t drawnAt = 0;
    if (!fresh && millis() - drawnAt < KARAOKE_TICK_MS) return;
    drawnAt = millis();
    drawKaraokeBody(false);
}

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"). Without a zone it acts like the BOTH page (Siri: spoken zone words are
// ignored); &zone=inside|outside (the Garmin app) acts on that zone's page only.
// GET /state?k=[&zone=] answers that zone's line (level included) without changing anything.
// A preset ("rose", "preset 2") is applied to both lights and answered with its line; the rest of
// that phrase (a colour, a level, the zone) is ignored. GET /presets?k= lists the four presets (one
// ledpresets::wireLine each), GET /status?k= each light's state.
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
// 112: the longest preset line ("Purpura: inside Purple 40 percent, outside ...") fits.
static char voiceReply[112];  // guarded by voiceMux, with replySeq
static uint32_t replySeq = 0;

// What /state and /status read (they run on the server task): loop() copies it from its own state
// after every pass (and before each voice reply), under stateMux, so a handler never sees a
// half-written look.
struct Published {
    Look looks[3];  // indexed by Target
    bool up[N_CTRL];
};
static Published published;
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

static void publish() {
    Published p;
    for (int i = 0; i < 3; i++) p.looks[i] = looks[i];
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
// given); INSIDE or OUTSIDE changes only that light; a preset sets both lights whatever the
// zone, and a preset ignores the rest of the phrase ("rose 50" is ROSE as it is). The page on
// screen stays. Then answers the waiting handler.
static void applyVoice(const VoiceJob& job) {
    const ledvoice::Command& c = job.c;
    char line[sizeof(voiceReply)];
    if (c.preset >= 1 && c.preset <= ledpresets::N) {  // "rose", "preset 2": both lights, whatever zone was asked
        applyPreset(c.preset - 1);
        ledpresets::describe(c.preset, line, sizeof(line));
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

// The four presets, one wireLine each ("1 ROSE pink 40 pink 100"), joined by \n. A fixed table: no
// snapshot needed.
static void handlePresets() {
    if (!gate.keyOk(server->arg("k").c_str())) { server->send(403, "text/plain", "Wrong key"); return; }
    char body[ledpresets::N * 32] = "";
    size_t len = 0;
    for (int i = 0; i < ledpresets::N && len < sizeof(body); i++) {
        char l[32];
        ledpresets::wireLine(i + 1, l, sizeof(l));
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
    beginWeather();
    beginLyrics();
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
        weatherOff = true;  // no WiFi without it
        return;
    }
    voiceQueue = xQueueCreate(1, sizeof(VoiceJob));
    voiceDone = xSemaphoreCreateBinary();
    server = new WebServer(80);
    server->on("/say", HTTP_ANY, handleSay);
    server->on("/state", HTTP_GET, handleState);
    server->on("/presets", HTTP_GET, handlePresets);
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
    typeHeader();  // before the tasks start: nothing else draws while it types
    showLastTrip();  // after the typing, so its few seconds start when the Menu shows
    drawAll(true);  // the typed header stays on screen
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
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second of the loop
    // (not of the boot: typeHeader() eats ~0.6 s of that before loop() ever runs).
    static uint32_t loopStart = 0;
    if (loopStart == 0) loopStart = millis() | 1;
    const bool down = touch::read(&x, &y) && millis() - loopStart > 1000;
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
        tap.down = false;
        if (tap.id >= 0) drawButton(tap.id);  // released: back to the normal look
    }

    // the message has had its time: the usual line comes back (show() already dropped one from
    // another screen)
    if (message[0] && millis() - messageAt >= messageMs) {
        message[0] = 0;
        drawLightState();
    }

    VoiceJob voice;
    if (voiceQueue && xQueueReceive(voiceQueue, &voice, 0) == pdTRUE) applyVoice(voice);
    flushSave();

    // every minute: free heap and the least stack each task has had to spare
    static uint32_t memAt = 0;
    if (millis() - memAt > 60000) {
        memAt = millis();
        Serial.printf("[mem] heap %u (min %u)  stack left: link %u, voice %u, weather %u, lyrics %u, loop %u\n",
                      static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                      linkHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(linkHandle)) : 0u,
                      voiceHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(voiceHandle)) : 0u,
                      weatherHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(weatherHandle)) : 0u,
                      lyricsHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(lyricsHandle)) : 0u,
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    handlePad();
    takeWeather();
    lyricsTick();
    updateDayNight();
    tripTick();
    blinkCursor();

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
    static uint32_t edgeAt[N_CTRL] = {};  // when each light last changed link state, and which way
    static bool edgeUp[N_CTRL] = {};
    for (int i = 0; i < N_CTRL; i++) {
        Controller& c = controllers[i];
        const bool up = linkUp(c);
        if (up && c.fresh) {
            c.fresh = false;
            push(i);
        }
        if (up != shown[i]) {
            shown[i] = up;
            char m[sizeof(message)];
            // both lights going the same way together (the phone hotspot, the car's power): one
            // message, or the second would replace the first
            const int o = 1 - i;
            edgeAt[i] = millis() | 1;
            edgeUp[i] = up;
            if (edgeAt[o] && millis() - edgeAt[o] < 150 && edgeUp[o] == up) lightstatus::linkChangeBoth(up, m, sizeof(m));
            else lightstatus::linkChange(i == 0, up, m, sizeof(m));
            // the boot's ">LAST TRIP" has its 6 s: a link change doesn't cut it short (the strip still redraws)
            if (!(message[0] && messageMs > MESSAGE_MS && millis() - messageAt < messageMs)) setMessage(m);
            drawLightState();  // the message now, then STATUS on Lights / the strip on the Menu: "--" while it's gone
        }
    }
    publish();  // for /state and /status
    delay(10);
}
