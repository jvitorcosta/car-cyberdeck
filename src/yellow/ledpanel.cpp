// The yellow deck (CYD) as the car's control screen: a Seegson-style terminal (pale green on black,
// red only for problems, amber at night, faint scanlines, all caps) that drives the car's two
// ELK-family LED controllers. Three screens, laid out by util/deckmenu:
//   Menu (boot, after the CYBERDECK welcome types out): a dashboard of stacked bands, for what the
//           car's panel and CarPlay don't show: the trip and the lights. The header line
//           (CYBERDECK // HONDA CITY TOURING 2022); the clock big with the date under it ("FRI 23
//           OCT"), and right of it the outside temperature big and the sky in words (see "Weather"
//           below), "HI 32 LO 26  MANAUS" right-aligned under them; the trip: TRIP, its time big and
//           the distance right ("12.4 KM", "-- KM" in DIM while no speed source feeds it); both lights
//           on one row, "IN [chip] ROSE 40%" | "OUT [chip] YELLOW 100%" (OFF, or -- NO LINK when the
//           deck has no link to it); the prompt line: a message, music mode's line, or just ">",
//           with a block cursor (blinking while a message shows); and at the bottom the three
//           items "[LIGHTS]", "[LYRICS]" (with a LIVE tag while the black deck plays a song with
//           synced lyrics, dimmed otherwise: a tap then only says why) and "MUSIC MODE" (a tap turns
//           music mode on or off, an ON or OFF tag under it; see "Music mode" below). The song
//           itself is on the Karaoke screen only
//   Lights: < BACK, the pages BOTH / INSIDE / OUTSIDE / PRESETS as Pip-Boy tabs (BOTH sets both
//           lights together; INSIDE and OUTSIDE each keep their own look), the status line ("LINK
//           2/2", or "OUTSIDE: NO LINK" in ALERT), then on the three look pages 2x4 colour buttons
//           (tap one to set it and turn on, double-tap the lit one to turn the lights off) and
//           [-]  [level: a 10-cell block bar and the %, OFF, or MIX on BOTH while the two lights
//           differ]  [+]; on PRESETS the six presets, two rows of three, each its name over its
//           inside and outside chips (tap: apply, ">ROSE APPLIED"; the tab stays). GOJIRA's name is
//           ゴジラ on screen (">ゴジラ APPLIED"): three Unifont glyphs (ui/gojiraname), as VT323 has
//           no kana; "GOJIRA" on the wire, in the logs and the voice replies. Its colour, light blue,
//           is a preset's only: not on the grid, never a song colour; the theme takes blue's for it
//   Karaoke: < BACK and "TITLE - ARTIST" of the song the black deck (the S3) is playing, its synced
//           lyric line big, the next line under it, and a thin progress line (or >NO SYNCED LYRICS /
//           >NOTHING PLAYING / >LYRICS: NO SIGNAL / >WRONG KEY big in place of the lyrics). Read
//           from the black deck's GET /state every second while this screen shows, every 5 s while
//           the Menu shows (see "Lyrics" below)
// A press acts once it settles (its first ~40 ms averaged), and a tap in a gap snaps to the nearest
// button; it shows as the bar with a ring cut in it (ui/presslook). After a screen or page
// change touch waits for the finger to lift and 250 ms (util/tapguard). The iPhone's Bluetooth
// must be off (Settings): each controller takes one connection.
// The STATUS line on Lights and the prompt line on the Menu show a terminal message for 2.5 s
// (">ROSE APPLIED", ">INSIDE: NO LINK"), then their usual line again (on the Menu: ">" or music mode's line).
// Each page's colour, intensity and power and the page last shown survive a reboot (NVS); the deck
// still boots to the Menu. Touch calibration (resistive panel) runs on first boot,
// or with BOOT held (at power-up, or 2 s any time). Voice and the Garmin watch: GET /say, /state,
// /presets and /status over WiFi (see "Voice" below), on either screen. An Xbox controller (BLE)
// drives it too: the D-pad moves the focus (an inverted bar on the Menu's three items, a TEXT ring on
// Lights and Karaoke; none is drawn while no controller is connected), A presses, B goes back to the
// Menu; on Lights X is the next page, and on the look pages Y on/off, LB / RB - / + (see "Xbox
// controller input" below).
// Night mode (like the black deck): 18:00-06:00 local time, once WiFi has synced the clock, the
// palette goes to termpal's amber and the backlight to its night level. Without REMOTE_TOKEN
// there is no WiFi, no clock, and the deck stays in day mode.
// Theme: while the INSIDE light is on and linked, the terminal (Menu, Lights, Karaoke; not the boot
// typing or the calibration) takes that light's colour, termpal's three levels of it, day and night
// (white turns to the amber at night; problems show in white on the red, orange and pink themes).
// The light off or unreachable: the green / amber again. The colour tiles and chips stay real.
// Music mode ("song colours"): the MUSIC item has the black deck (the S3) send the song's palette over
// UDP (util/musicwire); both lights then take its main colour snapped to one of the grid's 8 swatches
// (util/musiclights), steady, fading to the next song's over ~3 s, each at the owner's level under the
// palette's time-of-day cap (never under 40 %). An outside light left off stays off; an inside one
// off is switched on. The theme follows the swatch. The prompt line shows ">MUSIC", a chip and the
// swatch's name (">MUSIC: WAITING" before the first palette), or ">MUSIC: NO SIGNAL" when no palette
// came for 15 s. Any manual change (touch, the Xbox controller, voice, the watch) ends the mode,
// then applies; turning it off brings back the looks from before. Not kept over a reboot (see
// "Music mode" below).
// Test hooks: building with -D NIGHT_TEST (e.g. PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST") forces night
// mode whatever the time, to check the dimming without waiting for 18:00; -D THEME_TEST=<swatch 0..7>
// (ledlook's order: red, orange, pink, purple, blue, green, white, yellow) forces that theme whatever
// the inside light does, to see it without the car's lights. Neither is set in platformio.ini (every
// test hook: docs/ARCHITECTURE.md "Test hooks").
// Updates over WiFi (net/ota, like the black deck): once online, from a PC on the iPhone hotspot,
//   pio run -e ledpanel_ota -t upload
// A page in the theme holds the screen meanwhile (see "Update over WiFi" below); three crash resets
// in a row before new firmware has proven itself boot the previous one (util/bootguard).
// Over USB (the first time, or to recover):
//   pio run -e ledpanel -t upload
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <cstddef>
#include <type_traits>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // NimBLE 2's and the Xbox library's headers;
#pragma GCC diagnostic ignored "-Wdouble-promotion"  // src/ builds with -Werror
#include <NimBLEDevice.h>
#include <XboxPad.hpp>  // lib/xboxpad: asukiaaa's Xbox library, vendored with shorter timeouts
#pragma GCC diagnostic pop
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <lwip/sockets.h>
#include "pins.h"
#include "../common/ui/backlight.h"
#include "../common/core/embed.h"
#include "../common/core/watchdog.h"
#include "ui/gojiraname.h"
#include "ui/jpfont.h"
#include "ui/lyricrun.h"
#include "ui/presslook.h"
#include "ui/termfont.h"
#include "ui/termlayout.h"
#include "touch.h"
#include "../common/net/clock.h"
#include "../common/net/httpget.h"
#include "../common/net/ipaddr.h"
#include "../common/net/ota.h"
#include "../common/net/remote_config.h"
#include "../common/net/reqauth.h"
#include "../common/net/wifi.h"
#include "../util/yellow/bothlook.h"
#include "../util/yellow/dashtext.h"
#include "../util/common/daynight.h"
#include "../util/yellow/deckaddr.h"
#include "../util/yellow/deckmenu.h"
#include "../util/yellow/elkcmd.h"
#include "../util/yellow/geoip.h"
#include "../util/common/hotspotip.h"
#include "../util/lights/ledlook.h"
#include "../util/yellow/ledpresets.h"
#include "../util/yellow/ledvoice.h"
#include "../util/lights/musiclights.h"
#include "../util/wire/musicwire.h"
#include "../util/yellow/otapage.h"
#include "../util/yellow/lightstatus.h"
#include "../util/yellow/pollpace.h"
#include "../util/yellow/karaoke.h"
#include "../util/common/remotegate.h"
#include "../util/wire/statejson.h"
#include "../util/wire/stateparse.h"
#include "../util/yellow/tapguard.h"
#include "../util/yellow/termpal.h"
#include "../util/common/version.h"
#include "../util/yellow/touchcal.h"
#include "../util/lights/tripclock.h"
#include "../util/yellow/tripmeter.h"
#include "../util/yellow/weather.h"

static TFT_eSPI tft;
static Preferences prefs;

// The panel's name on a network other than the hotspot (http://cardeck-leds.local/): the lyrics
// task's mDNS and the update listener's hostname.
#define LEDS_MDNS_NAME "cardeck-leds"

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
static Target target = BOTH;  // the look page on screen (while PRESETS shows: the last one shown)
// The PRESETS tab is a page of no look: while it shows, the screen is deckmenu's Presets (the same
// < BACK and tabs, then the presets) and `target` keeps the look page it came from. LIGHTS on the
// Menu opens the page last shown, this one too.
static bool presetsTab = false;
static int currentTab() { return presetsTab ? deckmenu::TAB_PRESETS : static_cast<int>(target); }

static Look looks[3];  // indexed by Target; controller i shows looks[i + 1]
static Look& cur() { return looks[target]; }
// The BOTH page is on screen while the lights differ (after a preset or the INSIDE / OUTSIDE
// pages): it shows no selected colour and "MIX" for the level.
static bool bothMixed() { return target == BOTH && bothlook::mixed(looks[INSIDE], looks[OUTSIDE]); }

using deckmenu::Rect;
using deckmenu::Screen;
using deckmenu::M_ITEM0;
using deckmenu::L_BACK;
using deckmenu::L_TAB0;
using deckmenu::L_TILE0;
using deckmenu::L_MINUS;
using deckmenu::L_PLUS;
using deckmenu::K_BACK;
using deckmenu::P_PRE0;

static Screen screen = Screen::Menu;  // the screen shown; boot lands on the Menu
// Lights on any of its tabs (Presets is its PRESETS tab).
static bool onLights() { return screen == Screen::Lights || screen == Screen::Presets; }
// The palette: every colour goes through termpal with this flag, so night mode only flips it and
// redraws.
static bool night = false;
// The theme (see "Theme" above): a ledlook swatch index, or -1 for the green / amber. Loop-owned, as
// `night`; updateTheme() keeps it, from the looks and `insideSeen`: the INSIDE light's link as loop()
// last took it (its edge loop sets it before anything draws the change).
static int theme = -1;
static bool insideSeen = false;
static bool updateTheme();

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
static volatile bool lyricsNudge = false;         // loop(): poll now, the line shown changed; the task clears it
static constexpr uint32_t LYRICS_NUDGE_MS = 300;  // least gap between a poll and a nudged one

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

// Music mode (see "Music mode" below): on and off from the Menu's MUSIC item, never saved (boots
// off). musicOn is loop()'s; musicWant is the same for the lyrics task (the keep-alive) and the music
// task (the UDP socket). musicState and musicSwatch are what the prompt line draws (musicSwatch -1
// before a palette); musicTick() keeps them.
static bool musicOn = false;
static volatile bool musicWant = false;
static uint32_t musicOnAt = 0;  // millis() it went on
static musiclights::State musicState = musiclights::State::Waiting;
static int musicSwatch = -1;
// A manual change while music mode is on ends it first (see "Music mode" below).
static bool musicYield();

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

// What the strip gets for swatch `sw`: its SWATCHES LED mix.
struct Rgb { uint8_t r, g, b; };
static Rgb ledRgb(int sw) { return {SWATCHES[sw].r, SWATCHES[sw].g, SWATCHES[sw].b}; }

// What a chip of swatch `sw` shows on screen: its screen colour.
static uint32_t screenOf(int sw) { return SWATCHES[sw].screen; }

// Music mode's looks (see "Music mode" below): from the first song palette after the mode goes on
// until it goes off, light i shows musicLook[i] (the song's swatch at its music level) instead of
// its page's look. The pages' looks stay as the owner left them and are never saved over, so the
// mode's end (or a reboot) brings them back. Loop-owned.
static bool musicDriving = false;
static Look musicLook[N_CTRL];
static Look shownLook(int i) { return musicDriving ? musicLook[i] : looks[i + 1]; }

// Controller i's look (power + dimmed colour), both header variants.
static void push(int i) {
    Controller& c = controllers[i];
    const Look l = shownLook(i);
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(c, elkcmd::power(v, l.lit));
    if (!l.lit) return;
    const Rgb s = ledRgb(l.swatch);
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) {
        write(c, elkcmd::color(v, elkcmd::dim(s.r, l.pct), elkcmd::dim(s.g, l.pct), elkcmd::dim(s.b, l.pct)));
    }
}

// One change on `page` (a tap, Y, voice): on BOTH it goes to both lights (bothlook), on INSIDE or
// OUTSIDE to that light's look only; then the lights it covers get their looks (both, when it ended
// music mode: the other one goes back to its page's look too).
static void change(Target page, const bothlook::Change& c) {
    const bool restore = musicYield();
    if (page == BOTH) bothlook::apply(c, &looks[BOTH], &looks[INSIDE], &looks[OUTSIDE]);
    else bothlook::edit(&looks[page], c);
    for (int i = 0; i < N_CTRL; i++) {
        if (restore || page == BOTH || page == static_cast<Target>(i + 1)) push(i);
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
//    its 4 s scans every 15 s (it scans flat out);
//  - an update over WiFi (net/ota) has the radio: no connects and no scan (a running one stops)
//    until it ends. Connected lights and a connected controller stay as they are (the library
//    needs no tick for that: its reports come in callbacks).
static constexpr uint32_t PAD_SCAN_EVERY_MS = 15000;
static TaskHandle_t linkHandle = nullptr;

static void linkTask(void*) {
    uint32_t waitMs = 5000, nextLights = 0, lastPadScan = 0;
    watchdog::watch();  // a NimBLE wait that never returns reboots the deck (common/core/watchdog)
    for (;;) {
        watchdog::feed();
        if (ota::active()) {
            NimBLEScan* scan = NimBLEDevice::getScan();
            if (scan->isScanning()) scan->stop();
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const uint32_t now = millis();
        const bool padBusy = xbox.isConnected() || XboxSeriesXControllerESP32_asukiaaa::advDevice != nullptr ||
                             NimBLEDevice::getScan()->isScanning();
        if (padBusy) {
            xbox.onLoop();
        } else if (now - lastPadScan > PAD_SCAN_EVERY_MS) {
            lastPadScan = now;
            xbox.onLoop();  // starts a scan
        }
        watchdog::feed();  // the controller's connect and discovery: ~62 s at worst

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
                watchdog::feed();  // a light's connect and discovery: ~45 s at worst
                if (!linkUp(c) && !connect(c)) still = true;
            }
            nextLights = millis() + waitMs;
            waitMs = still ? (waitMs * 2 > 60000 ? 60000 : waitMs * 2) : 5000;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---- Buttons (320x240 landscape, laid out by util/deckmenu) ----

// The grid is all colours (ledlook's first N_GRID; light blue is a preset's only); there is no power
// tile: double-tapping the lit colour turns the lights off, and tapping any colour turns them back on.
static_assert(ledlook::N_GRID == 8 && L_MINUS - L_TILE0 == ledlook::N_GRID, "the grid is 2x4 colours");
static_assert(deckmenu::P_N - P_PRE0 == ledpresets::N, "a button per preset");
static_assert(static_cast<int>(deckmenu::TAB_BOTH) == BOTH && static_cast<int>(deckmenu::TAB_INSIDE) == INSIDE &&
                  static_cast<int>(deckmenu::TAB_OUTSIDE) == OUTSIDE,
              "the look pages' tabs come first, in Target order");

static bool built(int id) { return deckmenu::rect(screen, id).w > 0; }

// The button the Xbox controller points at; -1 while none is connected. On the Menu it is drawn as
// an inverted bar, on Lights as a TEXT ring around the button (the lit colour is the bar there).
static int focus = -1;
static bool padUp = false;  // an Xbox controller is connected and reporting
static void drawFocus(int id, bool on);

// Where the focus starts on each screen: the first item, the first preset, or the colour the page
// shows (blue for light blue, which has no tile).
static int homeFocus() {
    if (screen == Screen::Menu) return M_ITEM0;
    if (screen == Screen::Karaoke) return K_BACK;
    if (screen == Screen::Presets) return P_PRE0;
    return L_TILE0 + ledlook::gridSwatch(cur().swatch);
}

// ---- Drawing ----
// The lettering is VT323, a CRT terminal font, 1-bit at four sizes named by role (ui/termfont:
// SMALL, BODY, TITLE, BIG); where each label goes so it fits is ui/termlayout's, which
// test_termfont checks. Every label is upper case (the lyrics keep theirs). The touch calibration
// screens keep TFT_eSPI's built-in fonts.
// The background is black with a faint scanline on every 3rd screen row: every clear goes through
// clearBg(), which repaints a rect's lines on absolute rows, so a partial redraw stays aligned.
// Text over it is drawn transparent (free fonts always are: no black box punching through the
// lines); the solid fills (the inverted selection bar, a press's ringed bar, the chips) are the
// design.
using termfont::Face;
using namespace termlayout;

// termfont's tables are GFXfont's, so TFT_eSPI draws them as they are.
static_assert(sizeof(termfont::Glyph) == sizeof(GFXglyph) && offsetof(termfont::Glyph, bitmapOffset) == offsetof(GFXglyph, bitmapOffset) &&
                  offsetof(termfont::Glyph, width) == offsetof(GFXglyph, width) && offsetof(termfont::Glyph, height) == offsetof(GFXglyph, height) &&
                  offsetof(termfont::Glyph, xAdvance) == offsetof(GFXglyph, xAdvance) &&
                  offsetof(termfont::Glyph, xOffset) == offsetof(GFXglyph, xOffset) && offsetof(termfont::Glyph, yOffset) == offsetof(GFXglyph, yOffset),
              "termfont::Glyph is GFXglyph");
static_assert(sizeof(termfont::Font) == sizeof(GFXfont) && offsetof(termfont::Font, bitmap) == offsetof(GFXfont, bitmap) &&
                  offsetof(termfont::Font, glyph) == offsetof(GFXfont, glyph) && offsetof(termfont::Font, first) == offsetof(GFXfont, first) &&
                  offsetof(termfont::Font, last) == offsetof(GFXfont, last) && offsetof(termfont::Font, yAdvance) == offsetof(GFXfont, yAdvance),
              "termfont::Font is GFXfont");
// and field for field the same types (pointers aside: termfont's point to const)
static_assert(std::is_same<decltype(termfont::Glyph::bitmapOffset), decltype(GFXglyph::bitmapOffset)>::value &&
                  std::is_same<decltype(termfont::Glyph::width), decltype(GFXglyph::width)>::value &&
                  std::is_same<decltype(termfont::Glyph::height), decltype(GFXglyph::height)>::value &&
                  std::is_same<decltype(termfont::Glyph::xAdvance), decltype(GFXglyph::xAdvance)>::value &&
                  std::is_same<decltype(termfont::Glyph::xOffset), decltype(GFXglyph::xOffset)>::value &&
                  std::is_same<decltype(termfont::Glyph::yOffset), decltype(GFXglyph::yOffset)>::value,
              "termfont::Glyph's fields are GFXglyph's");
static_assert(std::is_same<std::remove_const<std::remove_pointer<decltype(termfont::Font::bitmap)>::type>::type,
                           std::remove_pointer<decltype(GFXfont::bitmap)>::type>::value &&
                  std::is_same<decltype(termfont::Font::first), decltype(GFXfont::first)>::value &&
                  std::is_same<decltype(termfont::Font::last), decltype(GFXfont::last)>::value &&
                  std::is_same<decltype(termfont::Font::yAdvance), decltype(GFXfont::yAdvance)>::value,
              "termfont::Font's fields are GFXfont's");

// Draws s in face f at (x, y), in the text colour set, and returns the pen's travel (where text drawn
// after it starts). `datum` is one of TFT_eSPI's nine (TL_DATUM .. BR_DATUM): its left / centre /
// right part places x as usual, but its top / middle / bottom part goes by the capitals, not by the
// font's tallest glyph as TFT_eSPI's free-font datums do (that set '(' and '[' high and every label
// low): T puts the caps' top row at y, M centres the caps on y, B puts the baseline (the row under
// the caps) at y. setFreeFont scans the font, so it runs only when the face changes. GOJIRA's mark
// in s is drawn as ゴジラ (textMarked).
static int textMarked(const char* s, int x, int y, Face f, uint8_t datum);
static int text(const char* s, int x, int y, Face f, uint8_t datum) {
    if (strchr(s, gojiraname::MARK)) return textMarked(s, x, y, f, datum);
    static int shown = -1;
    if (static_cast<int>(f) != shown) {
        shown = static_cast<int>(f);
        tft.setFreeFont(reinterpret_cast<const GFXfont*>(&termfont::font(f)));
    }
    const int cap = termfont::capHeight(f), v = datum / 3;
    const int base = v == 0 ? y + cap : v == 1 ? y + cap / 2 : y;
    tft.setTextDatum(L_BASELINE + datum % 3);
    return tft.drawString(s, x, base);
}

// text() for s holding GOJIRA's mark: the whole placed by gojiraname's width, the text around the
// mark drawn as usual (on the baseline, left datum), the mark as the three glyphs in the text colour
// sitting on the baseline (gojiraname::rows / top). Returns the pen's travel. A run of text between
// marks longer than MARKED_RUN - 1 bytes would be cut: every text it gets (a message, a preset's
// name) is shorter (static_assert at `message`).
static constexpr size_t MARKED_RUN = 64;
static int textMarked(const char* s, int x, int y, Face f, uint8_t datum) {
    const int w = gojiraname::width(f, s), h = datum % 3;
    const int x0 = h == 0 ? x : h == 1 ? x - w / 2 : x - w;
    const int cap = termfont::capHeight(f), v = datum / 3;
    const int base = v == 0 ? y + cap : v == 1 ? y + cap / 2 : y;
    const gojiraname::Rows rows = gojiraname::rows(f);
    const int top = gojiraname::top(f, base);
    const uint16_t ink = static_cast<uint16_t>(tft.textcolor);
    int pen = x0;
    char run[MARKED_RUN];
    size_t n = 0;
    for (const char* p = s;; p++) {
        if (*p && *p != gojiraname::MARK) {
            if (n + 1 < sizeof(run)) run[n++] = *p;
            continue;
        }
        if (n > 0) {
            run[n] = 0;
            pen += text(run, pen, base, f, BL_DATUM);
            n = 0;
        }
        if (!*p) break;
        for (int i = 0; i < gojiraname::N_GLYPHS; i++) {
            tft.drawBitmap(pen + i * gojiraname::GLYPH_W, top, gojiraname::glyph(i) + 2 * rows.first, gojiraname::GLYPH_W,
                           rows.last - rows.first + 1, ink);
        }
        pen += gojiraname::WIDTH;
    }
    return pen - x0;
}

static uint16_t c565(uint32_t rgb) { return tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static uint16_t pal(termpal::Role r) { return c565(termpal::rgb(r, night, theme)); }
// A terminal message's colour: ALERT for a problem (a lost link, the wrong key), else TEXT.
static bool isAlertMsg(const char* m) { return lightstatus::isAlert(m) || karaoke::isAlert(m); }
static uint16_t msgInk(const char* m) { return pal(isAlertMsg(m) ? termpal::ALERT : termpal::TEXT); }

// A real colour (a chip), dimmed with the palette at night.
static uint16_t tint(uint32_t rgb) { return c565(termpal::tile(rgb, night)); }

static void clearBg(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.fillRect(x, y, w, h, pal(termpal::BG));
    const uint16_t line = c565(termpal::scan(night));
    for (int row = y + (3 - y % 3) % 3; row < y + h; row += 3) tft.drawFastHLine(x, row, w, line);
}
static void clearBg(const Rect& r) { clearBg(r.x, r.y, r.w, r.h); }

// A swatch name ("Orange") as the screen shows it ("ORANGE").
static void upper(const char* s, char* out, size_t n) {
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) out[i] = static_cast<char>(toupper(static_cast<unsigned char>(s[i])));
    out[i] = 0;
}

// A press's BG ring (ui/presslook) cut into the bar over the rect (x, y, w, h).
static void drawPressRing(int x, int y, int w, int h) {
    for (int k = presslook::INSET; k < presslook::INSET + presslook::RING; k++) {
        tft.drawRect(x + k, y + k, w - 2 * k, h - 2 * k, pal(termpal::BG));
    }
}

// The box of a terminal button, as ui/presslook says: the scanlined bg inside a MID border (`border`
// px); the solid TEXT bar when selected, 1 px in from the edge so a Lights focus ring around it stays
// apart; pressed (selected or not), the bar with a BG ring in it. Theme colours only, so a press shows
// on every theme. Returns the ink for what goes on it (BG on a bar, else `ink`).
static uint16_t drawBox(const Rect& r, bool pressed, bool selected, int border, uint16_t ink) {
    clearBg(r);
    const presslook::Box b = presslook::box(pressed, selected);
    if (b.bar) {
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, pal(termpal::TEXT));
        if (b.ring) drawPressRing(r.x, r.y, r.w, r.h);
        return pal(termpal::BG);
    }
    for (int k = 0; k < border; k++) tft.drawRect(r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k, pal(termpal::MID));
    return ink;
}

// A bordered button (< BACK, [-], [+]): the label centred.
static void drawChrome(const Rect& r, const char* s, Face f, bool pressed, bool inverted) {
    const uint16_t ink = drawBox(r, pressed, inverted, 2, pal(termpal::TEXT));
    tft.setTextColor(ink);
    text(s, r.x + r.w / 2, r.y + r.h / 2, f, MC_DATUM);
}

// A Menu item ("[LIGHTS]"): a MID outline (it stands among the dashboard's text), the text top left;
// the inverted bar when focused. Only the Xbox controller focuses (focus is -1 without it), so
// without one the items look alike and the clock and the lights band lead. Dimmed (an item that
// won't open now): DIM text in a DIM border, the bar in DIM; a press keeps that look (only a second
// DIM outline inside the border), since it won't open: the message is the feedback. `tag` (LYRICS's
// LIVE while it opens, MUSIC MODE's ON / OFF; nullptr for none): under the text at the right edge, an
// inverted mini-label in the text's ink with the ground's colour for letters (TEXT, not ALERT: red
// is for problems).
static void drawRow(const Rect& r, const char* s, bool pressed, bool inverted, bool dim, const char* tag) {
    uint16_t ink;
    if (dim) {
        clearBg(r);
        if (inverted) {
            tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, pal(termpal::DIM));
            ink = pal(termpal::BG);
        } else {
            tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::DIM));
            if (pressed) tft.drawRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, pal(termpal::DIM));
            ink = pal(termpal::DIM);
        }
    } else {
        ink = drawBox(r, pressed, inverted, 1, pal(termpal::TEXT));
    }
    tft.setTextColor(ink);
    text(s, r.x + ROW_TEXT_X, r.y + ROW_TEXT_Y, Face::BODY, TL_DATUM);
    if (!tag) return;
    const int w = termfont::width(Face::SMALL, tag) + 2 * TAG_PAD, h = termfont::capHeight(Face::SMALL) + 2 * TAG_PAD;
    const int x = r.x + r.w - TAG_RIGHT - w, y = r.y + r.h - TAG_BOTTOM - h;
    tft.fillRect(x, y, w, h, ink);
    const bool onBar = dim ? inverted : presslook::box(pressed, inverted).bar;  // drawBox's bar
    tft.setTextColor(pal(onBar ? termpal::TEXT : termpal::BG));  // the ground's
    text(tag, x + w / 2, y + h / 2, Face::SMALL, MC_DATUM);
}

// A colour chip of `l` (dimmed at night like any real colour): filled when lit, a DIM outline when
// off. On a bar, a 1 px BG edge keeps it apart from the fill (pink on yellow, white on green).
static void drawChip(int x, int y, int w, int h, const Look& l, bool onBar) {
    if (l.lit) tft.fillRect(x, y, w, h, tint(screenOf(l.swatch)));
    else tft.drawRect(x, y, w, h, pal(termpal::DIM));
    if (onBar) tft.drawRect(x - 1, y - 1, w + 2, h + 2, pal(termpal::BG));
}

// A preset on the PRESETS tab (100x60): the name (TITLE) over two chips big enough to tell apart at
// a glance (inside, outside); drawBox's pressed look. The Xbox focus is the ring around it.
static void drawPresetSlot(const Rect& r, int slot, bool pressed) {
    const ledpresets::Preset& p = ledpresets::PRESETS[slot];
    const uint16_t ink = drawBox(r, pressed, false, 1, pal(termpal::TEXT));
    const Look* sides[] = {&p.in, &p.out};
    tft.setTextColor(ink);
    const int cx = r.x + r.w / 2, x0 = cx - (2 * PRESET_CHIP + PRESET_CHIP_GAP) / 2;
    text(ledpresets::shown(p), cx, r.y + PRESET_NAME_Y, Face::TITLE, TC_DATUM);  // ゴジラ for GOJIRA
    for (int k = 0; k < 2; k++) {
        drawChip(x0 + k * (PRESET_CHIP + PRESET_CHIP_GAP), r.y + PRESET_CHIP_Y, PRESET_CHIP, PRESET_CHIP, *sides[k], pressed);
    }
}

// A colour button: a box with a MID border, a tall chip of the colour (left, centred) and its name
// (BODY: "YELLOW" is 47 px, what the 75 px button has right of the chip).
// The lit colour is the inverted bar, chip kept; the Xbox focus is a separate ring around it.
static void drawTile(const Rect& r, int swatch, bool pressed) {
    const Swatch& s = SWATCHES[swatch];
    const bool sel = !bothMixed() && cur().lit && swatch == cur().swatch;
    const uint16_t ink = drawBox(r, pressed, sel, 1, pal(termpal::TEXT));
    Look l;
    l.swatch = swatch;
    const bool onBar = presslook::box(pressed, sel).bar;
    drawChip(r.x + TILE_CHIP_X, r.y + (r.h - TILE_CHIP_H) / 2, TILE_CHIP_W, TILE_CHIP_H, l, onBar);
    char name[12];
    upper(s.name, name, sizeof(name));
    tft.setTextColor(ink);
    text(name, r.x + TILE_TEXT_X, r.y + r.h / 2, Face::BODY, ML_DATUM);
}

// Lights' tabs are Pip-Boy tabs: a TEXT rule under the tab row, the current tab a TEXT bar that
// runs down into the rule, the others MID text (SMALL: four fit beside < BACK, termlayout).
// TAB_RULE_Y is just under the tab row (rects end at y 37) and over STATUS (y 40).
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
    const presslook::Box b = presslook::box(pressed, current);  // as drawBox's, the bar down into the rule
    if (b.bar) {
        tft.fillRect(r.x + 1, r.y + 1, r.w - 2, TAB_RULE_Y - r.y, pal(termpal::TEXT));
        if (b.ring) drawPressRing(r.x, r.y, r.w, TAB_RULE_Y + 1 - r.y);
        ink = pal(termpal::BG);
    }
    tft.setTextColor(ink);
    text(s, r.x + r.w / 2, r.y + r.h / 2, Face::SMALL, MC_DATUM);
}

static void drawButtonBody(int id, bool pressed) {
    const Rect r = deckmenu::rect(screen, id);
    if (r.w <= 0) return;
    if (screen == Screen::Menu) {
        static_assert(N_MENU_ITEMS == deckmenu::N_ITEMS, "a label per Menu item");
        bool dim = false;
        const char* tag = nullptr;
        if (id == M_ITEM0 + deckmenu::KARAOKE) {
            karaokeShownReady = karaokeReady();
            dim = !karaokeShownReady;
            if (karaokeShownReady) tag = LIVE_TAG;
        } else if (id == M_ITEM0 + deckmenu::MUSIC) {
            tag = musicOn ? ON_TAG : OFF_TAG;
        }
        drawRow(r, MENU_ITEMS[id - M_ITEM0], pressed, id == focus, dim, tag);
        return;
    }
    if (screen == Screen::Karaoke || id == L_BACK) {  // K_BACK: its only button
        drawChrome(r, "< BACK", Face::BODY, pressed, false);
        drawTabRule(r.x - 2, r.x + r.w + 2);  // its focus ring crosses the rule too
    } else if (id < L_TAB0 + deckmenu::N_TABS) {
        static_assert(N_TAB_LABELS == deckmenu::N_TABS, "a label per tab");
        drawTab(r, TAB_LABELS[id - L_TAB0], pressed, currentTab() == id - L_TAB0);
    } else if (screen == Screen::Presets) {
        drawPresetSlot(r, deckmenu::presetSlot(screen, id), pressed);
    } else if (id < L_MINUS) {
        drawTile(r, id - L_TILE0, pressed);
    } else {
        drawChrome(r, id == L_MINUS ? "[-]" : "[+]", Face::TITLE, pressed, false);
    }
}

static void drawButton(int id, bool pressed = false) {
    drawButtonBody(id, pressed);
    if (id == focus) drawFocus(id, true);  // on Lights its ring hugs the rect just drawn over
}

// The block bar: 10 cells, one filled per 10 % (the terminal's full block), the rest outlined (its
// shade block; the font's ASCII table has neither glyph), and the % under it (TITLE). "MIX" on BOTH
// while the lights differ (the main screen's lights band has each light's %), "OFF" in DIM when off.
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
    tft.setTextColor(pal(live ? termpal::TEXT : termpal::DIM));
    text(mixed ? "MIX" : l.lit ? pct : "OFF", r.x + r.w / 2, r.y + 34, Face::TITLE, MC_DATUM);
}

// A terminal message (">ROSE APPLIED", ">INSIDE: NO LINK") in place of the Lights page's STATUS
// line, or on the Menu's prompt line (in place of ">" or music mode's line), for MESSAGE_MS since
// messageAt; loop() then puts the usual line back. A newer message replaces it, and so does a screen
// change (show() drops it), so none outlives its screen. Only loop() touches it (and setup(), before
// loop() starts).
static constexpr uint32_t MESSAGE_MS = 2500;
static constexpr uint32_t LAST_TRIP_MS = 6000;  // the boot's ">LAST TRIP ..." is worth longer to read
static char message[32] = "";  // "" when none
static_assert(sizeof(message) <= MARKED_RUN, "a message holding GOJIRA's mark is drawn whole (textMarked)");
static uint32_t messageAt = 0;
static uint32_t messageMs = MESSAGE_MS;

// Sets the message; the caller redraws what shows it (drawMessage, or drawStatus on Lights).
static void setMessage(const char* m, uint32_t ms = MESSAGE_MS) {
    snprintf(message, sizeof(message), "%s", m);
    messageAt = millis();
    messageMs = ms;
}

// Lights: the line under the tabs, just the link state ("LINK 2/2" in TEXT; "OUTSIDE: NO LINK",
// "INSIDE: NO LINK" or "NO LINK" in ALERT), or the message. SMALL, sat on the rect's bottom: STATUS
// has 12 rows (40..51) between the tab rule (39) and the colours, and SMALL's "/" is 12 rows tall
// (BODY's would need 15), so the line fills them with no margin: "/" starts the row under the rule.
// Rows 52-53 can't give it room, as a top-row colour's focus ring is drawn there. The clear (the
// rect) covers all its ink: the line and the messages are capitals, digits and "% / : - >", none
// below the baseline (test_termfont checks).
static void drawStatus() {
    const Rect& r = deckmenu::STATUS;
    char line[24];
    const bool down = lightstatus::link(linkUp(controllers[0]), linkUp(controllers[1]), line, sizeof(line));
    clearBg(r);
    tft.setTextColor(message[0] ? msgInk(message) : pal(down ? termpal::ALERT : termpal::TEXT));
    text(message[0] ? message : line, r.x + STATUS_TEXT_X, r.y + r.h, Face::SMALL, BL_DATUM);
}

// ---- The Menu: a dashboard of stacked bands ----
// Top to bottom (termlayout places them, test_termfont checks they fit): the header line; the clock
// and the date left, the weather right, today's HI / LO under them; the trip (its time big, the
// distance right); both lights on one row; the prompt line (a message, music mode's line, or just
// ">") with its block cursor; the three items at the bottom. A MID rule under each band but the
// prompt line. Each part clears and draws its own rows, so it is redrawn alone when what it shows
// changes: the clock each minute, the weather on a new reading, the trip when its minute or its
// tenth of a km moves, the lights on a look or link change, the prompt line when a message comes or
// goes or music mode's line changes, the cursor's cell on a blink.

static void drawRule(int y) { tft.drawFastHLine(RULE_X0, y, RULE_X1 - RULE_X0, pal(termpal::MID)); }

// The header: one MID line and its rule (the boot's welcome lines typed out before it).
static void drawHeader() {
    clearBg(deckmenu::HEADER);
    tft.setTextColor(pal(termpal::MID));
    text(DASH_TITLE, DASH_X, DASH_TITLE_Y, Face::SMALL, TL_DATUM);
    drawRule(HEADER_RULE_Y);
}

// The clock band's last row (HI / LO): the first row its ink reaches (an accented capital in the
// city, "SÃO PAULO"), where its clear starts and the parts above it stop theirs, so neither takes
// the other.
static int hiLoRowTop() { return HILO_BASE - termfont::accentAscent(Face::SMALL); }

// The clock's minute of the day on screen (-1: not synced, "--:--") and its date, read together
// (netclock::localNow) so they never straddle midnight; updateDayNight keeps them.
static int clockMin = -1;
static int clockY = 0, clockMonth = 0, clockDay = 0;

// The clock band's left: the clock (BIG, TEXT) and under it the date (SMALL, MID: "FRI 23 OCT",
// "--- -- ---" before the clock synced). The date only turns over with a minute, so both go together.
static void drawClock() {
    char s[16];  // room for any int, so -Wformat-truncation stays quiet
    if (clockMin < 0) snprintf(s, sizeof(s), "--:--");
    else snprintf(s, sizeof(s), "%02d:%02d", clockMin / 60, clockMin % 60);
    char date[16];
    dashtext::date(clockMin >= 0, clockY, clockMonth, clockDay, date, sizeof(date));
    const int top = HEADER_RULE_Y + 1;
    clearBg(RULE_X0, top, DASH_SPLIT - RULE_X0, hiLoRowTop() - top);  // down to the HI / LO row
    tft.setTextColor(pal(termpal::TEXT));
    text(s, DASH_X, CLOCK_Y, Face::BIG, TL_DATUM);
    tft.setTextColor(pal(termpal::MID));
    text(date, DASH_X, DATE_Y, Face::SMALL, TL_DATUM);
}

// The weather's four texts as shown: the temperature ("32C", "--C" before the first reading), the sky
// in words ("PARTLY CLOUDY"; nothing before a reading), "HI 32 LO 26  MANAUS" (">OFFLINE", in ALERT,
// while no reading ever came and the last try failed, or there's no WiFi; "HI -- LO --" before the
// first try), and the same without the city ("HI 32 LO 26"), for when the city wouldn't fit its row.
// A failed try keeps the last good reading on screen. wxShown is what is on screen: a new reading
// redraws the part only when one of them changed.
static constexpr int N_WX_TEXT = 4;
static char wxShown[N_WX_TEXT][48];

static bool isOffline(const char* t) { return strcmp(t, ">OFFLINE") == 0; }  // the weather band's one problem

static void weatherTexts(char (&t)[N_WX_TEXT][48]) {
    dashtext::temp(wx.ok, wx.tempC, t[0], sizeof(t[0]));
    snprintf(t[1], sizeof(t[1]), "%s", wx.ok ? weather::words(wx.code) : "");
    if (wx.ok) dashtext::hiLo(wx.daily, wx.maxC, wx.minC, wx.city, t[2], sizeof(t[2]));
    else if (wx.failed || weatherOff) snprintf(t[2], sizeof(t[2]), ">OFFLINE");
    else dashtext::hiLo(false, 0, 0, nullptr, t[2], sizeof(t[2]));
    if (wx.ok) dashtext::hiLo(wx.daily, wx.maxC, wx.minC, nullptr, t[3], sizeof(t[3]));
    else snprintf(t[3], sizeof(t[3]), "%s", t[2]);  // already short
}

// The clock band's right above the HI / LO row, right-aligned: the temperature (BIG, TEXT) and the
// sky (SMALL, MID). Takes the weather's texts into wxShown; the HI / LO row is drawHiLoRow's.
static void drawWeatherTop() {
    weatherTexts(wxShown);
    const int top = HEADER_RULE_Y + 1;
    clearBg(DASH_SPLIT, top, RULE_X1 - DASH_SPLIT, hiLoRowTop() - top);
    tft.setTextColor(pal(termpal::TEXT));
    text(wxShown[0], DASH_RIGHT, CLOCK_Y, Face::BIG, TR_DATUM);
    tft.setTextColor(pal(termpal::MID));
    text(wxShown[1], DASH_RIGHT, WORDS_Y, Face::SMALL, TR_DATUM);
}

// The clock band's last row, cleared whole (from its ink's top to the band's rule, rule to rule
// across): the HI / LO line (SMALL, DIM; ">OFFLINE" in ALERT) right-aligned, without its city when
// that wouldn't fit (termlayout::hiLoFits; alone on its row, even a 23-letter city does).
static void drawHiLoRow() {
    const int top = hiLoRowTop();
    clearBg(RULE_X0, top, RULE_X1 - RULE_X0, CLOCK_RULE_Y - top);
    const char* hiLo = hiLoFits(wxShown[2]) ? wxShown[2] : wxShown[3];
    tft.setTextColor(pal(isOffline(hiLo) ? termpal::ALERT : termpal::DIM));
    text(hiLo, DASH_RIGHT, HILO_BASE, Face::SMALL, BR_DATUM);
}

// The weather after a new reading: the temperature and the sky, then the HI / LO row.
static void drawWeather() {
    drawWeatherTop();
    drawHiLoRow();
}

// The trip meter: time since the first loop() pass (the car started the deck), kilometres once a
// speed feeds it. The trip band shows it; tripTick() redraws the band when its text would change
// (every minute, or every 0.1 km).
static tripmeter::Meter tripMeter;

// The trip band, cleared between its rules and drawn again: TRIP (SMALL, MID), the time (BIG, TEXT)
// after it and the distance right-aligned (BODY, MID: "12.4 KM"; "-- KM" in DIM while no speed source
// has fed the meter), on one baseline. Digits and capitals: no accent rises past BIG's ascent.
static void drawTripBand() {
    const tripmeter::Trip t = tripMeter.trip();
    char hm[16], dist[16];
    tripmeter::elapsed(t, hm, sizeof(hm));
    tripmeter::distance(t, dist, sizeof(dist));
    clearBg(RULE_X0, CLOCK_RULE_Y + 1, RULE_X1 - RULE_X0, TRIP_RULE_Y - CLOCK_RULE_Y - 1);
    tft.setTextColor(pal(termpal::MID));
    text(TRIP_LABEL, DASH_X, TRIP_BASE, Face::SMALL, BL_DATUM);
    if (!t.hasDistance) tft.setTextColor(pal(termpal::DIM));
    text(dist, DASH_RIGHT, TRIP_BASE, Face::BODY, BR_DATUM);
    tft.setTextColor(pal(termpal::TEXT));
    text(hm, tripTimeX(), TRIP_BASE, Face::BIG, BL_DATUM);
}

// The lights row, cleared between its rules and drawn again: INSIDE left of a vertical MID rule,
// OUTSIDE right of it, each its label (IN / OUT, BODY, MID) and what lightstatus::of says of it: on,
// a chip of its colour on the caps and "ROSE 40%" (BODY, TEXT); off, a DIM outlined chip and OFF in
// DIM; no link, "-- NO LINK" in ALERT and no chip (never its last look: the deck can't tell what a
// light it has lost shows).
static_assert(N_CTRL == N_LIGHT_LABELS, "a label per light");
static void drawLightsBand() {
    const int top = TRIP_RULE_Y + 1;
    clearBg(RULE_X0, top, RULE_X1 - RULE_X0, LIGHTS_RULE_Y - top);
    tft.drawFastVLine(LIGHTS_SPLIT, top, LIGHTS_RULE_Y - top, pal(termpal::MID));
    for (int i = 0; i < N_CTRL; i++) {
        const Look l = shownLook(i);  // music mode's while it drives the lights
        tft.setTextColor(pal(termpal::MID));
        text(LIGHT_LABELS[i], lightX(i), LIGHT_ROW_BASE, Face::BODY, BL_DATUM);
        const lightstatus::State st = lightstatus::of(linkUp(controllers[i]), l.lit);
        if (st == lightstatus::State::NoLink) {
            tft.setTextColor(pal(termpal::ALERT));
            text("-- NO LINK", lightTextX(i), LIGHT_ROW_BASE, Face::BODY, BL_DATUM);
            continue;
        }
        drawChip(lightChipX(i), LIGHT_ROW_BASE - LIGHT_CHIP_H, LIGHT_CHIP_W, LIGHT_CHIP_H, l, false);  // on BODY's caps
        if (st == lightstatus::State::Off) {
            tft.setTextColor(pal(termpal::DIM));
            text("OFF", lightTextX(i), LIGHT_ROW_BASE, Face::BODY, BL_DATUM);
            continue;
        }
        char name[12], s[32];
        if (l.swatch == ledlook::LIGHT_BLUE) snprintf(name, sizeof(name), "%s", LIGHT_BLUE_SHORT);  // the full name won't fit
        else upper(SWATCHES[l.swatch].name, name, sizeof(name));
        snprintf(s, sizeof(s), "%s %u%%", name, static_cast<unsigned>(l.pct));
        tft.setTextColor(pal(termpal::TEXT));
        text(s, lightTextX(i), LIGHT_ROW_BASE, Face::BODY, BL_DATUM);
    }
}

// The block cursor: one SMALL cell (as wide as a character, as tall as the caps) at the end of the
// prompt line's text, blinking at ~1 Hz while a message shows, else steady. Only its cell is redrawn.
// -1 when it would pass DASH_RIGHT; the longest message and music line still leave it room
// (test_termfont).
static constexpr uint32_t BLINK_MS = 500;
static int cursorX = -1;
static bool cursorOn = false;

static void drawCursor(bool on) {
    cursorOn = on;
    if (cursorX < 0) return;
    const int cap = termfont::capHeight(Face::SMALL), w = termfont::cell(Face::SMALL);
    if (on) tft.fillRect(cursorX, PROMPT_BASE - cap, w, cap, pal(termpal::TEXT));
    else clearBg(cursorX, PROMPT_BASE - cap, w, cap);
}

// Whether the cursor shows now: blinking while a message shows, else always.
static bool cursorPhase() { return !message[0] || (millis() / BLINK_MS) % 2 == 0; }

// The cursor goes to x on the prompt line (whose clear has taken its old cell).
static void placeCursor(int x) {
    cursorX = x + termfont::cell(Face::SMALL) <= DASH_RIGHT ? x : -1;
    drawCursor(cursorPhase());
}

// Each loop pass on the Menu: the cursor's cell when its phase turned.
static void blinkCursor() {
    if (screen == Screen::Menu && cursorPhase() != cursorOn) drawCursor(!cursorOn);
}

// The prompt line, cleared whole (under the lights' rule, rule to rule across, down to the band's
// end: an accented capital and a descender stay inside) and drawn again, the cursor after its text:
// while a message shows, the message (SMALL, TEXT, ALERT for a problem); in music mode, once a
// palette came, MUSIC_PROMPT, a chip of the song's swatch and its name (">MUSIC [chip] PINK"), before
// that MUSIC_WAITING, or MUSIC_NO_SIGNAL (TEXT too: it's no fault of the deck's); else just ">".
// Redrawn on a message coming or going and on a change of music mode's state or swatch.
static void drawPrompt() {
    const int top = LIGHTS_RULE_Y + 1;
    clearBg(RULE_X0, top, RULE_X1 - RULE_X0, deckmenu::PROMPT_BAND.y + deckmenu::PROMPT_BAND.h - top);
    if (message[0]) {
        tft.setTextColor(msgInk(message));
        placeCursor(DASH_X + text(message, DASH_X, PROMPT_BASE, Face::SMALL, BL_DATUM));
        return;
    }
    tft.setTextColor(pal(termpal::TEXT));
    if (!musicOn) {
        placeCursor(DASH_X + text(PROMPT_IDLE, DASH_X, PROMPT_BASE, Face::SMALL, BL_DATUM));
        return;
    }
    if (musicState != musiclights::State::Colour || musicSwatch < 0) {
        const char* line = musicState == musiclights::State::NoSignal ? MUSIC_NO_SIGNAL : MUSIC_WAITING;
        placeCursor(DASH_X + text(line, DASH_X, PROMPT_BASE, Face::SMALL, BL_DATUM));
        return;
    }
    text(MUSIC_PROMPT, DASH_X, PROMPT_BASE, Face::SMALL, BL_DATUM);
    tft.fillRect(songChipX(), PROMPT_BASE - MUSIC_CHIP, MUSIC_CHIP, MUSIC_CHIP, tint(SWATCHES[musicSwatch].screen));
    char name[12];
    upper(SWATCHES[musicSwatch].name, name, sizeof(name));
    placeCursor(songNameX() + text(name, songNameX(), PROMPT_BASE, Face::SMALL, BL_DATUM));
}

// The whole dashboard but its three items (drawAll draws those as buttons).
static void drawDash() {
    drawHeader();
    drawClock();
    drawWeatherTop();
    drawHiLoRow();
    drawRule(CLOCK_RULE_Y);
    drawTripBand();
    drawRule(TRIP_RULE_Y);
    drawLightsBand();
    drawRule(LIGHTS_RULE_Y);
    drawPrompt();
}

// The terminal's boot: on a blank screen the welcome lines (termlayout::BOOT_LINES, SMALL, the last in MID)
// and the build's FW_LABEL (in DIM: "FW 1.1.0+226.g41b2b7a") type out one character every TYPE_MS
// behind a block cursor (a SMALL cell; ~110 characters, ~0.7 s), stay BOOT_HOLD_MS to be read, then
// drawAll() puts up the dashboard. It runs in setup(), so nothing else draws meanwhile: every other
// drawing path runs from loop(), and the link and voice tasks start after it (a voice job or a link
// change that comes later waits in the queue / the link flags for loop() as always).
static constexpr uint32_t TYPE_MS = 6, BOOT_HOLD_MS = 400;
static void typeLine(const char* s, int row, termpal::Role ink) {
    const int w = termfont::cell(Face::SMALL), h = termfont::capHeight(Face::SMALL);
    tft.setTextColor(pal(ink));
    const int y = BOOT_Y0 + row * BOOT_DY;  // the caps' top row
    int x = BOOT_X;
    for (const char* p = s; *p; p++) {
        const char ch[2] = {*p, 0};
        clearBg(x, y, w, h);  // the cursor
        x += text(ch, x, y, Face::SMALL, TL_DATUM);
        tft.fillRect(x, y, w, h, pal(termpal::TEXT));
        delay(TYPE_MS);
    }
    clearBg(x, y, w, h);
}

static void typeBoot() {
    clearBg(0, 0, tft.width(), tft.height());
    for (int i = 0; i < N_BOOT; i++) typeLine(BOOT_LINES[i], i, i == N_BOOT - 1 ? termpal::MID : termpal::TEXT);
    typeLine(FW_LABEL, N_BOOT, termpal::DIM);
    delay(BOOT_HOLD_MS);
}

static void drawGrid() {
    for (int id = L_TILE0; id < L_MINUS; id++) drawButton(id);
}

static void drawTabs() {
    for (int id = L_TAB0; id < L_TAB0 + deckmenu::N_TABS; id++) drawButton(id);
}

// What shows the lights' links on the current screen: Lights' STATUS line (the message too), the
// Menu's lights band.
static void drawLightState() {
    if (onLights()) drawStatus();
    else if (screen == Screen::Menu) drawLightsBand();
}

// What shows the message, or once it's gone the usual line: Lights' STATUS line, the Menu's prompt
// line (then music mode's line or ">" again).
static void drawMessage() {
    if (onLights()) drawStatus();
    else if (screen == Screen::Menu) drawPrompt();
}

// After a look changed (a tap, voice, the watch, Y): everything that shows it (Lights' STATUS line
// shows only the links, and the PRESETS tab no look). True when a new theme repainted the whole
// screen (then nothing more is drawn: the repaint showed it all, the parts would only flicker).
static bool drawLooks() {
    if (updateTheme()) return true;  // a look that changes the theme repaints all in the new one
    if (screen == Screen::Lights) {
        drawGrid();
        drawLevel();
    } else if (screen == Screen::Menu) {
        drawLightsBand();
    }
    return false;
}

// Lights and Karaoke (the Menu's focus is the inverted bar, drawn by the button itself): a TEXT ring in
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

// ---- Karaoke screen: the black deck's song, its synced lyrics big ----
// The top row: < BACK (the same boxed button as Lights') and right of it the song as
// "TITLE - ARTIST" (BODY, MID, in capitals like the rest of the chrome). The body
// (deckmenu::KARAOKE_BODY): the lyric line due now big (BIG, TEXT, centred, word-wrapped over up
// to 3 lines, ".." on the 3rd; a line too long for that in TITLE over up to 4), the next line under
// it (BODY, DIM, up to 2 lines), and a thin progress line at the foot (TEXT while playing). The
// lyrics keep their case (they read better than caps). The texts are VT323's Latin-1 (other accents
// and punctuation folded, txt::latin1Fold) and Japanese from the Unifont subset in flash
// (data/jp16.bin, ui/jpfont): kana, CJK punctuation, half/fullwidth forms and the JIS level 1 kanji,
// 32 px in BIG, 16 px elsewhere, centred on the caps, in the text's colour; a BIG block holding them
// goes KARAOKE_WIDE_DY apart. Any other CJK or Hangul character is an outlined box the size of a
// glyph; emoji are dropped (ui/lyricrun). Without a song to sing along the big text is the state
// word in DIM (>NO SYNCED LYRICS, >NOTHING PLAYING, >LYRICS: NO SIGNAL; >WRONG KEY, a problem, in
// ALERT) and the screen stays. The line switches at its start time: karaoke interpolates the
// position between polls and lyricsTick looks every KARAOKE_TICK_MS. Each part is redrawn only when
// what it shows changed.
static constexpr uint32_t KARAOKE_TICK_MS = 50;
// The areas' sizes and pitches are termlayout's (test_termfont checks each face's lines fit them).
static constexpr int KARAOKE_X = 4, KARAOKE_CX = 160;
static constexpr int KARAOKE_SONG_X = 80, KARAOKE_SONG_W = 236;  // right of < BACK (x 4..74), on the top row
static constexpr int KARAOKE_BAR_Y = 224, KARAOKE_BAR_H = 3;

// The Japanese subset font (tools/gen_jp_font.py), read from flash as it draws. Empty (every CJK
// character a box) if the blob is invalid.
EMBED(data_jp16_bin);
static const jpfont::Font& jpFont() {
    static jpfont::Font jp;
    static bool opened = false;
    if (!opened) {
        opened = true;
        if (!jp.open(EMBED_DATA(data_jp16_bin), EMBED_SIZE(data_jp16_bin)))
            Serial.printf("[jp] font blob invalid (%u bytes)\n", static_cast<unsigned>(EMBED_SIZE(data_jp16_bin)));
    }
    return jp;
}
static int jpWidth(uint32_t cp, const void*) { return jpFont().width(cp); }

// A Wide item's glyph (or a Box: an outline over the cell's rows and columns 2..13) with its top left
// at (x, top), each font pixel s x s, in ink. Each row's runs of ink go as one rect.
static void drawWide(const lyricrun::Item& it, int x, int top, int s, uint16_t ink) {
    if (it.kind == lyricrun::Kind::Box) {
        for (int k = 0; k < s; k++) tft.drawRect(x + 2 * s + k, top + 2 * s + k, 12 * s - 2 * k, 12 * s - 2 * k, ink);
        return;
    }
    int w = 0;
    const uint8_t* g = jpFont().glyph(it.cp, &w);
    if (!g) return;
    for (int r = 0; r < lyricrun::GLYPH_H; r++) {
        const uint32_t bits = w == 16 ? (static_cast<uint32_t>(g[2 * r]) << 8 | g[2 * r + 1]) : g[r];
        auto on = [&](int c) { return ((bits >> (w - 1 - c)) & 1u) != 0; };
        for (int c = 0; c < w;) {
            if (!on(c)) {
                c++;
                continue;
            }
            int e = c;
            while (e < w && on(e)) e++;
            tft.fillRect(x + c * s, top + r * s, (e - c) * s, s, ink);
            c = e;
        }
    }
}

// The decoded texts (ui/lyricrun), wrapped into karaokeLines (only loop draws). Room for the song
// row, the longest text.
static constexpr size_t KARAOKE_ITEMS = sizeof(stateparse::Deck::track) + sizeof(stateparse::Deck::artist) + 4;
static lyricrun::Item karaokeItems[KARAOKE_ITEMS];
static lyricrun::Span karaokeLines[4];
static size_t karaokeDecode(const char* s, bool upper = false) {
    return lyricrun::decode(s, karaokeItems, KARAOKE_ITEMS, jpWidth, nullptr, upper);
}

// One wrapped line of karaokeItems at (x, y) by datum, as text() places text, in the text colour set.
// Latin-1 only: text() as before. With Japanese: the whole placed by lyricrun's width, the Latin-1
// runs drawn by text() on the baseline, the glyphs centred on the caps (lyricrun::glyphTop).
static void drawSpan(const lyricrun::Span& sp, int x, int y, Face f, uint8_t datum) {
    static char run[KARAOKE_ITEMS + 3];
    size_t n = 0;
    if (!lyricrun::hasWide(karaokeItems, sp)) {
        for (size_t i = sp.start; i < sp.end; i++) run[n++] = static_cast<char>(karaokeItems[i].cp);
        if (sp.dots) {
            run[n++] = '.';
            run[n++] = '.';
        }
        run[n] = 0;
        text(run, x, y, f, datum);
        return;
    }
    const int w = lyricrun::width(f, karaokeItems + sp.start, sp.end - sp.start, sp.dots), h = datum % 3;
    int pen = h == 0 ? x : h == 1 ? x - w / 2 : x - w;
    const int cap = termfont::capHeight(f), v = datum / 3;
    const int base = v == 0 ? y + cap : v == 1 ? y + cap / 2 : y;
    const int top = lyricrun::glyphTop(f, base), s = lyricrun::scale(f);
    const uint16_t ink = static_cast<uint16_t>(tft.textcolor);
    for (size_t i = sp.start;; i++) {
        const bool end = i == sp.end;
        if (!end && karaokeItems[i].kind == lyricrun::Kind::Latin1) {
            run[n++] = static_cast<char>(karaokeItems[i].cp);
            continue;
        }
        if (end && sp.dots) {
            run[n++] = '.';
            run[n++] = '.';
        }
        if (n > 0) {
            run[n] = 0;
            pen += text(run, pen, base, f, BL_DATUM);
            n = 0;
        }
        if (end) break;
        drawWide(karaokeItems[i], pen, top, s, ink);
        pen += lyricrun::advance(f, karaokeItems[i]);
    }
}

// karaokeLines[0..n) centred, the first line's tallest glyph at row y, a line every dy px (each
// face's lines fit their pitch whole: no line's descender reaches the next's capitals).
static void drawLines(int n, Face f, int y, int dy, uint16_t ink) {
    tft.setTextColor(ink);
    for (int i = 0; i < n; i++) drawSpan(karaokeLines[i], KARAOKE_CX, y + i * dy + termfont::ascent(f), f, BC_DATUM);
}

// How far an accented capital (À, Ã, Å) rises above the row drawLines puts a face's tallest ASCII
// glyph at: a lyric area's clear starts that much higher.
static int accentRise(Face f) { return termfont::accentAscent(f) - termfont::ascent(f); }

// The big text: BIG over up to 3 lines (KARAOKE_WIDE_DY apart when one holds Japanese), or TITLE
// over up to 4 when BIG would cut it; the block centred in its area. Each is measured in the face it
// is drawn in.
static void drawBig(const char* s, uint16_t ink) {
    const size_t items = karaokeDecode(s);
    bool cut = false;
    Face f = Face::BIG;
    int n = lyricrun::wrap(karaokeItems, items, f, KARAOKE_W, 3, karaokeLines, &cut);
    int dy = KARAOKE_BIG_DY;
    if (cut) {
        f = Face::TITLE;
        n = lyricrun::wrap(karaokeItems, items, f, KARAOKE_W, 4, karaokeLines);
        dy = KARAOKE_FIT_DY;
    } else {
        for (int i = 0; i < n; i++)
            if (lyricrun::hasWide(karaokeItems, karaokeLines[i])) dy = KARAOKE_WIDE_DY;
    }
    drawLines(n, f, karaokeBlockY(n, dy), dy, ink);
}

// The next line: BODY over up to 2 lines, DIM.
static void drawNext(const char* s) {
    const size_t items = karaokeDecode(s);
    const int n = lyricrun::wrap(karaokeItems, items, Face::BODY, KARAOKE_W, 2, karaokeLines);
    drawLines(n, Face::BODY, KARAOKE_NEXT_Y, KARAOKE_NEXT_DY, pal(termpal::DIM));
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
        static_assert(sizeof(songText) <= KARAOKE_ITEMS, "the song row's items fit");
        const size_t items = karaokeDecode(songText, true);
        const int n = lyricrun::wrap(karaokeItems, items, Face::BODY, KARAOKE_SONG_W, 1, karaokeLines);
        clearBg(KARAOKE_SONG_X, 2, KARAOKE_SONG_W, 36);
        tft.setTextColor(pal(termpal::MID));
        if (n > 0) drawSpan(karaokeLines[0], KARAOKE_SONG_X, 20, Face::BODY, ML_DATUM);
    }

    const uint32_t pos = hasTrack ? static_cast<uint32_t>(karaoke::progressMs(d, now - song.at)) : 0;
    const char* stateWord = karaoke::state(heard, song.wrongKey, d.playing, d);
    const bool isWord = stateWord[0] != 0;
    const char* big = isWord ? stateWord : karaoke::lyric(d, pos);
    const bool bigChanged = all || isWord != karaokeShownWord || strcmp(big, karaokeShownBig) != 0;
    if (bigChanged) {
        snprintf(karaokeShownBig, sizeof(karaokeShownBig), "%s", big);
        karaokeShownWord = isWord;
        // from as high as any block's accents or Japanese glyphs reach to as low (test_termfont checks)
        clearBg(KARAOKE_X, KARAOKE_BIG_CLEAR_Y, KARAOKE_W, KARAOKE_BIG_CLEAR_END - KARAOKE_BIG_CLEAR_Y);
        drawBig(big, pal(karaoke::isAlert(big) ? termpal::ALERT : isWord ? termpal::DIM : termpal::TEXT));
    }
    const char* next = isWord ? "" : karaoke::nextLine(d, pos);
    if (all || strcmp(next, karaokeShownNext) != 0) {
        snprintf(karaokeShownNext, sizeof(karaokeShownNext), "%s", next);
        const int rise = accentRise(Face::BODY);
        clearBg(KARAOKE_X, KARAOKE_NEXT_Y - rise, KARAOKE_W, KARAOKE_NEXT_H + rise);
        drawNext(next);
    }
    // The line switched between polls (the position is interpolated) and the one after it isn't known
    // until the next poll: ask for that one now. The area stays blank meanwhile, not a stale line.
    if (bigChanged && !all && !isWord && next[0] == 0) lyricsNudge = true;

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

// Redraws the current screen in the current palette.
static void drawAll() {
    clearBg(0, 0, tft.width(), tft.height());
    if (screen != Screen::Menu) drawTabRule(0, tft.width());
    for (int id = 0; id < deckmenu::count(screen); id++) {
        if (built(id)) drawButton(id);
    }
    if (screen == Screen::Menu) {
        drawDash();
    } else if (screen == Screen::Karaoke) {
        drawKaraokeBody(true);
    } else {
        if (screen == Screen::Lights) drawLevel();
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
        watchdog::feed();  // BOOT's calibration runs on the watched loop and waits on the pen
        delay(5);
    }
    for (;;) {
        while (!touch::readRaw(&x, &y)) {
            watchdog::feed();
            delay(5);
        }
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
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        text("Touch calibration", 160, 82, Face::TITLE, MC_DATUM);
        text("Tap each cross with the pen", 160, 160, Face::BODY, MC_DATUM);
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
            text("Calibrated", 160, 120, Face::TITLE, MC_DATUM);
            delay(800);
            return;
        }
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        text("Missed a cross - again", 160, 120, Face::TITLE, MC_DATUM);
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

// NVS keys per look: sw0/pct0/lit0 (BOTH), ...1 (INSIDE), ...2 (OUTSIDE), and the page ("target":
// the tab, 0..2 the look pages as Target, 3 PRESETS).
// save() only marks the state dirty; flushSave() (from loop) writes what changed, SAVE_AFTER_MS
// after the last change: a burst of +/- taps is one write, not 10 commits per tap. A change in
// the last SAVE_AFTER_MS before the power goes is lost.
static constexpr uint32_t SAVE_AFTER_MS = 1500;
static Look savedLooks[3];
static int savedTab = BOTH;
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
    if (currentTab() != savedTab) {
        savedTab = currentTab();
        prefs.putUChar("target", static_cast<uint8_t>(savedTab));
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
    // builds before the PRESETS tab stored 0..2, which load as they were
    const int tab = prefs.getUChar("target", BOTH) % deckmenu::N_TABS;
    presetsTab = tab == deckmenu::TAB_PRESETS;
    target = presetsTab ? BOTH : static_cast<Target>(tab);
    savedTab = tab;
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
// Meter isn't thread-safe). Nothing calls it yet and no speed is faked, so the trip band shows "-- KM".
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

// Each loop pass: once a second the seconds, and the trip band if the trip's text changed; once a minute
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
        if (screen == Screen::Menu) drawTripBand();
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

// Preset slot0 (0..5) onto the lights: each light gets the preset's look. The BOTH page copies them
// when they match; otherwise it takes the inside look, lit if either light is (bothlook::seed),
// shows "MIX", and the main screen's lights band tells each light's look. From a tap, A, voice or
// the watch.
static void applyPreset(int slot0) {
    musicYield();  // both lights get the preset anyway
    const ledpresets::Preset& p = ledpresets::PRESETS[slot0];
    looks[INSIDE] = p.in;
    looks[OUTSIDE] = p.out;
    looks[BOTH] = bothlook::seed(p.in, p.out);
    for (int i = 0; i < N_CTRL; i++) push(i);
    save();
    char m[sizeof(message)];
    lightstatus::applied(ledpresets::shown(p), m, sizeof(m));  // ">ゴジラ APPLIED" on screen
    setMessage(m);
    if (!drawLooks()) drawMessage();  // Lights' STATUS line, the Menu's prompt line (a repaint drew it)
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
// A screen or page change arms it: the finger that made it, a double tap's second touch or a bounce
// then reads as released until the panel has been up for tapguard::QUIET_MS, so none lands on the
// new screen's button under it ([LIGHTS] sits where Lights' [-] and PRESETS' PURPURA are).
static tapguard::Guard tapGuard;

// The button a controller press is showing pressed, and since when (see padPress).
static constexpr uint32_t PAD_FLASH_MS = 150;
static int padFlash = -1;
static uint32_t padFlashAt = 0;

// Whether button `id` is under a finger or a controller press right now (drawn pressed).
static bool pressedNow(int id) { return (tap.down && tap.id == id) || padFlash == id; }

// Switches to screen `s`: the focus goes to its first button (none without a controller), a press
// still drawn pressed is forgotten (its id means another button there), touch waits for the finger
// to lift (tapGuard), and a message goes (it was about the screen left; the new screen shows its
// usual line).
// Back from Karaoke, the focus lands on the KARAOKE item it came from.
static void show(Screen s) {
    const Screen from = screen;
    screen = s;
    tap.id = -1;
    padFlash = -1;
    tapGuard.arm();
    message[0] = 0;
    if (s == Screen::Karaoke) karaokeAt = millis();
    focus = padUp ? homeFocus() : -1;
    if (focus >= 0 && s == Screen::Menu && from == Screen::Karaoke) focus = M_ITEM0 + deckmenu::KARAOKE;
    drawAll();
}

// The Lights screen on the page it showed last: a look page, or PRESETS.
static Screen lightsScreen() { return presetsTab ? Screen::Presets : Screen::Lights; }

// Tab `tab` of the Lights screen. Between the look pages only what shows the look is redrawn; to or
// from PRESETS the screen changes (Lights <-> Presets) and is redrawn whole, its message kept, and
// a focus on < BACK or a tab stays (their ids are the same on both), else it goes to the new one's
// first button. Either way touch waits for the finger to lift (tapGuard).
static void showTab(int tab) {
    tapGuard.arm();
    presetsTab = tab == deckmenu::TAB_PRESETS;
    if (!presetsTab) target = static_cast<Target>(tab);
    const Screen s = lightsScreen();
    if (s == screen) {
        drawTabs();
        if (s == Screen::Lights) {
            drawGrid();
            drawLevel();
        }
        return;
    }
    screen = s;
    tap.id = -1;  // its id means another button there
    padFlash = -1;
    if (focus >= L_TAB0 + deckmenu::N_TABS) focus = homeFocus();
    drawAll();
}

// One settled tap on a Lights button (either screen: a look page or PRESETS).
static void pressLights(int id) {
    Look& l = cur();
    if (id == L_BACK) {
        show(Screen::Menu);
        return;
    }
    if (id < L_TAB0 + deckmenu::N_TABS) {  // another page
        showTab(id - L_TAB0);
        save();
        return;
    }
    if (deckmenu::isPreset(screen, id)) {
        applyPreset(deckmenu::presetSlot(screen, id));
        return;  // applyPreset saved and drew; the release redraws the button
    }
    if (screen != Screen::Lights) return;
    if (id < L_MINUS) {
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
    }
    save();
}

static void setMusic(bool on, bool restore = true);

// One settled tap on a button of the current screen.
static void press(int id) {
    if (onLights()) {
        pressLights(id);
        return;
    }
    if (screen == Screen::Karaoke) {
        if (id == K_BACK) show(Screen::Menu);
        return;
    }
    if (id < M_ITEM0 || id >= M_ITEM0 + deckmenu::N_ITEMS) return;
    switch (static_cast<deckmenu::Item>(id - M_ITEM0)) {
    case deckmenu::LIGHTS: show(lightsScreen()); break;
    case deckmenu::KARAOKE:
        // dimmed: no song to sing along, so only a message (on the prompt line) says why
        if (karaokeReady()) {
            show(Screen::Karaoke);
        } else {
            setMessage(karaoke::whyNot(lyricsHeard(), song.wrongKey, song.deck.playing, song.deck));
            drawMessage();
        }
        break;
    case deckmenu::MUSIC: setMusic(!musicOn); break;  // the caller redraws the item (its ON tag)
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
//   D-pad: move the focus (held: repeats; the sticks are ignored; on the Menu left / right along
//          its three items, no wrap)    A: press the focused button
//   B: back to the Menu (from Lights or Karaoke; on the Menu: nothing)
//   Lights only: X next page (BOTH, INSIDE, OUTSIDE, PRESETS, round)
//   the look pages only: Y this page's lights on/off    LB / RB: - / +  (on PRESETS: nothing)

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

// A button press on the controller: the press looks (drawn pressed) for PAD_FLASH_MS. One at a time: a
// second press inside that window first restores the earlier button (else -/+ stayed pressed).
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
    if (!onLights()) return;  // the rest is the Lights screen's
    if (x) padPress(L_TAB0 + (currentTab() + 1) % deckmenu::N_TABS);
    if (screen != Screen::Lights) return;  // PRESETS (X may just have shown it): no look for Y, LB or RB
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
// Each try is a task of its own (core 1, idle priority like the voice server, which it never
// blocks) that ends when the try does, so its 6 KB stack isn't held for the 15 min between tries:
// the voice task starts one once WiFi is up, then every 15 min (weatherTick). A try GETs the place
// (once per boot, again while it fails), then the weather, both plain http (no TLS on a heap shared
// with NimBLE, WiFi and the server), 5 s timeouts, the body into a fixed 2 KB buffer. Each try is
// published under weatherMux; a failed try keeps the last good reading and comes again in 2 min.
// loop() polls weatherFresh to redraw. The place is the phone's public IP's: approximate (the
// operator's city, sometimes a neighbour's), which is fine for the weather.
static constexpr uint32_t WEATHER_EVERY_MS = 15 * 60 * 1000, WEATHER_RETRY_MS = 2 * 60 * 1000;
static volatile bool weatherBusy = false;      // a try's task is running
static volatile uint32_t weatherDoneAt = 0;    // when the last try ended (millis)
static volatile uint32_t weatherWait = 0;      // and how long until the next
static volatile unsigned weatherStackLeft = 0; // the last try's least stack to spare, for [mem]
static portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
static WeatherReading weatherPub;           // guarded by weatherMux
static volatile bool weatherFresh = false;  // a try was published; loop() clears it
static char weatherBody[2048];              // only the weather task touches it

// GET `url` into weatherBody (cut at 2 KB, always terminated); the HTTP status, negative without an
// answer (net/httpget). 5 s timeouts, 8 s for the whole body.
static int weatherGet(const char* url) {
    return net::httpGet(url, 5000, weatherBody, sizeof(weatherBody), 8000);
}

static void publishWeather(const WeatherReading& r) {
    portENTER_CRITICAL(&weatherMux);
    weatherPub = r;
    portEXIT_CRITICAL(&weatherMux);
    weatherFresh = true;
}

// Kept from try to try (only the one running touches them): the place, and the last good reading.
static geoip::Place weatherPlace{};
static bool weatherPlaced = false;
static WeatherReading weatherLast;  // with the last try's outcome

static void weatherTry(void*) {
    watchdog::watch();  // two bounded GETs (httpGet's timeouts and deadline), fed before each
    geoip::Place& place = weatherPlace;
    bool& placed = weatherPlaced;
    WeatherReading& r = weatherLast;
    bool ok = false;
    if (!net::isOnline()) {
        Serial.println("[weather] offline");
    } else {
        if (!placed) {
            watchdog::feed();
            const int rc = weatherGet(geoip::URL);
            placed = rc == 200 && geoip::parse(weatherBody, &place);
            if (placed) Serial.printf("[weather] place %s (%s, %s)\n", place.city, place.lat, place.lon);
            else Serial.printf("[weather] place failed (http %d)\n", rc);
        }
        if (placed) {
            char url[256];
            weather::url(place.lat, place.lon, url, sizeof(url));
            watchdog::feed();
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
    weatherWait = ok ? WEATHER_EVERY_MS : WEATHER_RETRY_MS;
    weatherDoneAt = millis();
    weatherStackLeft = static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr));
    watchdog::forget();  // before the task goes: a deleted task can't feed
    weatherBusy = false;  // last: the voice task may start the next try from here on
    vTaskDelete(nullptr);
}

// From the voice task, once WiFi is up and then every pass: starts a try when the last one's wait
// is over (at once the first time), not while an update has the WiFi. 6 KB: the GETs and the
// logging; the [mem] line shows what the last try left.
static void weatherTick() {
    static bool tried = false;
    if (weatherBusy || ota::active()) return;
    if (tried && millis() - weatherDoneAt < weatherWait) return;
    tried = true;
    weatherBusy = true;
    if (xTaskCreatePinnedToCore(weatherTry, "weather", 6144, nullptr, 0, nullptr, 1) != pdPASS) {
        weatherBusy = false;
        weatherWait = WEATHER_RETRY_MS;
        weatherDoneAt = millis();
        Serial.println("[weather] no memory for a try, retry in 2 min");
    }
}

// In loop(): takes a published try and, on the Menu, redraws the weather when one of its texts
// changed (a try every 15 min mostly brings the same).
static void takeWeather() {
    if (!weatherFresh) return;
    weatherFresh = false;  // before the copy: a try published meanwhile raises it again
    portENTER_CRITICAL(&weatherMux);
    wx = weatherPub;
    portEXIT_CRITICAL(&weatherMux);
    if (screen != Screen::Menu) return;
    char t[N_WX_TEXT][48];
    weatherTexts(t);
    for (int i = 0; i < N_WX_TEXT; i++) {
        if (strcmp(t[i], wxShown[i]) != 0) {
            drawWeather();
            return;
        }
    }
}

// ---- Lyrics: what the black deck (the S3) is playing, from its GET /state ----
// Its own task (core 1, idle priority like the weather; core 0 is the lights' and the controller's
// link), started with the weather once WiFi is up. It polls only while a screen that shows it is up
// (loop() keeps lyricsWant): the KARAOKE screen every second, every 5 s after ten misses in a row
// (util/pollpace); the Menu every MENU_POLL_MS, every 30 s after those ten misses (enough to light or
// dim its KARAOKE item); at once when either opens. Otherwise it sleeps. On KARAOKE loop() also
// nudges an extra poll (lyricsNudge, at most one per LYRICS_NUDGE_MS) when the line shown changes
// and the next one isn't known, so the dim line under it needn't wait for the next second's poll.
// The black deck is at its fixed hotspot address (BLACK_DECK_ADDR) on the iPhone hotspot, else
// looked up by its mDNS name (DECK_MDNS_NAME) and kept through blips (util/deckaddr). HTTP/1.0,
// 1.5 s timeouts, the body into a fixed statejson::MAX_JSON buffer, util/stateparse. Each poll is
// published under lyricsMux; loop() polls lyricsFresh to redraw. The log tells when the deck is
// found, lost and back, not every poll.
// The same task keeps music mode alive on the black deck (musicKeepAlive, below), whatever screen
// shows: it owns lyricsHost.
static TaskHandle_t lyricsHandle = nullptr;
static portMUX_TYPE lyricsMux = portMUX_INITIALIZER_UNLOCKED;
static LyricsReading lyricsPub;             // guarded by lyricsMux
static volatile bool lyricsFresh = false;  // a poll was published; loop() clears it
enum LyricsWant : uint8_t { WANT_NONE, WANT_MENU, WANT_KARAOKE };
static volatile uint8_t lyricsWant = WANT_NONE;  // what the screen shown needs; loop() keeps it
static pollpace::Pace lyricsPace;                 // only the lyrics task touches it (and lyricsHost)
static char lyricsBody[statejson::MAX_JSON];  // only the lyrics task touches these three
static deckaddr::Keep lyricsAddr;
static bool lyricsMdns = false;

// Where the black deck is: its fixed address on the hotspot (until it stops answering as the deck),
// else its mDNS name on whatever network both are on. False when it can't be found.
static bool lyricsHost(char* out, size_t n) {
    const bool hot =
        hotspotip::onHotspot(hostOrder(WiFi.localIP()), hostOrder(WiFi.gatewayIP()), hostOrder(WiFi.subnetMask()));
    if (hot && lyricsAddr.addr() == 0 && lyricsAddr.tryFixed()) {
        snprintf(out, n, "%s", BLACK_DECK_ADDR);
        return true;
    }
    if (!lyricsMdns) lyricsMdns = MDNS.begin(LEDS_MDNS_NAME);
    if (lyricsAddr.addr() == 0) {
        // each failed lookup holds the task for up to 1.5 s: while the deck stays away, not every poll
        const uint32_t now = millis();
        if (!lyricsPace.mayLookup(now)) return false;
        lyricsPace.lookedUp(now);
        const IPAddress a = MDNS.queryHost(DECK_MDNS_NAME, 1500);
        if (static_cast<uint32_t>(a) == 0) {
            lyricsAddr.lookupFailed();  // on the hotspot: the fixed address again
            return false;
        }
        lyricsAddr.found(static_cast<uint32_t>(a));
        Serial.printf("[lyrics] deck found at %s (mDNS%s)\n", a.toString().c_str(), hot ? ", off its fixed address" : "");
    }
    snprintf(out, n, "%s", IPAddress(lyricsAddr.addr()).toString().c_str());
    return true;
}

// What deckGet returns when it sent nothing: the clock isn't set, so nothing can be signed (the
// bearer key is never sent). Not a miss: the deck wasn't asked.
static constexpr int NOT_SIGNED = -1000;

// A signed GET of `path` with `params` (short fixed words, sent as they are) to the black deck, the
// body into `body` when there is one; the HTTP status, negative without an answer, or NOT_SIGNED.
// peer: the address asked. Short timeouts: the deck is one hop away, and a missing deck must not hold
// the polls up.
static int deckGet(const char* path, const sigreq::Param* params, int count, char* body, size_t bodyLen,
                   uint32_t* peer = nullptr) {
    if (!net::isOnline()) return -1;
    char auth[sigreq::AUTH_LEN];
    if (!reqauth::signGet(path, params, count, auth, sizeof(auth))) {
        static bool told = false;
        if (!told) Serial.println("[lyrics] waiting for the clock: requests to the deck are signed");
        told = true;
        return NOT_SIGNED;
    }
    char host[40], query[32] = "", url[160];
    if (!lyricsHost(host, sizeof(host))) return -1;
    for (int i = 0; i < count; ++i) {
        const size_t at = strlen(query);
        snprintf(query + at, sizeof(query) - at, "%s=%s&", params[i].name, params[i].value);
    }
    if (snprintf(url, sizeof(url), "http://%s%s?%s%s", host, path, query, auth) >= static_cast<int>(sizeof(url)))
        return -1;
    IPAddress a;
    if (peer && a.fromString(host)) *peer = static_cast<uint32_t>(a);
    return net::httpGet(url, 1500, body, bodyLen, body ? 3000 : 0);
}

// GET /state into lyricsBody (cut at MAX_JSON, always terminated).
static int lyricsGet() {
    lyricsBody[0] = 0;
    return deckGet("/state", nullptr, 0, lyricsBody, sizeof(lyricsBody));
}

// ---- Music mode: the black deck sends the song's palette ----
// From the lyrics task: while musicWant, GET /music?on=1 at once and every MUSIC_KEEP_MS (the black
// deck stops after 30 s without one), MUSIC_RETRY_MS after a miss (no answer, "stopping, ask again"),
// but MUSIC_KEEP_MS after a 503 (its low memory: Spotify's TLS needs that heap, so no hurry); on=0 once
// when it goes. Each ask also records the black deck's address (musicPeer), the one sender the music
// task takes packets from, and counts toward lyricsAddr's misses like a lyrics poll (a stale mDNS
// address is looked up again). The log tells each change of answer.
static constexpr uint32_t MUSIC_KEEP_MS = 10000, MUSIC_RETRY_MS = 2000;
static volatile uint32_t musicPeer = 0;  // IPv4, network order (IPAddress's dword); 0: not asked yet

static int musicAsk(bool on) {
    const sigreq::Param p[] = {{"on", on ? "1" : "0"}};
    uint32_t peer = 0;
    const int rc = deckGet("/music", p, 1, nullptr, 0, &peer);  // the status is the answer
    if (peer) musicPeer = peer;
    return rc;
}

static int musicGet(bool on) {
    const int rc = musicAsk(on);
    // the deck's own answers (200, "stopping" 409, low memory 503); 3 misses in a row: look it up again
    if (rc != NOT_SIGNED) lyricsAddr.onPoll(rc == 200 || rc == 409 || rc == 503);
    return rc;
}

static void musicKeepAlive() {
    static bool asked = false;  // an on=1 went out since the last on=0
    static uint32_t nextAt = 0;
    static int lastRc = 0;
    if (ota::active()) return;
    if (!musicWant) {
        if (asked) {
            asked = false;
            lastRc = 0;
            Serial.printf("[music] off (rc=%d)\n", musicGet(false));
        }
        return;
    }
    if (asked && static_cast<int32_t>(millis() - nextAt) < 0) return;
    const int rc = musicGet(true);
    if (rc != lastRc) Serial.printf("[music] on=1: rc=%d\n", rc);
    lastRc = rc;
    asked = true;
    nextAt = millis() + (rc == 200 || rc == 503 ? MUSIC_KEEP_MS : MUSIC_RETRY_MS);
}

static void lyricsTask(void*) {
    static stateparse::Deck deck;  // static: ~650 B, off the task stack
    pollpace::Pace& pace = lyricsPace;
    uint32_t lastPoll = 0;
    uint8_t was = WANT_NONE;  // what was wanted at the last poll
    watchdog::watch();  // each pass: a few GETs of 1.5 s and an mDNS query of 1.5 s at most
    for (;;) {
        watchdog::feed();
        musicKeepAlive();
        const uint8_t want = lyricsWant;
        if (want == WANT_NONE || ota::active()) {  // nothing shows it, or an update has the WiFi
            was = WANT_NONE;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const uint32_t now = millis();
        const uint32_t every = want == WANT_MENU ? pace.menuIntervalMs() : pace.intervalMs();
        // a nudge only helps while the deck answers (a missing one is polled at its own pace)
        const bool nudged = lyricsNudge && want == WANT_KARAOKE && pace.misses() == 0 && now - lastPoll >= LYRICS_NUDGE_MS;
        if (want != was || now - lastPoll >= every || nudged) {
            // KARAOKE just opened: poll at once, at the fast pace, and look the name up again at once.
            // (The Menu keeps the misses so far, or its 30 s back-off would restart on every visit.)
            if (want == WANT_KARAOKE && was != WANT_KARAOKE) pace = pollpace::Pace();
            was = want;
            lastPoll = now;
            lyricsNudge = false;
            const int rc = lyricsGet();
            if (rc == NOT_SIGNED) {  // no clock yet: nothing was asked, so nothing is lost either
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            const bool ok = rc == 200 && stateparse::parse(lyricsBody, deck);
            if (pace.misses() == 0 && !ok) Serial.printf("[lyrics] deck lost (rc=%d)\n", rc);
            if (pace.misses() > 0 && ok) Serial.println("[lyrics] deck back");
            pace.onResult(ok);
            // the deck's own answers (a /state that parses, low memory 503); 3 misses in a row: look it up again
            lyricsAddr.onPoll(ok || rc == 503);
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

// From the voice task, once WiFi is up. 6 KB: the GETs and the mDNS query; the [mem]
// line shows what's left.
static void beginLyrics() {
    if (lyricsHandle) return;
    xTaskCreatePinnedToCore(lyricsTask, "lyrics", 6144, nullptr, 0, &lyricsHandle, 1);
}

// In loop(): tells the task how to poll, takes a published poll, and redraws what changed: on the
// Menu the KARAOKE item when it turns lit or dimmed (a state also goes stale with time, so this looks
// each pass), on KARAOKE the parts of the screen that changed (the line and the bar move between
// polls, so this looks every KARAOKE_TICK_MS).
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

// ---- Music mode: the song's colour on both lights ----
// Its own task (core 1, idle priority, 4 KB), started the first time music mode goes on. While
// musicWant and online it holds a UDP socket on musicwire::PORT (raw lwip, 200 ms receive timeout),
// closed again when music mode goes off or an update has the WiFi. It takes packets only from the
// black deck's address (musicPeer), signed with REMOTE_TOKEN and with a counter above the last one
// (util/wire/musicwire), and publishes the latest PalPkt and its time under musicMux.
// musicTick() (loop) reads them at most MUSIC_TICK_MS apart: the palette's main colour snapped to a
// swatch (musiclights::nearestSwatch) and each light's level (musiclights::level: the owner's under
// the palette's cap, a 40 % floor) are the lights' music looks; a new one fades in (musicFadeTick).
// The prompt line is redrawn when its state or swatch changed.
struct MusicHeard {
    bool palOk = false;  // a PalPkt came since the socket opened
    uint32_t at = 0;     // millis() it came
    musicwire::PalPkt pal{};
};
static TaskHandle_t musicHandle = nullptr;
static portMUX_TYPE musicMux = portMUX_INITIALIZER_UNLOCKED;
static MusicHeard musicPub;  // guarded by musicMux
static constexpr uint32_t MUSIC_TICK_MS = 250;

static int musicOpen() {
    const int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(musicwire::PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    timeval tv{0, 200000};
    lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (lwip_bind(sock, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) < 0) {
        lwip_close(sock);
        return -1;
    }
    return sock;
}

static void musicTask(void*) {
    int sock = -1;
    uint8_t buf[musicwire::PAL_LEN + 8];  // room past a packet: a longer datagram is refused, not cut to fit
    musicwire::Fresh fresh;               // the black deck's counter must rise (a packet played again is dropped)
    uint32_t freshPeer = 0;
    watchdog::watch();  // each pass waits 200 ms at most (the socket's receive timeout)
    for (;;) {
        watchdog::feed();
        if (!musicWant || !net::isOnline() || ota::active()) {
            if (sock >= 0) {
                lwip_close(sock);
                sock = -1;
                Serial.println("[music] socket closed");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (sock < 0) {
            sock = musicOpen();
            if (sock < 0) {
                Serial.println("[music] no UDP socket");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            Serial.printf("[music] listening on udp %u\n", static_cast<unsigned>(musicwire::PORT));
            portENTER_CRITICAL(&musicMux);
            musicPub = MusicHeard();
            portEXIT_CRITICAL(&musicMux);
        }
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        const int n = lwip_recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) continue;  // the timeout: look at musicWant again
        const uint32_t peer = musicPeer;
        if (peer == 0 || from.sin_addr.s_addr != peer) continue;  // not the black deck's address
        if (peer != freshPeer) {  // another black deck (or address): its counter starts over
            fresh.reset();
            freshPeer = peer;
        }
        musicwire::PalPkt p;
        musicwire::Seq seq;
        // signed with REMOTE_TOKEN (a spoofed source address isn't enough), and not one heard before
        if (!reqauth::hmacOk() || !musicwire::decode(buf, static_cast<size_t>(n), REMOTE_TOKEN, &p, &seq)) continue;
        if (!fresh.accept(seq)) continue;
        const uint32_t now = millis();
        portENTER_CRITICAL(&musicMux);
        musicPub.palOk = true;
        musicPub.at = now;
        musicPub.pal = p;
        portEXIT_CRITICAL(&musicMux);
    }
}

// A new music look fades in over ~3 s: each light from the colour frame it showed to the new look's,
// one colour write (both header variants) per step: the inside light MUSIC_FADE_IN steps 500 ms
// apart (its controller takes ~7 writes a second), the outside one MUSIC_FADE_OUT steps 1 s apart
// (its takes ~1). The first step goes as soon as that gap since the light's last step allows (a new
// song right after a change still keeps the rate). A light switched on for it fades up from black,
// its power-on sent with the first step; an outside light left off gets nothing. Loop-owned.
struct MusicFade {
    songpalette::Rgb from{}, to{};
    int steps = 0, done = 0;  // done == steps: no fade
    uint32_t every = 0, at = 0;  // at: millis() of the last step written
};
static constexpr int MUSIC_FADE_IN = 6, MUSIC_FADE_OUT = 3;
static constexpr uint32_t MUSIC_FADE_MS = 3000;
static MusicFade musicFade[N_CTRL];

// The colour frame push() sends for `l` (black when off).
static songpalette::Rgb frameOf(const Look& l) {
    if (!l.lit) return {0, 0, 0};
    const Rgb s = ledRgb(l.swatch);
    return {elkcmd::dim(s.r, l.pct), elkcmd::dim(s.g, l.pct), elkcmd::dim(s.b, l.pct)};
}

// The colour frame light i shows now: a fade's current step, else its look's.
static songpalette::Rgb frameNow(int i) {
    const MusicFade& f = musicFade[i];
    if (f.done < f.steps) return musiclights::blend(f.from, f.to, f.done, f.steps);
    return frameOf(shownLook(i));
}

static void writeFrame(int i, const songpalette::Rgb& c) {
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(controllers[i], elkcmd::color(v, c.r, c.g, c.b));
}

// Each loop pass: the fades' due steps. A light out of reach skips its writes (it gets its look
// when it comes back: loop()'s fresh push).
static void musicFadeTick() {
    const uint32_t now = millis();
    for (int i = 0; i < N_CTRL; i++) {
        MusicFade& f = musicFade[i];
        if (f.done >= f.steps || now - f.at < f.every) continue;
        f.done++;
        f.at = now;
        if (linkUp(controllers[i])) writeFrame(i, musiclights::blend(f.from, f.to, f.done, f.steps));
    }
}

// The song's swatch at `cap` onto the lights: each light's music look (the inside one always on,
// the outside one as the owner left it), fading in where it changed. False when nothing changed.
static bool musicTarget(int swatch, int cap) {
    bool any = false;
    for (int i = 0; i < N_CTRL; i++) {
        const Look& own = looks[i + 1];
        Look t;
        t.swatch = swatch;
        t.lit = i == 0 || own.lit;
        t.pct = static_cast<uint8_t>(musiclights::level(own.pct, cap));
        if (musicDriving && t == musicLook[i]) continue;
        any = true;
        const Look was = shownLook(i);
        const songpalette::Rgb from = frameNow(i);
        musicLook[i] = t;
        MusicFade& f = musicFade[i];
        const uint32_t lastStep = f.at;
        f = MusicFade();
        f.at = lastStep;
        if (!t.lit) continue;  // the outside light left off: stays off, nothing sent
        f.from = was.lit ? from : songpalette::Rgb{0, 0, 0};
        f.to = frameOf(t);
        f.steps = i == 0 ? MUSIC_FADE_IN : MUSIC_FADE_OUT;
        f.every = MUSIC_FADE_MS / static_cast<uint32_t>(f.steps);
        if (!was.lit && linkUp(controllers[i])) {  // switched on with its first step, never its old colour
            for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(controllers[i], elkcmd::power(v, true));
            f.done = 1;
            f.at = millis();
            writeFrame(i, musiclights::blend(f.from, f.to, f.done, f.steps));
        }
    }
    musicDriving = true;
    return any;
}

// Music mode on or off (the MUSIC item, or musicYield): the tasks follow musicWant (the music task is
// started the first time), and ">MUSIC ON" / ">MUSIC OFF" shows like any message; the prompt line's
// music state (or the idle ">") comes back after it. Off, the lights go back to their pages' looks
// (restore false: the caller pushes them itself). The caller redraws the item.
static void setMusic(bool on, bool restore) {
    musicOn = on;
    musicWant = on;
    if (on) {
        if (!musicHandle && xTaskCreatePinnedToCore(musicTask, "music", 4096, nullptr, 0, &musicHandle, 1) != pdPASS) {
            musicHandle = nullptr;  // the next tap tries again
            Serial.println("[music] no memory for its task");
        }
        musicOnAt = millis();
        musicState = musiclights::State::Waiting;
        musicSwatch = -1;
    } else {
        const bool drove = musicDriving;
        musicDriving = false;
        musicSwatch = -1;
        for (MusicFade& f : musicFade) f = MusicFade();
        if (drove && restore) {
            for (int i = 0; i < N_CTRL; i++) push(i);
            drawLooks();  // the lights band and the theme
        }
    }
    setMessage(on ? ">MUSIC ON" : ">MUSIC OFF");
    drawMessage();
}

// A manual change (a tap, the Xbox controller, voice, the watch) while music mode is on: the mode
// ends first (the MUSIC item shows OFF), then the change applies to the pages' looks. True when the
// lights showed the song: the caller pushes both lights (the one it doesn't change goes back too).
static bool musicYield() {
    if (!musicOn) return false;
    const bool drove = musicDriving;
    Serial.println("[music] a manual change ends it");
    setMusic(false, false);
    if (screen == Screen::Menu) {
        const int item = M_ITEM0 + deckmenu::MUSIC;
        drawButton(item, pressedNow(item));
    }
    return drove;
}

// In loop(), while music mode is on, at most every MUSIC_TICK_MS: the latest palette onto the lights
// (musicTarget: nothing while it is the same) and the prompt line's state; the line is redrawn only
// when its state or swatch changed (and not while a message holds it: it brings the line back as it
// goes).
static void musicTick() {
    static uint32_t tickAt = 0;
    if (!musicOn || millis() - tickAt < MUSIC_TICK_MS) return;
    tickAt = millis();
    MusicHeard h;
    portENTER_CRITICAL(&musicMux);
    h = musicPub;
    portEXIT_CRITICAL(&musicMux);
    const uint32_t now = millis();  // after the copy: never before the packet's time
    const int was = musicSwatch;
    const musiclights::State wasState = musicState;
    musicState = musiclights::state(h.palOk, h.palOk ? h.at : musicOnAt, now);
    if (h.palOk) {
        const int sw = musiclights::nearestSwatch(h.pal.p.c[0]);
        musicSwatch = sw;  // before drawLooks: a new theme repaints the prompt line too
        if (musicTarget(sw, h.pal.p.cap)) {
            Serial.printf("[music] song %u: %s, inside %u%%, outside %u%%%s\n", static_cast<unsigned>(h.pal.songId),
                          SWATCHES[sw].word, static_cast<unsigned>(musicLook[0].pct),
                          static_cast<unsigned>(musicLook[1].pct), musicLook[1].lit ? "" : " (off)");
            if (drawLooks()) return;  // the lights band and the theme; a new theme's repaint drew all
        }
    }
    if (musicState == wasState && musicSwatch == was) return;
    if (screen == Screen::Menu && !message[0]) drawPrompt();
}

// ---- Update over WiFi (net/ota): its page holds the screen ----
// While an update arrives (ArduinoOTA on its own task, core 0) a full-screen page in the current
// theme (util/otapage, laid out by termlayout's UPDATE_*) covers whatever showed: the dashboard's
// header, then ">UPDATING" over a 10-cell block bar (one cell per 10 %, as the Lights level) and
// the %; ">REBOOTING" as it ends well (net/ota restarts the deck otaview::DONE_MS later); or
// ">UPDATE FAILED" in ALERT for otaview::FAILED_HOLD_MS (the running firmware stays), then the
// screen it covered comes back. Meanwhile loop() draws and acts on nothing else: touch and the
// controller do nothing, a voice job waits (/say answers busy); the lights keep their looks, and
// the link, lyrics and weather tasks pause themselves on ota::active().

// The bar and the % under it (everything from the bar down to the screen's bottom).
static void drawUpdateBar(int pct) {
    clearBg(0, UPDATE_BAR_Y, tft.width(), tft.height() - UPDATE_BAR_Y);
    const int x0 = (tft.width() - UPDATE_BAR_W) / 2, full = otapage::cells(pct);
    for (int k = 0; k < otapage::CELLS; k++) {
        const int x = x0 + k * (UPDATE_CELL + UPDATE_GAP);
        if (k < full) tft.fillRect(x, UPDATE_BAR_Y, UPDATE_CELL, UPDATE_BAR_H, pal(termpal::TEXT));
        else tft.drawRect(x, UPDATE_BAR_Y, UPDATE_CELL, UPDATE_BAR_H, pal(termpal::MID));
    }
    char line[8];
    snprintf(line, sizeof(line), "%d%%", pct < 0 ? 0 : pct > 100 ? 100 : pct);
    tft.setTextColor(pal(termpal::TEXT));
    text(line, tft.width() / 2, UPDATE_PCT_Y, Face::TITLE, TC_DATUM);
}

static void drawUpdatePage(otaview::Phase p, int pct) {
    clearBg(0, 0, tft.width(), tft.height());
    drawHeader();
    tft.setTextColor(pal(otapage::alert(p) ? termpal::ALERT : termpal::TEXT));
    text(otapage::title(p), tft.width() / 2, UPDATE_TITLE_Y, Face::BIG, TC_DATUM);
    drawUpdateBar(pct);
}

// Each loop pass: draws what the update's phase asks for; true while the page holds the screen.
static bool updateTick() {
    static otapage::Shown shown;
    const ota::View v = ota::view();
    switch (otapage::step(shown, v.phase, v.pct)) {
        case otapage::Draw::Page: drawUpdatePage(v.phase, v.pct); break;
        case otapage::Draw::Bar: drawUpdateBar(v.pct); break;
        case otapage::Draw::Back:  // the screen it covered, in full, in the palette of the moment
            Serial.println("[ota] back to the deck");
            // A press from before (or during) the page is forgotten, so it can't act on this screen.
            tap.down = false;
            tap.id = -1;
            drawAll();
            break;
        case otapage::Draw::None: break;
    }
    return otapage::holds(v.phase);
}

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"). Without a zone it acts like the BOTH page (Siri: spoken zone words are
// ignored); &zone=inside|outside (the Garmin app) acts on that zone's page only.
// GET /state?k=[&zone=] answers that zone's line (level included) without changing anything.
// A preset ("rose", "preset 2") is applied to both lights and answered with its line; the rest of
// that phrase (a colour, a level, the zone) is ignored. GET /presets?k= lists the six presets (one
// ledpresets::wireLine each), GET /status?k= each light's state.
// Each of the four takes the bearer ?k= (Siri Shortcuts can't sign) or a signature (common/net/reqauth:
// the Garmin app signs, so it never sends the key).
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
    if (musicDriving) {  // what the lights show: the watch syncs its screen from it
        p.looks[INSIDE] = shownLook(0);
        p.looks[OUTSIDE] = shownLook(1);
        p.looks[BOTH] = bothlook::seed(p.looks[INSIDE], p.looks[OUTSIDE]);
    }
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

// A voice reply's note for a change to `zone` that reached a light the deck has no link to ("Purple,
// 70 percent, outside not connected"): from the link state loop() last published, what /status says
// (answerVoice publishes the change itself).
static void noteUnlinked(char* line, size_t n, Target zone) {
    const Published p = snapshot();
    const bool inMissed = zone != OUTSIDE && !p.up[0];  // up[]: the controllers, 0 inside, 1 outside
    const bool outMissed = zone != INSIDE && !p.up[1];
    lightstatus::notConnected(inMissed, outMissed, line, n);
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
// screen stays. Then answers the waiting handler, saying so when the change was for a light the
// deck has no link to.
static void applyVoice(const VoiceJob& job) {
    const ledvoice::Command& c = job.c;
    char line[sizeof(voiceReply)];
    if (c.preset >= 1 && c.preset <= ledpresets::N) {  // "rose", "preset 2": both lights, whatever zone was asked
        applyPreset(c.preset - 1);
        ledpresets::describe(c.preset, line, sizeof(line));
        noteUnlinked(line, sizeof(line), BOTH);
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
    drawLooks();  // on either screen (a new theme repaints it); the page on Lights stays
    describe(line, sizeof(line), looks[job.zone], job.zone, c.level >= 0 || c.levelStep != 0);
    noteUnlinked(line, sizeof(line), job.zone);
    answerVoice(line, job.seq);
}

static void handleSay() {
    if (!reqauth::pass(*server, gate, "/say", "Wrong key")) return;
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
    if (!reqauth::pass(*server, gate, "/state", "Wrong key")) return;
    const String zone = server->arg("zone");
    const Target z = zone == "inside" ? INSIDE : zone == "outside" ? OUTSIDE : BOTH;
    const Published p = snapshot();
    char line[64];
    describe(line, sizeof(line), p.looks[z], z);
    server->send(200, "text/plain", line);
}

// The six presets, one wireLine each ("1 ROSE pink 40 pink 100"), joined by \n. A fixed table: no
// snapshot needed.
static void handlePresets() {
    if (!reqauth::pass(*server, gate, "/presets", "Wrong key")) return;
    char body[ledpresets::N * 48] = "";
    size_t len = 0;
    for (int i = 0; i < ledpresets::N && len < sizeof(body); i++) {
        char l[48];  // "3 GOJIRA light blue 40 light blue 100" is 37
        ledpresets::wireLine(i + 1, l, sizeof(l));
        const int k = snprintf(body + len, sizeof(body) - len, "%s%s", i ? "\n" : "", l);
        if (k > 0) len += static_cast<size_t>(k);
    }
    server->send(200, "text/plain", body);
}

// Each light on, off or out of reach: "inside=on outside=nolink".
static void handleStatus() {
    if (!reqauth::pass(*server, gate, "/status", "Wrong key")) return;
    const Published p = snapshot();
    char line[96];
    lightstatus::reply(lightstatus::of(p.up[0], p.looks[INSIDE].lit), lightstatus::of(p.up[1], p.looks[OUTSIDE].lit),
                       FW_VERSION, line, sizeof(line));
    server->send(200, "text/plain", line);
}

static void voiceTask(void*) {
    // Watched: WebServer's waits are 5 s per request, a WiFi join feeds before each try.
    watchdog::watch();
    net::connectAny();
    while (!net::isOnline()) {
        watchdog::feed();
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    server->begin();
    netclock::begin();  // SNTP, for night mode and the Menu's clock
    weatherTick();  // the first try
    beginLyrics();
    ota::begin(LEDS_MDNS_NAME);  // updates over WiFi (off without OTA_PASSWORD; WiFi itself needs REMOTE_TOKEN)
    Serial.printf("[voice] on: http://%s/say?text=purple&k=<REMOTE_TOKEN>  (heap %u)\n",
                  net::deviceIp().c_str(), static_cast<unsigned>(ESP.getFreeHeap()));
    // The deck stays on the network it joined (the decks' rule since 2026-10-14; docs/BACKLOG.md):
    // in the car only the hotspot exists. At home, a deck that booted while the iPhone hid its
    // hotspot sits on home WiFi, out of the iPhone's reach: open the hotspot screen and restart it.
    // (An every-20 s hunt for the hotspot was tried here on 2026-10-22 and removed: the hidden
    // hotspot was rarely seen, and the scans cost the server and the radio.)
    for (;;) {
        watchdog::feed();
        server->handleClient();
        net::loop();  // rejoins after a drop
        weatherTick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void beginVoice() {
    if (!gate.enabled()) {
        Serial.println("[voice] off (REMOTE_TOKEN empty)");
        return;
    }
    reqauth::begin();  // HMAC's self-test: the watch's requests and ours to the black deck are signed
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
    int m = 0, y = 0, month = 0, day = 0;
    const bool synced = netclock::localNow(&m, &y, &month, &day);
    const int minute = synced ? m : -1;
    bool want = synced && daynight::isNight(m);
#ifdef NIGHT_TEST
    want = true;
#endif
    if (minute != clockMin || day != clockDay) {
        if (minute >= 0 && clockMin < 0) Serial.printf("[clock] synced, local %02d:%02d\n", m / 60, m % 60);
        clockMin = minute;
        clockY = y;
        clockMonth = month;
        clockDay = day;
        if (want == night && screen == Screen::Menu) drawClock();
    }
    if (want == night) return;
    night = want;
    backlight::set(night);  // before the repaint
    Serial.printf("[ui] night mode %s\n", night ? "on" : "off");
    drawAll();
}

// The theme the INSIDE light asks for: its colour while it's on and linked (insideSeen), else -1.
static int wantTheme() {
#ifdef THEME_TEST
    return THEME_TEST;
#else
    const Look l = shownLook(0);  // in music mode the song's swatch
    return lightstatus::of(insideSeen, l.lit) == lightstatus::State::On ? ledlook::gridSwatch(l.swatch) : -1;  // light blue: blue's
#endif
}

// Called before anything draws a changed look or link (drawLooks, a preset, voice, the link edge),
// and once more at the end of each loop as a backstop: a new theme repaints the current screen (the
// backlight stays), with a button still under a finger or a controller press drawn pressed again.
// The early return keeps the repaint (which may come back here) from recursing. True when it
// repainted.
static bool updateTheme() {
    const int want = wantTheme();
    if (want == theme) return false;
    theme = want;
    Serial.printf("[ui] theme %d\n", theme);
    drawAll();
    if (tap.down && tap.id >= 0) drawButton(tap.id, true);
    if (padFlash >= 0) drawButton(padFlash, true);
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.printf("[boot] ledpanel v%s\n", FW_VERSION);
    ota::checkBoot();  // may boot the other app slot (net/ota), before anything else starts
    // The task watchdog at 90 s (common/core/watchdog), before any task subscribes: the link,
    // voice, lyrics, music and weather tasks, and this loop from the end of setup().
    watchdog::begin();
    tft.init();
    backlight::begin();  // PWM, at the day level
    tft.invertDisplay(true);  // CYD panel: colours are inverted without this
    tft.setRotation(1);
    // The texts are Latin-1 bytes (txt::latin1Fold) and the fonts cover 0x20-0xFF: each byte is its
    // glyph. TFT_eSPI would read them as UTF-8 (its default) and lose the accents and the degree sign.
    tft.setAttribute(UTF8_SWITCH, 0);
    vividGamma();
    touch::begin();

    prefs.begin("leds", false);
    load();

    pinMode(PIN_BOOT, INPUT_PULLUP);
    if (!loadCalibration() || digitalRead(PIN_BOOT) == LOW) calibrate();
    typeBoot();  // before the tasks start: nothing else draws while it types
    showLastTrip();  // after the typing, so its few seconds start when the Menu shows
    weatherOff = !gate.enabled();  // no REMOTE_TOKEN, no WiFi: the first draw already says >OFFLINE
    theme = wantTheme();  // after the typing (always green); no link yet, so -1 but for THEME_TEST
    drawAll();
    // NimBLE 2 (the Xbox library needs it): init, bonding on for the controller, +9 dBm. Its bonds
    // live in NVS "nimble_bond"; an older NimBLE 1.4 build boot-loops on them (blescan/bletest).
    xbox.begin();
    // 8 KB: the Xbox library's connect, discovery and subscribe (std::function, std::string) run on
    // this task too; the [mem] line shows what's left.
    xTaskCreatePinnedToCore(linkTask, "leds-link", 8192, nullptr, 1, &linkHandle, 0);
    publish();
    beginVoice();
    enableLoopWDT();  // the core feeds it before each loop(); BOOT's calibration feeds while it waits
}

void loop() {
    // Up bootguard::HEALTHY_MS, online at that moment and the update listener started (when updates
    // are on; off, online is enough; ota::onGoodPoll checks the time and the listener): this
    // firmware has proven itself (net/ota), and can take the next update. Checked once a second.
    static uint32_t provenAt = 0;
    if (millis() - provenAt >= 1000) {
        provenAt = millis();
        if (net::isOnline()) ota::onGoodPoll();
    }
    // An update over WiFi: its page holds the screen, and nothing below draws or acts meanwhile.
    if (updateTick()) {
        flushSave();  // a change made just before the upload is kept through its reboot
        delay(10);
        return;
    }

    int x, y;
    // The panel reports a phantom press at power-up (raw x=0): ignore the first second of the loop
    // (not of the boot: typeBoot() eats ~1 s of that before loop() ever runs).
    static uint32_t loopStart = 0;
    if (loopStart == 0) loopStart = millis() | 1;
    const bool down = tapGuard.update(touch::read(&x, &y) && millis() - loopStart > 1000, millis());
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
        drawMessage();
    }

    VoiceJob voice;
    if (voiceQueue && xQueueReceive(voiceQueue, &voice, 0) == pdTRUE) applyVoice(voice);
    flushSave();

    // every minute: free heap and the least stack each task has had to spare
    static uint32_t memAt = 0;
    if (millis() - memAt > 60000) {
        memAt = millis();
        Serial.printf("[mem] heap %u (min %u)  stack left: link %u, voice %u, weather %u, lyrics %u, music %u, ota %u, loop %u\n",
                      static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                      linkHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(linkHandle)) : 0u,
                      voiceHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(voiceHandle)) : 0u,
                      static_cast<unsigned>(weatherStackLeft),  // the last try's (each ends with it)
                      lyricsHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(lyricsHandle)) : 0u,
                      musicHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(musicHandle)) : 0u,
                      ota::stackLeft(),
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    handlePad();
    takeWeather();
    lyricsTick();
    musicTick();
    musicFadeTick();
    updateDayNight();
    tripTick();
    blinkCursor();

    // BOOT held for 2 s: calibrate the touch again.
    static uint32_t bootSince = 0;
    if (digitalRead(PIN_BOOT) == LOW) {
        if (bootSince == 0) bootSince = millis();
        else if (millis() - bootSince > 2000) {
            calibrate();
            tapGuard.arm();  // the last cross's tap must not land on the screen drawn next
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
            musicFade[i].done = musicFade[i].steps;  // it has the target: no step back to a mid-fade colour
        }
        if (up != shown[i]) {
            shown[i] = up;
            if (i == 0) {
                insideSeen = up;
                updateTheme();  // before drawLightState: the screen is in the right theme first
            }
            char m[sizeof(message)];
            // both lights going the same way together (the phone hotspot, the car's power): one
            // message, or the second would replace the first
            const int o = 1 - i;
            edgeAt[i] = millis() | 1;
            edgeUp[i] = up;
            if (edgeAt[o] && millis() - edgeAt[o] < 150 && edgeUp[o] == up) lightstatus::linkChangeBoth(up, m, sizeof(m));
            else lightstatus::linkChange(i == 0, up, m, sizeof(m));
            // the boot's ">LAST TRIP" has its 6 s: a link change doesn't cut it short (the lights still redraw)
            if (!(message[0] && messageMs > MESSAGE_MS && millis() - messageAt < messageMs)) setMessage(m);
            drawLightState();  // STATUS on Lights (the message too), the lights band on the Menu: NO LINK while it's gone
            if (screen == Screen::Menu) drawMessage();
        }
    }
    updateTheme();  // a backstop: every path above has already called it
    publish();  // for /state and /status
    delay(10);
}
