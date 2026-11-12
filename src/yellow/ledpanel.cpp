// The yellow deck (CYD) as the car's control screen: a Seegson-style terminal (pale green on black,
// red only for problems, vivid at night, faint scanlines, all caps) that drives the car's two
// ELK-family LED controllers. Two screens, laid out by util/deckmenu:
//   Menu (boot, after the CYBERDECK welcome types out): a dashboard of stacked bands, for what the
//           car's panel and CarPlay don't show: the trip and the lights. The header line
//           (HONDA CITY TOURING, the PS3 controller's battery at the right); the clock big with the
//           date under it ("FRI 23 OCT"), and right of it the trip's time big ("0:16", the deck's
//           own) over TRIP; both lights on one row, "IN [chip] ROSE 40%" | "OUT [chip] YELLOW 100%"
//           (OFF, or -- NO LINK when the deck has no link to it); the prompt line: a message, music
//           mode's line, or just ">", with a block cursor (blinking while a message shows); and at
//           the bottom the three items "[LIGHTS]" (opens Lights), "KARAOKE" and "MUSIC MODE", the
//           last two on/off switches (a tap flips it, an ON or OFF tag under it; see "Music mode"
//           below). KARAOKE has the black deck (the S3) show the song's lyrics full screen, swept
//           line by line; the prompt line says ">KARAOKE ON" / ">KARAOKE OFF". The deck draws no
//           lyrics itself and no longer reads the black deck's /state
//   Lights: < BACK, the pages BOTH / INSIDE / OUTSIDE / PRESETS as Pip-Boy tabs (BOTH sets both
//           lights together; INSIDE and OUTSIDE each keep their own look), the status line ("LINK
//           2/2", or "OUTSIDE: NO LINK" in ALERT), then on the three look pages 2x4 colour buttons
//           (tap one to set it and turn on, double-tap the lit one to turn the lights off) and
//           [-]  [level: a 10-cell block bar and the %, OFF, or on BOTH while the two lights'
//           levels differ each one's, "IN 60%  OUT 40%"]  [+]; on PRESETS the six presets, two rows of three, each its name over its
//           inside and outside chips (tap: apply, ">ROSE APPLIED"; the tab stays). GOJIRA's name is
//           ゴジラ on screen (">ゴジラ APPLIED"): three Unifont glyphs (ui/gojiraname), as VT323 has
//           no kana; "GOJIRA" on the wire, in the logs and the voice replies. Its colour, cyan,
//           is a preset's only: not on the grid, never a song colour; the theme takes blue's for it
// A press acts once it settles (its first ~40 ms averaged), and a tap in a gap snaps to the nearest
// button; it shows as the bar with a ring cut in it (ui/presslook). After a screen or page
// change touch waits for the finger to lift and 250 ms (util/tapguard). The iPhone's Bluetooth
// must be off (Settings): each controller takes one connection.
// The STATUS line on Lights and the prompt line on the Menu show a terminal message for 2.5 s
// (">ROSE APPLIED", ">INSIDE: NO LINK"), then their usual line again (on the Menu: ">" or music mode's line).
// Each page's colour, intensity and power and the page last shown survive a reboot (NVS); the deck
// still boots to the Menu. Karaoke and music mode are not kept: both boot off. Touch calibration (resistive panel) runs on first boot,
// or with BOOT held (at power-up, or 2 s any time). Voice and the Garmin watch: GET /say, /state,
// /presets and /status over WiFi (see "Voice" below), on either screen. Voice and the watch also say
// "karaoke" / "karaoke off" (reply "Karaoke on/off"). A PS3 controller (Bluetooth
// Classic) drives it too: the D-pad moves the focus (an inverted bar on the Menu's three items, a TEXT
// ring on Lights; none is drawn while no controller is connected), cross presses (on KARAOKE or
// MUSIC MODE it flips the switch), circle
// goes back to the Menu; on Lights square is the next page, and on the look pages triangle on/off,
// L1 / R1 - / +. On any screen START, L2 / R2, SELECT, L1 / R1 and PS are shortcuts for both lights,
// answered by a buzz, and the pad's LEDs show it is connected and the links (see "PS3 controller input" below).
// Theme: while the INSIDE look is on, the terminal (Menu, Lights; not the boot typing or the
// calibration) takes its colour, termpal's three levels of it (problems show in white on the red,
// orange and pink themes), linked or not: a colour tapped with the lights out of reach still shows.
// The look off: the green again. The colour tiles and chips stay real.
// Colours and night: the terminal is in termpal's night palette day and night, each theme's own hue
// at full saturation (the day's pale tints looked dull, and went grey on the dimmed panel). Night,
// 18:00-06:00 local time once WiFi has synced the clock, only drops the backlight to
// NIGHT_BACKLIGHT. The chips stay real. Without REMOTE_TOKEN there is no WiFi, no clock, and it
// stays day.
// Music mode ("song colours"): the MUSIC item has the black deck (the S3) send the song's palette over
// UDP (util/musicwire); both lights then take its main colour snapped to one of the grid's 8 swatches
// (util/musiclights), steady, fading to the next song's over ~3 s, the inside at the owner's level under
// the palette's time-of-day cap (never under 40 %), the outside at 100 %. An outside light left off stays off; an inside one
// off is switched on. The theme follows the swatch. The prompt line shows ">MUSIC", a chip and the
// swatch's name (">MUSIC: WAITING" before the first palette), or ">MUSIC: NO SIGNAL" when no palette
// came for 15 s. Voice and the watch turn it on and off too ("music", "music off"). Any manual
// change (touch, the PS3 controller, voice, the watch) ends the mode, then applies; turning it off
// brings back the looks from before. Not kept over a reboot (see
// "Music mode" below).
// Test hooks: building with -D NIGHT_TEST (e.g. PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST") is night
// whatever the time, to check it without waiting for 18:00, with a serial tuner to try night colours
// and the backlight live (see tuneLine); -D THEME_TEST=<swatch 0..7>
// (ledlook's order: red, orange, pink, purple, blue, green, white, yellow) forces that theme whatever
// the inside light does, to see it without the car's lights. Neither is set in platformio.ini (every
// test hook: docs/ARCHITECTURE.md "Test hooks").
// Updates over USB only, `pio run -e ledpanel -t upload` (the WiFi updates went to fit the PS3
// controller's Bluedroid in the heap; docs/BACKLOG.md). Three crash resets in a row before new
// firmware has proven itself still boot the previous one (net/ota's boot guard, util/bootguard).
// The deck does lights, karaoke (the switch only), music mode, the PS3 controller and the watch's
// routes: the car's health, the speed cameras and the weather went for the same heap (the OBD deck
// shows the car; the black deck speaks the cameras). Its "deck" task only sends the music and
// karaoke keep-alives to the black deck.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <Ps3Controller.h>  // esp32-ps3: the DualShock 3 over Bluedroid's Bluetooth Classic
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <esp_coexist.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include "pins.h"
#include <irrx.h>
#include "../common/ui/backlight.h"
#include "../common/core/embed.h"
#include "../common/core/crashlog.h"
#include "../common/core/watchdog.h"
#include "elklink.h"
#include "padhid.h"
#include "ui/gojiraname.h"
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
#include "../util/yellow/keepalive.h"
#include "../util/yellow/deckmenu.h"
#include "../util/yellow/elkcmd.h"
#include "../util/yellow/irkeys.h"
#include "../util/common/hotspotip.h"
#include "../util/lights/ledintro.h"
#include "../util/lights/ledlook.h"
#include "../util/yellow/ledpresets.h"
#include "../util/yellow/ledvoice.h"
#include "../util/lights/musiclights.h"
#include "../util/wire/musicwire.h"
#include "../util/yellow/lightstatus.h"
#include "../util/yellow/padbattery.h"
#include "../util/yellow/padfeel.h"
#include "../util/yellow/padflick.h"
#include "../util/yellow/padguard.h"
#include "../util/common/remotegate.h"
#include "../util/yellow/tapguard.h"
#include "../util/common/termpal.h"
#include "../util/common/version.h"
#include "../util/yellow/touchcal.h"
#include "../util/lights/tripclock.h"
#include "../util/yellow/tripmeter.h"

static TFT_eSPI tft;
static Preferences prefs;

// The panel's name on a network other than the hotspot (http://cardeck-leds.local/): the deck
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
// pages): it shows no selected colour, and each light's level when those differ.
static bool bothMixed() { return target == BOTH && bothlook::mixed(looks[INSIDE], looks[OUTSIDE]); }

using deckmenu::Rect;
using deckmenu::Screen;
using deckmenu::M_ITEM0;
using deckmenu::L_BACK;
using deckmenu::L_TAB0;
using deckmenu::L_TILE0;
using deckmenu::L_MINUS;
using deckmenu::L_PLUS;
using deckmenu::P_PRE0;

static Screen screen = Screen::Menu;  // the screen shown; boot lands on the Menu
// Lights on any of its tabs (Presets is its PRESETS tab).
static bool onLights() { return screen == Screen::Lights || screen == Screen::Presets; }
// The theme (see "Theme" above): a ledlook swatch index, or -1 for the green. Loop-owned;
// updateTheme() keeps it, from the looks.
static int theme = -1;
static bool updateTheme();
// Night (see "Night" above): the backlight at nightBacklight and termpal's night palette. Loop-owned;
// updateClock() keeps it.
static bool night = false;
static constexpr int NIGHT_BACKLIGHT = 85;  // of 255 (a third), picked on the panel with the night palette
static int nightBacklight = NIGHT_BACKLIGHT;  // the tuner may change it
#ifdef NIGHT_TEST
static bool tuneNight = true;  // the tuner's "D" turns it off to compare with the day
static int tuneTheme = -2;     // the tuner's forced theme; -2 none
#endif

// Karaoke (see "Karaoke" below): the Menu's KARAOKE item switches the black deck's karaoke view on
// and off, never saved (boots off). karaokeOn is loop()'s; karaokeWant is the same for the deck task
// (the keep-alive). karaokeTaps counts the switches on, so the deck task knows to tell the answer
// (keepalive::Confirm); karaokeShown is what it told (a keepalive::Shown), for loop() to show.
static bool karaokeOn = false;
static volatile bool karaokeWant = false;
static volatile uint32_t karaokeTaps = 0;
static volatile uint8_t karaokeShown = 0;

// Music mode (see "Music mode" below): on and off from the Menu's MUSIC item, never saved (boots
// off). musicOn is loop()'s; musicWant is the same for the deck task (the keep-alive) and the music
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

// The link task (core 0) connects; the loop (core 1) writes. Both go through yellow/elklink
// (Bluedroid's GATT client, shared with the PS3 controller), which writes by handle, so nothing
// the loop holds is freed by a reconnect.
struct Controller {
    const char* label;
    const char* addr;
    volatile bool fresh = false;  // just connected: needs the current state
};

// The two controllers' BLE addresses come from src/config.h (include/config.example.h says how to
// find them). A config.h without them doesn't build; the example's all-zero placeholders build and
// are named in the boot log (setup).
#if !defined(LED_INSIDE_MAC) || !defined(LED_OUTSIDE_MAC)
#error "Set LED_INSIDE_MAC and LED_OUTSIDE_MAC in src/config.h (see include/config.example.h)"
#endif
static constexpr const char* NO_MAC = "00:00:00:00:00:00";
static Controller controllers[] = {
    {"INSIDE", LED_INSIDE_MAC},
    {"OUTSIDE", LED_OUTSIDE_MAC},
};
static constexpr int N_CTRL = sizeof(controllers) / sizeof(controllers[0]);

static_assert(N_CTRL <= elklink::MAX, "a link per controller");
static int indexOf(const Controller& c) { return static_cast<int>(&c - controllers); }

static bool linkUp(const Controller& c) { return elklink::up(indexOf(c)); }

static void write(Controller& c, const elkcmd::Frame& f) {
    crashlog::at("led write");
    if (linkUp(c) && !elklink::write(indexOf(c), f.b, sizeof(f.b))) {
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

// A running fade (see "A new music look" below; presets use it too), or the welcome's intro (see
// startIntro): its keys in place of from..to, each at its time after `start`. Loop-owned.
struct LightFade {
    songpalette::Rgb from{}, to{};
    int steps = 0, done = 0;  // done == steps: no fade
    uint32_t every = 0, at = 0;  // at: millis() of the last step written
    ledintro::Track intro;  // intro.n > 0: the welcome (steps == intro.n)
    uint32_t start = 0;
};
static LightFade lightFade[N_CTRL];

// A look goes out at most every PUSH_GAP_MS per light, the latest one: the inside controller takes
// ~7 writes a second and a look is four, so a burst of presses (L1/R1 held down, the pad's
// shortcuts) queued writes on the stack faster than the light took them. A fade or the welcome drops a look still waiting. Loop-owned.
static constexpr uint32_t PUSH_GAP_MS = 300;
static bool pushWanted[N_CTRL] = {};
static uint32_t pushAt[N_CTRL] = {};

// The colour frame push() sends for `l` (black when off).
static songpalette::Rgb frameOf(const Look& l) {
    if (!l.lit) return {0, 0, 0};
    const Rgb s = ledRgb(l.swatch);
    return {elkcmd::dim(s.r, l.pct), elkcmd::dim(s.g, l.pct), elkcmd::dim(s.b, l.pct)};
}

// The colour frame light i shows now: a fade's current step, else its look's.
static songpalette::Rgb frameNow(int i) {
    const LightFade& f = lightFade[i];
    if (f.done < f.steps && f.intro.n) return f.intro.k[f.done ? f.done - 1 : 0].c;
    if (f.done < f.steps) return musiclights::blend(f.from, f.to, f.done, f.steps);
    return frameOf(shownLook(i));
}

static void writeFrame(int i, const songpalette::Rgb& c) {
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(controllers[i], elkcmd::color(v, c.r, c.g, c.b));
}

// Light i from `from` to `to` in `steps` steps `every` ms apart. powerOn: the power-on goes now with
// the first step, so the light never shows its old colour; else the first step waits for its gap.
static void startFade(int i, const songpalette::Rgb& from, const songpalette::Rgb& to, bool powerOn, int steps, uint32_t every) {
    LightFade& f = lightFade[i];
    pushWanted[i] = false;  // the fade writes from here on
    f.intro.n = 0;  // a fade ends the welcome
    f.from = from;
    f.to = to;
    f.steps = steps;
    f.done = 0;
    f.every = every;
    if (!powerOn || !linkUp(controllers[i])) return;
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(controllers[i], elkcmd::power(v, true));
    f.done = 1;
    f.at = millis();
    writeFrame(i, musiclights::blend(f.from, f.to, f.done, f.steps));
}

// Key n of light i's welcome: its colour frame, or the start of the inside light's curtain fill
// (its speed, then the effect; the MELK frames only, the ELK one ignores them). Each key is logged
// with its time since the welcome began, sent or not (no link: the desk's INTRO replay), so
// tools/intro_check.py can hold the firmware's timeline to ledintro's.
static void writeKey(int i, int n) {
    const LightFade& f = lightFade[i];
    const ledintro::Key& k = f.intro.k[n];
    const bool up = linkUp(controllers[i]);
    if (k.fill) {
        Serial.printf("[intro] %s +%u ms key %d/%d: fill speed %u effect %u, %s\n", controllers[i].label,
                      static_cast<unsigned>(millis() - f.start), n + 1, f.intro.n,
                      static_cast<unsigned>(ledintro::FILL_SPEED), static_cast<unsigned>(ledintro::FILL_MODE), up ? "sent" : "no link");
        if (!up) return;
        write(controllers[i], elkcmd::speed(ledintro::FILL_SPEED));
        write(controllers[i], elkcmd::effect(ledintro::FILL_MODE));
        return;
    }
    Serial.printf("[intro] %s +%u ms key %d/%d: colour %u,%u,%u, %s\n", controllers[i].label,
                  static_cast<unsigned>(millis() - f.start), n + 1, f.intro.n, k.c.r, k.c.g, k.c.b, up ? "sent" : "no link");
    if (up) writeFrame(i, k.c);
}

// The welcome, the first time since boot light i links lit (ledintro: the inside light's strip
// fills from both ends inwards, then the outside light rises, ~6 s): its power-on and first key
// (dark) now, the rest from fadeTick. A change, a preset or a song's look takes over from the key
// it reached (its colour frame ends the fill); a reconnect gets its look (push).
static void startIntro(int i) {
    const Look l = shownLook(i);
    const Rgb s = ledRgb(l.swatch);
    LightFade& f = lightFade[i];
    pushWanted[i] = false;  // the welcome writes from here on
    f = LightFade();
    f.intro = ledintro::plan(i == 0, {s.r, s.g, s.b}, l.pct);
    f.steps = f.intro.n;
    f.start = f.at = millis();
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(controllers[i], elkcmd::power(v, true));
    writeKey(i, 0);
    f.done = 1;
}

// Controller i's look (power + dimmed colour), both header variants: four writes.
static void pushNow(int i) {
    Controller& c = controllers[i];
    const Look l = shownLook(i);
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) write(c, elkcmd::power(v, l.lit));
    if (!l.lit) return;
    const Rgb s = ledRgb(l.swatch);
    for (elkcmd::Variant v : {elkcmd::V0, elkcmd::V1}) {
        write(c, elkcmd::color(v, elkcmd::dim(s.r, l.pct), elkcmd::dim(s.g, l.pct), elkcmd::dim(s.b, l.pct)));
    }
}


// Each loop pass, and from push(): the wanted look of each light whose gap has passed.
static void pushTick() {
    for (int i = 0; i < N_CTRL; i++) {
        if (!pushWanted[i] || (pushAt[i] && millis() - pushAt[i] < PUSH_GAP_MS)) continue;
        pushWanted[i] = false;
        pushAt[i] = millis() | 1;
        pushNow(i);
    }
}

// Controller i gets its look: now, or once its gap has passed (pushTick). Ends a running fade.
static void push(int i) {
    lightFade[i].done = lightFade[i].steps;
    pushWanted[i] = true;
    pushTick();
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
    if (!elklink::connect(indexOf(c), c.label)) return false;
    Serial.printf("[leds] %s: connected\n", c.label);
    c.fresh = true;
    return true;
}

// The PS3 controller (DualShock 3, Bluetooth Classic): it connects to the deck by itself when
// its PS button is pressed, to the address stored in it (its "master", written over USB: the
// boot log's "[pad] deck address"). Nothing to scan or tick here; its reports come in callbacks.
// Its LEDs are set by the loop once it's up (padSend), not Ps3.setPlayer (a 4 KB buffer).

// All connecting runs here on core 0, so the touch loop (core 1) never waits on it. Connects keep
// the radio, which WiFi shares, busy: with the lights out of range (the deck at home), 5 s retries
// starved WiFi until the deck didn't even answer ping. So the lights connect only while one is
// missing, in rounds 5 s apart, doubling after each failed round up to 60 s, back to 5 s (at once)
// when a connected light drops. An update over WiFi (net/ota) has the radio: no connects until it
// ends; connected lights and the controller stay as they are.
static TaskHandle_t linkHandle = nullptr;

static void linkTask(void*) {
    uint32_t waitMs = 5000, nextLights = 0;
    watchdog::watch();  // a Bluetooth wait that never returns reboots the deck (common/core/watchdog)
    for (;;) {
        watchdog::feed();
        if (ota::active()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const uint32_t now = millis();

        // lost: a light that was up last tick and isn't now
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

// The grid is all colours (ledlook's first N_GRID; cyan is a preset's only); there is no power
// tile: double-tapping the lit colour turns the lights off, and tapping any colour turns them back on.
static_assert(ledlook::N_GRID == 8 && L_MINUS - L_TILE0 == ledlook::N_GRID, "the grid is 2x4 colours");
static_assert(deckmenu::P_N - P_PRE0 == ledpresets::N, "a button per preset");
static_assert(static_cast<int>(deckmenu::TAB_BOTH) == BOTH && static_cast<int>(deckmenu::TAB_INSIDE) == INSIDE &&
                  static_cast<int>(deckmenu::TAB_OUTSIDE) == OUTSIDE,
              "the look pages' tabs come first, in Target order");

static bool built(int id) { return deckmenu::rect(screen, id).w > 0; }

// The button the PS3 controller points at; -1 while none is connected. On the Menu it is drawn as
// an inverted bar, on Lights as a TEXT ring around the button (the lit colour is the bar there).
static int focus = -1;
static bool padUp = false;  // a PS3 controller is connected
static void drawFocus(int id, bool on);

// Where the focus starts on each screen: the first item, the first preset, or the colour the page
// shows (blue for cyan, which has no tile).
static int homeFocus() {
    if (screen == Screen::Menu) return M_ITEM0;
    if (screen == Screen::Presets) return P_PRE0;
    return L_TILE0 + ledlook::gridSwatch(cur().swatch);
}

// ---- Drawing ----
// The lettering is VT323, a CRT terminal font, 1-bit at four sizes named by role (ui/termfont:
// SMALL, BODY, TITLE, BIG); where each label goes so it fits is ui/termlayout's, which
// test_termfont checks. Every label is upper case. The touch calibration
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
#ifdef NIGHT_TEST
// The night tuner's palette (see tuneLine): each theme's night colours (index theme + 1), live.
static uint32_t tuned[9][5];
static bool tunedReady = false;
static uint32_t tunedRgb(termpal::Role r) {
    if (!tunedReady) {
        for (int t = -1; t < 8; t++) {
            for (int k = termpal::BG; k <= termpal::ALERT; k++) tuned[t + 1][k] = termpal::rgb(static_cast<termpal::Role>(k), t, true);
        }
        tunedReady = true;
    }
    return tuned[theme < -1 || theme > 7 ? 0 : theme + 1][r];
}
static uint16_t pal(termpal::Role r) { return c565(tunedRgb(r)); }  // vivid day and night
#else
// termpal's vivid colours day and night (the day's pale tints looked dull); night only dims the backlight.
static uint16_t pal(termpal::Role r) { return c565(termpal::rgb(r, theme, true)); }
#endif
// A terminal message's colour: ALERT for a problem (a lost link, the wrong key, a car warning: alert),
// else TEXT.
static bool isAlertMsg(const char* m) {
    return lightstatus::isAlert(m) || padbattery::isAlert(m);
}
static uint16_t msgInk(const char* m, bool alert) { return pal(alert || isAlertMsg(m) ? termpal::ALERT : termpal::TEXT); }

static void clearBg(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft.fillRect(x, y, w, h, pal(termpal::BG));
    const uint16_t line = c565(termpal::scan());
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
// the inverted bar when focused. Only the PS3 controller focuses (focus is -1 without it), so
// without one the items look alike and the clock and the lights band lead. `tag` (KARAOKE's and
// MUSIC MODE's ON / OFF; nullptr for none): under the text at the right edge, an
// inverted mini-label in the text's ink with the ground's colour for letters (TEXT, not ALERT: red
// is for problems).
static void drawRow(const Rect& r, const char* s, bool pressed, bool inverted, const char* tag) {
    const uint16_t ink = drawBox(r, pressed, inverted, 1, pal(termpal::TEXT));
    tft.setTextColor(ink);
    text(s, r.x + ROW_TEXT_X, r.y + ROW_TEXT_Y, Face::BODY, TL_DATUM);
    if (!tag) return;
    const int w = termfont::width(Face::SMALL, tag) + 2 * TAG_PAD, h = termfont::capHeight(Face::SMALL) + 2 * TAG_PAD;
    const int x = r.x + r.w - TAG_RIGHT - w, y = r.y + r.h - TAG_BOTTOM - h;
    tft.fillRect(x, y, w, h, ink);
    const bool onBar = presslook::box(pressed, inverted).bar;  // drawBox's bar
    tft.setTextColor(pal(onBar ? termpal::TEXT : termpal::BG));  // the ground's
    text(tag, x + w / 2, y + h / 2, Face::SMALL, MC_DATUM);
}

// A colour chip of `l`: filled when lit, a DIM outline when
// off. On a bar, a 1 px BG edge keeps it apart from the fill (pink on yellow, white on green).
static void drawChip(int x, int y, int w, int h, const Look& l, bool onBar) {
    if (l.lit) tft.fillRect(x, y, w, h, c565(screenOf(l.swatch)));
    else tft.drawRect(x, y, w, h, pal(termpal::DIM));
    if (onBar) tft.drawRect(x - 1, y - 1, w + 2, h + 2, pal(termpal::BG));
}

// A preset on the PRESETS tab (100x60): the name (TITLE) over two chips big enough to tell apart at
// a glance (inside, outside); drawBox's pressed look. The controller's focus is the ring around it.
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
// The lit colour is the inverted bar, chip kept; the controller's focus is a separate ring around it.
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
        const char* tag = nullptr;
        if (id == M_ITEM0 + deckmenu::KARAOKE) tag = karaokeOn ? ON_TAG : OFF_TAG;
        else if (id == M_ITEM0 + deckmenu::MUSIC) tag = musicOn ? ON_TAG : OFF_TAG;
        drawRow(r, MENU_ITEMS[id - M_ITEM0], pressed, id == focus, tag);
        return;
    }
    if (id == L_BACK) {
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
// shade block; the font's ASCII table has neither glyph), and the % under it (TITLE), "OFF" in DIM
// when off. On BOTH while the lights differ the bar is the inside light's and the line has both
// levels when they differ ("IN 60%  OUT 40%", SMALL: TITLE doesn't fit the 108 px rect).
static void drawLevel() {
    const Rect& r = deckmenu::LEVEL;
    const bool mixed = bothMixed();
    const Look& l = mixed ? looks[INSIDE] : cur();
    const bool live = mixed || l.lit;
    clearBg(r);
    tft.drawRect(r.x, r.y, r.w, r.h, pal(termpal::MID));
    constexpr int N_CELL = 10, CELL = 8, GAP = 2;
    const int full = l.lit ? l.pct / 10 : 0;
    const int x0 = r.x + (r.w - (N_CELL * (CELL + GAP) - GAP)) / 2;
    for (int k = 0; k < N_CELL; k++) {
        const int x = x0 + k * (CELL + GAP);
        if (k < full) tft.fillRect(x, r.y + 7, CELL, 14, pal(termpal::TEXT));
        else tft.drawRect(x, r.y + 7, CELL, 14, pal(live ? termpal::MID : termpal::DIM));
    }
    char line[24];
    if (mixed) bothlook::levelText(looks[INSIDE], looks[OUTSIDE], line, sizeof(line));
    else bothlook::levelText(l, l, line, sizeof(line));
    tft.setTextColor(pal(live ? termpal::TEXT : termpal::DIM));
    const bool both = strchr(line, ' ') != nullptr;  // "IN 60%  OUT 40%"
    text(line, r.x + r.w / 2, r.y + 34, both ? Face::SMALL : Face::TITLE, MC_DATUM);
}

// A terminal message (">ROSE APPLIED", ">INSIDE: NO LINK") in place of the Lights page's STATUS
// line, or on the Menu's prompt line (in place of ">" or music mode's line), for MESSAGE_MS since
// messageAt; loop() then puts the usual line back. A newer message replaces it, and so does a screen
// change (show() drops it), so none outlives its screen. Only loop() touches it (and setup(), before
// loop() starts).
static constexpr uint32_t MESSAGE_MS = 2500;
static constexpr uint32_t KARAOKE_WAIT_MS = 15000;  // ">KARAOKE: CONNECTING..." until an answer replaces it
static constexpr uint32_t LAST_TRIP_MS = 6000;  // the boot's ">LAST TRIP ..." is worth longer to read
static char message[32] = "";  // "" when none
static_assert(sizeof(message) <= MARKED_RUN, "a message holding GOJIRA's mark is drawn whole (textMarked)");
static uint32_t messageAt = 0;
static uint32_t messageMs = MESSAGE_MS;
static bool messageAlert = false;  // drawn in ALERT whatever it says (the car's warnings)

// Sets the message; the caller redraws what shows it (drawMessage, or drawStatus on Lights).
static void setMessage(const char* m, uint32_t ms = MESSAGE_MS, bool alert = false) {
    snprintf(message, sizeof(message), "%s", m);
    messageAt = millis();
    messageMs = ms;
    messageAlert = alert;
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
    tft.setTextColor(message[0] ? msgInk(message, messageAlert) : pal(down ? termpal::ALERT : termpal::TEXT));
    text(message[0] ? message : line, r.x + STATUS_TEXT_X, r.y + r.h, Face::SMALL, BL_DATUM);
}

// ---- The Menu: a dashboard of stacked bands ----
// Top to bottom (termlayout places them, test_termfont checks they fit): the header line; one row of
// the clock and the date left, the trip's time right; both lights, each a block of its colour with
// its name and level; the prompt line (a message, music mode's line, or just ">") with its block
// cursor; the three tall items at the bottom. A MID rule under each band but the
// prompt line. Each part clears and draws its own rows, so it is redrawn alone when what it shows
// changes: the clock each minute, the weather on a new reading, the trip when its minute or its
// tenth of a km moves, the lights on a look or link change, the prompt line when a message comes or
// goes or music mode's line changes, the cursor's cell on a blink.

static void drawRule(int y) { tft.drawFastHLine(RULE_X0, y, RULE_X1 - RULE_X0, pal(termpal::MID)); }

// The PS3 controller's battery at the header's right while one is connected (padUp): "PAD" (MID)
// and a battery icon on the title's caps, its outline MID and 0-4 cells TEXT (both ALERT when it
// is dying), cycling while it charges (util/yellow/padbattery). Nothing without a controller.
static padbattery::Look padBattery;  // what the controller last reported; loop-owned
static void drawPadBattery() {
    const int x0 = DASH_RIGHT - termlayout::padWidth();
    clearBg(x0, 0, RULE_X1 - x0, HEADER_RULE_Y);
    if (!padUp) return;
    tft.setTextColor(pal(termpal::MID));
    const int ix = x0 + termfont::width(Face::SMALL, PAD_LABEL) + PAD_GAP;
    text(PAD_LABEL, x0, DASH_TITLE_Y, Face::SMALL, TL_DATUM);
    const int h = termfont::capHeight(Face::SMALL), y = DASH_TITLE_Y;
    const uint16_t edge = pal(padBattery.alert ? termpal::ALERT : termpal::MID);
    const uint16_t fill = pal(padBattery.alert ? termpal::ALERT : termpal::TEXT);
    tft.drawRect(ix, y, PAD_ICON_W, h, edge);
    tft.fillRect(ix + PAD_ICON_W, y + (h - PAD_NUB_H) / 2, PAD_NUB_W, PAD_NUB_H, edge);
    const int cells = padbattery::shownCells(padBattery, millis());
    for (int k = 0; k < cells; k++) {
        tft.fillRect(ix + 2 + k * (PAD_CELL_W + PAD_CELL_GAP), y + 2, PAD_CELL_W, h - 4, fill);
    }
}

// The header: one MID line and its rule (the boot's welcome lines typed out before it), and the
// controller's battery right-aligned on it, well clear of the title (test_termfont).
static void drawHeader() {
    clearBg(deckmenu::HEADER);
    tft.setTextColor(pal(termpal::MID));
    text(DASH_TITLE, DASH_X, DASH_TITLE_Y, Face::SMALL, TL_DATUM);
    drawRule(HEADER_RULE_Y);
    drawPadBattery();
}

// The clock's minute of the day on screen (-1: not synced, "--:--") and its date, read together
// (netclock::localNow) so they never straddle midnight; updateClock keeps them.
static int clockMin = -1;
static int clockY = 0, clockMonth = 0, clockDay = 0;

// The clock row's left: the clock (TITLE, TEXT) and after it the date (SMALL, MID: "FRI 23 OCT",
// "--- -- ---" before the clock synced). The date only turns over with a minute, so both go together.
static void drawClock() {
    char s[16];  // room for any int, so -Wformat-truncation stays quiet
    if (clockMin < 0) snprintf(s, sizeof(s), "--:--");
    else snprintf(s, sizeof(s), "%02d:%02d", clockMin / 60, clockMin % 60);
    char date[16];
    dashtext::date(clockMin >= 0, clockY, clockMonth, clockDay, date, sizeof(date));
    const int top = HEADER_RULE_Y + 1;
    clearBg(RULE_X0, top, DASH_SPLIT - RULE_X0, CLOCK_RULE_Y - top);  // down to the band's rule
    tft.setTextColor(pal(termpal::TEXT));
    text(s, DASH_X, CLOCK_Y, Face::TITLE, TL_DATUM);
    tft.setTextColor(pal(termpal::MID));
    text(date, termlayout::dateX(), DATE_Y, Face::SMALL, TL_DATUM);
}

// The trip meter: time since the first loop() pass (the car started the deck). Its NVS record is
// the boot's ">LAST TRIP".
static tripmeter::Meter tripMeter;

// The clock row's right, right-aligned: the trip's time (TITLE, TEXT: "0:42", hours and minutes) and
// TRIP before it (SMALL, MID). tripShown is the minute on screen: tripTick redraws it when it moves.
static uint32_t tripShown = UINT32_MAX;
static void drawTripTime() {
    tripShown = tripMeter.trip().seconds / 60U;
    char s[16];
    snprintf(s, sizeof(s), "%u:%02u", static_cast<unsigned>(tripShown / 60U), static_cast<unsigned>(tripShown % 60U));
    const int top = HEADER_RULE_Y + 1;
    clearBg(DASH_SPLIT, top, RULE_X1 - DASH_SPLIT, CLOCK_RULE_Y - top);
    tft.setTextColor(pal(termpal::TEXT));
    text(s, DASH_RIGHT, CLOCK_Y, Face::TITLE, TR_DATUM);
    tft.setTextColor(pal(termpal::MID));
    text(TRIP_LABEL, DASH_RIGHT - termfont::width(Face::TITLE, s) - CLOCK_GAP, WORDS_Y, Face::SMALL, TR_DATUM);
}

// The lights band, cleared between its rules and drawn again: INSIDE left of a vertical MID rule,
// OUTSIDE right of it, each a block of its colour, its label (SMALL, MID) and under it what
// lightstatus::of says of it (LIGHT_VALUE_FACE): on, the block filled and "ROSE 40%" (TEXT); off, a
// DIM outlined block and OFF in DIM; no link, "-- NO LINK" in ALERT and no block (never its last
// look: the deck can't tell what a light it has lost shows).
static_assert(N_CTRL == N_LIGHT_LABELS, "a label per light");
static void drawLightsBand() {
    const int top = CLOCK_RULE_Y + 1;
    clearBg(RULE_X0, top, RULE_X1 - RULE_X0, LIGHTS_RULE_Y - top);
    tft.drawFastVLine(LIGHTS_SPLIT, top, LIGHTS_RULE_Y - top, pal(termpal::MID));
    for (int i = 0; i < N_CTRL; i++) {
        const Look l = shownLook(i);  // music mode's while it drives the lights
        tft.setTextColor(pal(termpal::MID));
        text(LIGHT_LABELS[i], lightTextX(i), LIGHT_LABEL_Y, Face::SMALL, TL_DATUM);
        const lightstatus::State st = lightstatus::of(linkUp(controllers[i]), l.lit);
        if (st == lightstatus::State::NoLink) {
            tft.setTextColor(pal(termpal::ALERT));
            text("-- NO LINK", lightTextX(i), LIGHT_VALUE_BASE, LIGHT_VALUE_FACE, BL_DATUM);
            continue;
        }
        drawChip(lightX(i), LIGHT_BLOCK_Y, LIGHT_BLOCK_W, LIGHT_BLOCK_H, l, false);
        if (st == lightstatus::State::Off) {
            tft.setTextColor(pal(termpal::DIM));
            text("OFF", lightTextX(i), LIGHT_VALUE_BASE, LIGHT_VALUE_FACE, BL_DATUM);
            continue;
        }
        char name[12], s[32];
        upper(SWATCHES[l.swatch].name, name, sizeof(name));
        snprintf(s, sizeof(s), "%s %u%%", name, static_cast<unsigned>(l.pct));
        tft.setTextColor(pal(termpal::TEXT));
        text(s, lightTextX(i), LIGHT_VALUE_BASE, LIGHT_VALUE_FACE, BL_DATUM);
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
        tft.setTextColor(msgInk(message, messageAlert));
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
    tft.fillRect(songChipX(), PROMPT_BASE - MUSIC_CHIP, MUSIC_CHIP, MUSIC_CHIP, c565(SWATCHES[musicSwatch].screen));
    char name[12];
    upper(SWATCHES[musicSwatch].name, name, sizeof(name));
    placeCursor(songNameX() + text(name, songNameX(), PROMPT_BASE, Face::SMALL, BL_DATUM));
}

// The whole dashboard but its three items (drawAll draws those as buttons).
static void drawDash() {
    drawHeader();
    drawClock();
    drawTripTime();
    drawRule(CLOCK_RULE_Y);
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

// Lights (the Menu's focus is the inverted bar, drawn by the button itself): a TEXT ring in
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

// Redraws the current screen in the current palette.
static void drawAll() {
    clearBg(0, 0, tft.width(), tft.height());
    if (screen != Screen::Menu) drawTabRule(0, tft.width());
    for (int id = 0; id < deckmenu::count(screen); id++) {
        if (built(id)) drawButton(id);
    }
    if (screen == Screen::Menu) {
        drawDash();
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

// ---- Trip meter: the clock and the NVS record ("trip", 9 bytes) ----
static tripclock::Clock tripClock;
static uint32_t tripStartMs = 0;  // millis() of the first tick, 0: not started
static uint8_t tripSaved[tripmeter::RECORD_BYTES];
static bool tripSavedValid = false;

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
    tripMeter.setSeconds(tripClock.seconds(now));
    const tripmeter::Trip t = tripMeter.trip();
    if (screen == Screen::Menu && t.seconds / 60U != tripShown) drawTripTime();
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
// shows each light's level, and the main screen's lights band tells each light's look. From a tap, A, voice or
// the watch. The lights fade to it from what they show (even a song's): the inside over ~1 s, the
// outside, whose controller takes ~1 write a second, half now and the rest 1 s later.
static constexpr int PRESET_FADE_IN = 4, PRESET_FADE_OUT = 2;
static constexpr uint32_t PRESET_STEP_IN_MS = 300, PRESET_STEP_OUT_MS = 1000;
static void applyPreset(int slot0) {
    const songpalette::Rgb from[N_CTRL] = {frameNow(0), frameNow(1)};
    musicYield();  // both lights get the preset anyway
    const ledpresets::Preset& p = ledpresets::PRESETS[slot0];
    looks[INSIDE] = p.in;
    looks[OUTSIDE] = p.out;
    looks[BOTH] = bothlook::seed(p.in, p.out);
    startFade(0, from[0], frameOf(p.in), true, PRESET_FADE_IN, PRESET_STEP_IN_MS);
    startFade(1, from[1], frameOf(p.out), true, PRESET_FADE_OUT, PRESET_STEP_OUT_MS);
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
static void show(Screen s) {
    screen = s;
    tap.id = -1;
    padFlash = -1;
    tapGuard.arm();
    message[0] = 0;
    focus = padUp ? homeFocus() : -1;
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
static void drawMusicItem();
static void setKaraoke(bool on);
static void drawKaraokeItem();

// One settled tap on a button of the current screen.
static void press(int id) {
    if (onLights()) {
        pressLights(id);
        return;
    }
    if (id < M_ITEM0 || id >= M_ITEM0 + deckmenu::N_ITEMS) return;
    switch (static_cast<deckmenu::Item>(id - M_ITEM0)) {
    case deckmenu::LIGHTS: show(lightsScreen()); break;
    case deckmenu::KARAOKE: setKaraoke(!karaokeOn); break;  // the caller redraws the item (its ON tag)
    case deckmenu::MUSIC: setMusic(!musicOn); break;
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

// ---- PS3 controller input ----
// Every press goes through the guard first (util/padguard): light touches, set-downs and stuck
// buttons do nothing.
// Moving around (the screen's buttons):
//   D-pad: move the focus (held: repeats; on the Menu left / right along its three items, no wrap)
//   cross: press the focused button    circle: back to the Menu (from Lights; on the Menu: nothing)
//   Lights only: square next page (BOTH, INSIDE, OUTSIDE, PRESETS, round)
// Shortcuts, on any screen, for both lights; each buzzes its own way (util/padfeel):
//   START: lights on / off (firm)    SELECT: music mode on / off (two ticks)
//   triangle: karaoke on / off (long)    L2 / R2: previous / next preset (tick)
//   L1 / R1: both lights - / + (tick)    right stick flicked left / right: previous / next colour,
//   each light keeping its level and on/off (tick)    PS: back to the Menu
// The player LEDs: 1 connected, 2 the inside light linked, 3 the outside one, 4 the pad's battery low.

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

// The battery from the controller's reports: the header redrawn on a change (and every 500 ms
// while it charges, a cell more each time); ">PAD: BATTERY LOW" (ALERT) once as it starts dying.
static void padBatteryTick() {
    const uint8_t raw = static_cast<uint8_t>(Ps3.data.status.battery);
    const padbattery::Look now = padbattery::next(padBattery, raw);
    static uint32_t frameAt = 0;
    const bool frame = now.charging && millis() - frameAt >= 500;
    if (now == padBattery && !frame) return;
    if (now.alert && !padBattery.alert) {
        setMessage(padbattery::LOW_MESSAGE);
        drawMessage();
    }
    if (now != padBattery) Serial.printf("[pad] battery %d cells%s (0x%02x)\n", now.cells, now.charging ? ", charging" : "", raw);
    padBattery = now;
    frameAt = millis();
    if (screen == Screen::Menu) drawPadBattery();
}

// The controller's rumble and LEDs go out together (one output report carries both, yellow/padhid),
// so every buzz re-sends the LEDs. padLeds: what they last showed; -1 forces the next send (a new
// connection, or a report that didn't go out).
static padfeel::Buzz padBuzz;
static int padLeds = -1;

static uint32_t padRetryAt = 0;  // after a report that didn't go out: the LEDs' next try

// The LEDs, with a buzz when `p` is one (nullptr: LEDs only).
static void padSend(const padfeel::Pulse* p) {
    const uint8_t bits = padfeel::leds(linkUp(controllers[0]), linkUp(controllers[1]), padBattery.alert);
    if (padhid::send(bits, p ? p->smallMs : 0, p ? p->bigMs : 0, p ? p->bigPower : 0)) {
        padLeds = bits;
    } else {  // the LEDs try again in 500 ms; a lost buzz is just lost
        padLeds = -1;
        padRetryAt = millis() + 500;
    }
}

// The buzz for a shortcut (util/padfeel): its feel, whether it turned something on, and the lights
// it was for (one that isn't linked adds a weak long buzz).
static void padFelt(padfeel::Feel f, bool on, bool toIn, bool toOut) {
    const bool partial = (toIn && !linkUp(controllers[0])) || (toOut && !linkUp(controllers[1]));
    padBuzz.start(f, on, partial, millis());
}

// Each tick while connected: a due buzz, else the LEDs when a link or the battery changed them.
static void padFeelTick() {
    padfeel::Pulse p;
    if (padBuzz.due(millis(), &p)) {
        Serial.println("[pad] buzz");
        padSend(&p);
        return;
    }
    const int bits = padfeel::leds(linkUp(controllers[0]), linkUp(controllers[1]), padBattery.alert);
    if (bits != padLeds && static_cast<int32_t>(millis() - padRetryAt) >= 0) padSend(nullptr);
}

// START: both lights on, or off if either is on.
static void padLightsPower() {
    bothlook::Change c;
    c.power = looks[INSIDE].lit || looks[OUTSIDE].lit ? 0 : 1;
    change(BOTH, c);
    save();
    setMessage(c.power ? ">LIGHTS ON" : ">LIGHTS OFF");
    if (!drawLooks()) drawMessage();
    padFelt(padfeel::Feel::Firm, c.power == 1, true, true);
}

// L1 / R1: both lights a step down / up.
static void padLevel(int step) {
    bothlook::Change c;
    c.step = step;
    change(BOTH, c);
    save();
    drawLooks();
    padFelt(padfeel::Feel::Tick, true, true, true);
}

// L2 / R2: the previous / next preset, round; the first press goes to the first (R2) or last (L2).
static void padPreset(int dir) {
    static int slot = -1;
    slot = slot < 0 ? (dir > 0 ? 0 : ledpresets::N - 1) : (slot + dir + ledpresets::N) % ledpresets::N;
    applyPreset(slot);
    padFelt(padfeel::Feel::Tick, true, true, true);
}

// After a colour on both lights (the right stick, the IR remote): both pushed and saved, and its
// name as the message (">RED").
static void colourShown(int sw) {
    for (int i = 0; i < N_CTRL; i++) push(i);
    save();
    char m[16];
    snprintf(m, sizeof(m), ">%s", ledlook::SWATCHES[sw].name);
    for (char* c = m; *c; c++) *c = static_cast<char>(toupper(static_cast<unsigned char>(*c)));
    setMessage(m);
    if (!drawLooks()) drawMessage();
}

// The right stick's flick: the previous / next grid colour on both lights, from the inside one's;
// each keeps its own level and on/off (bothlook::recolour). Ends music mode like any manual change.
static void padColour(int dir) {
    const int sw = ledlook::stepGrid(looks[INSIDE].swatch, dir);
    musicYield();
    bothlook::recolour(sw, &looks[BOTH], &looks[INSIDE], &looks[OUTSIDE]);
    colourShown(sw);
    Serial.printf("[pad] colour %s\n", ledlook::SWATCHES[sw].word);
    padFelt(padfeel::Feel::Tick, true, true, true);
}

// Triangle: karaoke on / off, as the Menu's KARAOKE item.
static void padKaraoke() {
    setKaraoke(!karaokeOn);
    drawKaraokeItem();
    padFelt(padfeel::Feel::Long, karaokeOn, false, false);
}

// The light kit's IR remote, on the dash next to the deck (lib/irrx, util/yellow/irkeys): every
// key acts on both lights, as the PS3 controller's do but without a buzz. A held brightness key
// steps about four times a second; every other key acts once per press.
static irkeys::Repeat irRepeat;

// The outside light's effect frame, sent 1 s after its speed frame (its controller drops a write
// within ~1 s of the last): the mode, or -1. A later key drops it.
static int irEffectDue = -1;
static uint32_t irEffectAt = 0;

// JUMP / FADE: the controllers' own effect `mode` on each lit light (elkcmd: speed, then the
// effect; the MELK frames, the ELK one ignores them). The looks don't change: the next colour,
// level or power press ends the effect with its colour frame. Ends music mode like any manual
// change, and stops a fade or the welcome that would write over it.
static void irEffect(uint8_t mode, const char* name) {
    musicYield();
    bool lit = false;
    for (int i = 0; i < N_CTRL; i++) {
        if (!shownLook(i).lit) continue;
        lit = true;
        pushWanted[i] = false;
        lightFade[i].intro.n = 0;
        lightFade[i].done = lightFade[i].steps;
        if (!linkUp(controllers[i])) continue;
        write(controllers[i], elkcmd::speed(irkeys::EFFECT_SPEED));
        if (i == 0) {
            write(controllers[i], elkcmd::effect(mode));
        } else {
            irEffectDue = mode;
            irEffectAt = millis();
        }
    }
    Serial.printf("[ir] effect mode %u%s\n", static_cast<unsigned>(mode), lit ? "" : " (lights off)");
    char m[16];
    snprintf(m, sizeof(m), ">%s", lit ? name : "LIGHTS OFF");
    setMessage(m);
    drawMessage();
}

static void irTick() {
    if (irEffectDue >= 0 && millis() - irEffectAt >= 1000) {
        write(controllers[1], elkcmd::effect(static_cast<uint8_t>(irEffectDue)));
        irEffectDue = -1;
    }
    uint16_t address;
    uint8_t command;
    bool repeat;
    if (!irrx::poll(&address, &command, &repeat)) return;
    irkeys::Key k;
    if (!irkeys::lookup(address, command, &k)) {
        Serial.printf("[ir] not the lights' remote: %04X %02X\n", address, command);
        return;
    }
    if (!irRepeat.accept(k, repeat, millis())) return;
    const irkeys::Action a = irkeys::action(k);
    Serial.printf("[ir] key %u%s\n", static_cast<unsigned>(command), repeat ? " held" : "");
    if (a.kind != irkeys::Kind::Playlist) irEffectDue = -1;
    bothlook::Change c;
    switch (a.kind) {
        case irkeys::Kind::Level:
            c.step = a.value;
            change(BOTH, c);
            save();
            drawLooks();
            break;
        case irkeys::Kind::Power:
            c.power = a.value;
            change(BOTH, c);
            save();
            setMessage(c.power ? ">LIGHTS ON" : ">LIGHTS OFF");
            if (!drawLooks()) drawMessage();
            break;
        case irkeys::Kind::Colour:
            musicYield();
            bothlook::paint(a.value, &looks[BOTH], &looks[INSIDE], &looks[OUTSIDE]);
            colourShown(a.value);
            break;
        case irkeys::Kind::Effect: {
#ifdef IR_EFFECT_PROBE
            // the car's tuning build: JUMP3 / JUMP7 step the mode, FADE3 sends it (FADE7 as usual)
            static int probe = 0;
            if (a.value < 2) {
                probe = a.value == 0 ? (probe > 0 ? probe - 1 : 0) : (probe < 212 ? probe + 1 : 212);
                char m[24];
                snprintf(m, sizeof(m), ">MODE %d", probe);
                setMessage(m);
                drawMessage();
                Serial.printf("[ir] probe mode %d\n", probe);
                break;
            }
            if (a.value == 2) {
                irEffect(static_cast<uint8_t>(probe), "PROBE");
                break;
            }
#endif
            static const char* const NAMES[4] = {"JUMP3", "JUMP7", "FADE3", "FADE7"};
            irEffect(irkeys::EFFECT_MODE[a.value], NAMES[a.value]);
            break;
        }
        case irkeys::Kind::Playlist:
            break;  // Task 6
    }
}

// The buttons the loop acts on, from each report (onPadReport, Ps3.attach, the Bluetooth task):
// the held ones, and every one seen down since the loop last looked. The DS3 reports only ~19
// times a second next to WiFi and the lights, so a quick tap is a single report, ~50 ms, and a
// loop pass busy drawing could miss it; the latch keeps it. Bits as the DS3's report has them
// (ps3_parser.c's masks).
static std::atomic<uint32_t> padHeld{0}, padSeen{0};
static std::atomic<uint32_t> padReports{0};  // reports since boot: the [pad] rate line

// The guard (util/padguard) decides which presses count: light touches, a set-down's many light
// presses, the moments the pad is picked up or put down, and a button stuck on all do nothing.
// The Bluetooth task's alone. The loop logs each press's trigger pressure and each of the guard's
// verdicts (padChord, padRest, padStuck), to tune it on the hand.
static padguard::Guard padGuard;
static std::atomic<uint8_t> padL2At{0}, padR2At{0};
static std::atomic<uint8_t> padChord{0};   // bits on as a set-down began; 0: none since the last log
static std::atomic<uint8_t> padRest{0};    // 1 resting, 2 picked up; 0: no change since the last log
static std::atomic<uint32_t> padStuck{0};  // buttons the guard dropped as stuck since the last log
// A new connection's first report can carry junk (every face button down at once): the buttons
// count only from the first report with none past its press line (padguard::idle). handlePad
// disarms it on each connect.
static std::atomic<bool> padArmed{false};
// X / O pressed hard (their pressure) in a report without their on/off bit: the loop logs it, to
// tell a worn button from one the deck drops. 0: none since the last log.
static std::atomic<uint8_t> padXNoBit{0}, padONoBit{0};
// The right stick's flicks (util/padflick), the Bluetooth task's: -1 / +1 waiting for the loop
// (the last one wins), 0 none.
static padflick::Flick padFlick;
static std::atomic<int8_t> padFlickDir{0};

static void onPadReport() {
    using namespace padguard;
    const ps3_button_t& b = Ps3.data.button;
    const ps3_analog_button_t& a = Ps3.data.analog.button;
    padL2At.store(a.l2);
    padR2At.store(a.r2);
    if (!b.cross && a.cross > 60) padXNoBit.store(a.cross);
    if (!b.circle && a.circle > 60) padONoBit.store(a.circle);
    Report r{};
    r.bits = (b.select ? 1u << SELECT : 0) | (b.start ? 1u << START : 0) | (b.up ? 1u << UP : 0) |
             (b.right ? 1u << RIGHT : 0) | (b.down ? 1u << DOWN : 0) | (b.left ? 1u << LEFT : 0) |
             (b.l2 ? 1u << L2 : 0) | (b.r2 ? 1u << R2 : 0) | (b.l1 ? 1u << L1 : 0) | (b.r1 ? 1u << R1 : 0) |
             (b.triangle ? 1u << TRIANGLE : 0) | (b.circle ? 1u << CIRCLE : 0) | (b.cross ? 1u << CROSS : 0) |
             (b.square ? 1u << SQUARE : 0) | (b.ps ? 1u << HOME : 0);
    r.pressure[SELECT] = b.select ? 255 : 0;
    r.pressure[START] = b.start ? 255 : 0;
    r.pressure[HOME] = b.ps ? 255 : 0;
    r.pressure[UP] = a.up;
    r.pressure[RIGHT] = a.right;
    r.pressure[DOWN] = a.down;
    r.pressure[LEFT] = a.left;
    r.pressure[L2] = a.l2;
    r.pressure[R2] = a.r2;
    r.pressure[L1] = a.l1;
    r.pressure[R1] = a.r1;
    r.pressure[TRIANGLE] = a.triangle;
    r.pressure[CIRCLE] = a.circle;
    r.pressure[CROSS] = a.cross;
    r.pressure[SQUARE] = a.square;
    r.ax = Ps3.data.sensor.accelerometer.x;
    r.ay = Ps3.data.sensor.accelerometer.y;
    r.az = Ps3.data.sensor.accelerometer.z;
    r.now = millis();
    padReports.fetch_add(1);
    // the guard's last verdicts, to log each change once
    static bool chord = false, resting = false;
    static uint32_t stuck = 0;
    if (!padArmed.load()) {
        padGuard.reset();  // a new connection starts with nothing held and not resting
        padFlick.reset();
        chord = resting = false;
        stuck = 0;
        if (!idle(r)) return;  // the connect's junk, or a button held through it (a light touch is fine)
        padArmed.store(true);
    }
    const uint32_t held = padGuard.feed(r);
    const int flick = padFlick.feed(Ps3.data.analog.stick.rx);
    if (flick && !padGuard.paused(r.now)) padFlickDir.store(static_cast<int8_t>(flick));
    int on = 0;
    for (uint32_t v = r.bits; v; v &= v - 1) on++;
    if ((on >= CHORD) != chord) {
        chord = on >= CHORD;
        if (chord) padChord.store(static_cast<uint8_t>(on));
    }
    if (padGuard.resting() != resting) {
        resting = padGuard.resting();
        padRest.store(resting ? 1 : 2);
    }
    if (padGuard.stuck() & ~stuck) padStuck.fetch_or(padGuard.stuck() & ~stuck);
    stuck = padGuard.stuck();
    padHeld.store(held);
    padSeen.fetch_or(held);
}

static void handlePad() {
    const bool is = Ps3.isConnected();
    if (is != padUp) {
        padUp = is;
        Serial.printf("[pad] %s\n", is ? "connected" : "gone");
        setFocus(is ? homeFocus() : -1);
        padArmed.store(false);  // each connection's buttons count from its first clear report
        if (!is) {
            padBattery = padbattery::Look{};  // the next controller starts unknown
            padHeld.store(0);  // nothing held while it's gone
            padSeen.store(0);
        }
        padLeds = -1;  // a new connection shows the links (and stops the four LEDs blinking)
        padBuzz = padfeel::Buzz{};
        if (screen == Screen::Menu) drawPadBattery();
    }
    if (is) {
        padBatteryTick();
        padFeelTick();
    }
    if (padFlash >= 0 && millis() - padFlashAt > PAD_FLASH_MS) {
        drawButton(padFlash);
        padFlash = -1;
    }
    if (!is) return;

    // every 10 s: how often the controller reports (a quick tap is one report)
    static uint32_t rateAt = 0, rateFrom = 0;
    if (millis() - rateAt >= 10000) {
        const uint32_t now = padReports.load();
        if (rateAt) Serial.printf("[pad] %u reports/s\n", static_cast<unsigned>((now - rateFrom) * 1000 / (millis() - rateAt)));
        rateAt = millis();
        rateFrom = now;
    }
    if (const uint8_t p = padXNoBit.exchange(0)) Serial.printf("[pad] X pressed to %u without its bit\n", p);
    if (const uint8_t p = padONoBit.exchange(0)) Serial.printf("[pad] O pressed to %u without its bit\n", p);
    if (const uint8_t k = padChord.exchange(0)) Serial.printf("[pad] ignored: %u buttons at once\n", k);
    if (const uint8_t k = padRest.exchange(0)) Serial.println(k == 1 ? "[pad] resting" : "[pad] picked up");
    if (const uint32_t k = padStuck.exchange(0)) {
        static const char* const NAMES[padguard::N_BITS] = {"SELECT", "L3", "R3", "START", "up", "right", "down", "left", "L2",
                                                            "R2", "L1", "R1", "triangle", "circle", "cross", "square", "PS"};
        for (int i = 0; i < padguard::N_BITS; i++)
            if (k & 1u << i) Serial.printf("[pad] stuck: %s\n", NAMES[i]);
    }
    // the held buttons, and any tapped since the last pass (onPadReport)
    const uint32_t m = padHeld.load() | padSeen.exchange(0);
    struct {
        bool select, start, up, right, down, left, l2, r2, l1, r1, triangle, circle, cross, square, ps;
    } n = {(m & 1u << 0) != 0,  (m & 1u << 3) != 0,  (m & 1u << 4) != 0,  (m & 1u << 5) != 0,
           (m & 1u << 6) != 0,  (m & 1u << 7) != 0,  (m & 1u << 8) != 0,  (m & 1u << 9) != 0,
           (m & 1u << 10) != 0, (m & 1u << 11) != 0, (m & 1u << 12) != 0, (m & 1u << 13) != 0,
           (m & 1u << 14) != 0, (m & 1u << 15) != 0, (m & 1u << 16) != 0};
    // D-pad only: the sticks drift, which would wander the ring on its own.
    int dx = (n.right ? 1 : 0) - (n.left ? 1 : 0);
    int dy = (n.down ? 1 : 0) - (n.up ? 1 : 0);
    if (dx && dy) dy = 0;  // diagonals: horizontal wins
    static int lastDx = 0, lastDy = 0;
    static uint32_t repeatAt = 0;
    if (dx || dy) {
        if (dx != lastDx || dy != lastDy) {
            Serial.printf("[pad] d-pad %d,%d\n", dx, dy);
            moveFocus(dx, dy);
            repeatAt = millis() + 400;
        } else if (static_cast<int32_t>(millis() - repeatAt) >= 0) {
            Serial.printf("[pad] d-pad repeat %d,%d\n", dx, dy);
            moveFocus(dx, dy);
            repeatAt = millis() + 150;
        }
    }
    lastDx = dx;
    lastDy = dy;

    // buttons act on the press, once; every edge is taken on both screens, so a button held across
    // a screen change doesn't fire on the next one
    static bool pA = false, pB = false, pX = false, pY = false, pLB = false, pRB = false;
    static bool pSt = false, pSe = false, pL2 = false, pR2 = false, pPs = false;
    auto edge = [](bool now, bool& prev) {
        const bool down = now && !prev;
        prev = now;
        return down;
    };
    const bool a = edge(n.cross, pA), b = edge(n.circle, pB), x = edge(n.square, pX), y = edge(n.triangle, pY);
    const bool lb = edge(n.l1, pLB), rb = edge(n.r1, pRB);
    const bool start = edge(n.start, pSt), select = edge(n.select, pSe), l2 = edge(n.l2, pL2), r2 = edge(n.r2, pR2);
    const bool home = edge(n.ps, pPs);
    // every press in the log, to tell a real press from the deck acting by itself
    if (a || b || x || y || lb || rb || start || select || l2 || r2 || home) {
        crashlog::at("pad press");
        Serial.printf("[pad] press%s%s%s%s%s%s%s%s%s%s%s\n", a ? " cross" : "", b ? " circle" : "", x ? " square" : "",
                      y ? " triangle" : "", lb ? " L1" : "", rb ? " R1" : "", start ? " START" : "",
                      select ? " SELECT" : "", l2 ? " L2" : "", r2 ? " R2" : "", home ? " PS" : "");
        if (l2 || r2) Serial.printf("[pad] trigger pressure L2 %u, R2 %u\n", padL2At.load(), padR2At.load());
    }

    // the shortcuts, on any screen; one per tick, before the screen's own buttons (a flick doesn't
    // take the tick: a press in the same pass still acts)
    if (const int8_t d = padFlickDir.exchange(0)) padColour(d);
    if (home) {
        if (screen != Screen::Menu) show(Screen::Menu);
        return;
    }
    if (start) {
        padLightsPower();
        return;
    }
    if (l2 || r2) {
        padPreset(r2 ? 1 : -1);
        return;
    }
    if (select) {
        setMusic(!musicOn);
        drawMusicItem();
        padFelt(padfeel::Feel::Double, musicOn, false, false);
        return;
    }
    if (y) {
        padKaraoke();
        return;
    }
    if (lb || rb) {
        padLevel(rb ? 1 : -1);
        return;
    }

    const Screen was = screen;
    if (a) padPress(focus);
    if (screen != was) return;  // A changed the screen: the other buttons wait for their next press
    if (b && screen != Screen::Menu) {
        show(Screen::Menu);
        return;
    }
    if (!onLights()) return;  // the rest is the Lights screen's
    if (x) padPress(L_TAB0 + (currentTab() + 1) % deckmenu::N_TABS);
}

// ---- The black deck: its address, signed asks and the keep-alives ----
// Its own task (core 1, idle priority; core 0 is the lights' and the controller's link), started once
// WiFi is up: it keeps music mode and karaoke alive on the black deck (keepAlive, below) and
// otherwise sleeps, waking every 100 ms. The black deck is at its fixed
// hotspot address (BLACK_DECK_ADDR) on the iPhone hotspot, else looked up by its mDNS name
// (DECK_MDNS_NAME) and kept through blips (util/deckaddr). HTTP/1.0, 1.5 s timeouts.
static TaskHandle_t deckHandle = nullptr;
static deckaddr::Keep deckAddr;  // under deckLock (see deckGet)
// A failed mDNS lookup holds the asker for up to 1.5 s: while the deck stays away, one every
// LOOKUP_GAP_MS at most.
static constexpr uint32_t LOOKUP_GAP_MS = 10000;
static uint32_t lookedUpAt = 0;  // under deckLock
static bool lookedUp = false;

// The deck's mDNS (its name, and the lookups of the other decks), started by the first task that
// looks a deck up (a black-deck asker, under deckLock), so under its own lock. True once up.
static SemaphoreHandle_t mdnsLock = nullptr;  // made in beginVoice, before any task that looks up
static bool mdnsUp() {
    static bool up = false;
    xSemaphoreTake(mdnsLock, portMAX_DELAY);
    if (!up) up = MDNS.begin(LEDS_MDNS_NAME);
    xSemaphoreGive(mdnsLock);
    return up;
}

// Where the black deck is: its fixed address on the hotspot (until it stops answering as the deck),
// else its mDNS name on whatever network both are on. False when it can't be found.
static bool deckHost(char* out, size_t n) {
    const bool hot =
        hotspotip::onHotspot(hostOrder(WiFi.localIP()), hostOrder(WiFi.gatewayIP()), hostOrder(WiFi.subnetMask()));
    if (hot && deckAddr.addr() == 0 && deckAddr.tryFixed()) {
        snprintf(out, n, "%s", BLACK_DECK_ADDR);
        return true;
    }
    mdnsUp();
    if (deckAddr.addr() == 0) {
        const uint32_t now = millis();
        if (lookedUp && now - lookedUpAt < LOOKUP_GAP_MS) return false;
        lookedUp = true;
        lookedUpAt = now;
        const IPAddress a = MDNS.queryHost(DECK_MDNS_NAME, 1500);
        if (static_cast<uint32_t>(a) == 0) {
            deckAddr.lookupFailed();  // on the hotspot: the fixed address again
            return false;
        }
        deckAddr.found(static_cast<uint32_t>(a));
        Serial.printf("[karaoke] deck found at %s (mDNS%s)\n", a.toString().c_str(), hot ? ", off its fixed address" : "");
    }
    snprintf(out, n, "%s", IPAddress(deckAddr.addr()).toString().c_str());
    return true;
}

// What deckGet returns when it sent nothing: the clock isn't set (or HMAC's self-test failed), so
// nothing can be signed (the bearer key is never sent). Not a miss: the deck wasn't asked.
static constexpr int NOT_SIGNED = -1000;

// Only the deck task asks the black deck (/music, /karaoke), under deckLock all the same: the address
// keeping (deckHost, deckAddr) stays one asker's at a time if another comes. The longest hold is an
// mDNS query and a GET (1.5 s, then 1.5 s and the body's 3 s): DECK_LOCK_MS waits that out, then
// counts as no answer.
static SemaphoreHandle_t deckLock = nullptr;  // beginVoice makes it, before any asker starts
static constexpr uint32_t DECK_LOCK_MS = 7000;

// A signed GET of `path` with `params` (short fixed words, sent as they are) to the black deck, the
// body into `body` when there is one; the HTTP status, negative without an answer, or NOT_SIGNED.
// peer: the address asked; nonce: the request's n (16 hex). Short timeouts: the deck is one hop
// away, and a missing deck must not hold the polls up. Under deckLock (deckGet).
static int deckAsk(const char* path, const sigreq::Param* params, int count, char* body, size_t bodyLen,
                   uint32_t* peer, char* nonce) {
    if (!net::isOnline()) return -1;
    char auth[sigreq::AUTH_LEN];
    if (!reqauth::signGet(path, params, count, auth, sizeof(auth))) {
        static bool told = false;
        if (!told) {
            Serial.println(reqauth::hmacOk() ? "[karaoke] waiting for the clock: requests to the deck are signed"
                                             : "[karaoke] HMAC self-test failed: nothing is sent to the deck");
        }
        told = true;
        return NOT_SIGNED;
    }
    if (nonce) {  // auth is "t=..&n=<16 hex>&sig=.."
        const char* at = strstr(auth, "&n=");
        snprintf(nonce, 17, "%.16s", at ? at + 3 : "");
    }
    char host[40], query[32] = "", url[160];
    if (!deckHost(host, sizeof(host))) return -1;
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

static int deckGet(const char* path, const sigreq::Param* params, int count, char* body, size_t bodyLen,
                   uint32_t* peer = nullptr, char* nonce = nullptr) {
    if (!deckLock || xSemaphoreTake(deckLock, pdMS_TO_TICKS(DECK_LOCK_MS)) != pdTRUE) return -1;
    const int rc = deckAsk(path, params, count, body, bodyLen, peer, nonce);
    xSemaphoreGive(deckLock);
    return rc;
}

// An asker's verdict on the address it asked (deckAddr: 3 misses in a row, look it up again),
// under deckLock like the rest of the address keeping.
static void deckAnswered(bool valid) {
    if (!deckLock || xSemaphoreTake(deckLock, pdMS_TO_TICKS(DECK_LOCK_MS)) != pdTRUE) return;
    deckAddr.onPoll(valid);
    xSemaphoreGive(deckLock);
}

// ---- Music mode: the black deck sends the song's palette ----
// From the deck task: while musicWant, GET /music?on=1 at once and every MUSIC_KEEP_MS (the black
// deck stops after 30 s without one), MUSIC_RETRY_MS after a miss (no answer, "stopping, ask again"),
// but MUSIC_KEEP_MS after a 503 (its low memory: Spotify's TLS needs that heap, so no hurry); on=0 once
// when it goes. Each ask also records the black deck's address (musicPeer), the one sender the music
// task takes packets from, and counts toward deckAddr's misses (a stale mDNS
// address is looked up again). An on=1 answered 200 also proves the black deck's boot id for this
// request's nonce (util/wire/musicwire::readBoot): the music task takes packets only from that boot
// (musicBoot). The log tells each change of answer.
static constexpr uint32_t MUSIC_KEEP_MS = 10000, MUSIC_RETRY_MS = 2000;
static volatile uint32_t musicPeer = 0;  // IPv4, network order (IPAddress's dword); 0: not asked yet
static volatile uint32_t musicBoot = 0;  // the black deck's proven boot id; 0: none yet

static int musicAsk(bool on) {
    crashlog::at("music ask");
    const sigreq::Param p[] = {{"on", on ? "1" : "0"}};
    uint32_t peer = 0;
    char reply[96], nonce[17] = "";
    const int rc = deckGet("/music", p, 1, reply, sizeof(reply), &peer, nonce);  // the status is the answer
    if (peer) musicPeer = peer;
    if (on && rc == 200) {
        uint32_t boot = 0;
        if (!musicwire::readBoot(reply, REMOTE_TOKEN, nonce, &boot)) {
            Serial.println("[music] the deck's reply has no valid boot proof: its packets are ignored");
        } else if (boot != musicBoot) {
            musicBoot = boot;
            Serial.println("[music] the deck's boot id is proven: taking its packets");
        }
    }
    return rc;
}

static int musicGet(bool on, int) {
    const int rc = musicAsk(on);
    // the deck's own answers (200, "stopping" 409, low memory 503); 3 misses in a row: look it up again
    if (rc != NOT_SIGNED) deckAnswered(rc == 200 || rc == 409 || rc == 503);
    return rc;
}

// A keep-alive's state (music mode's, karaoke's): while wanted, `get(true)` at once and every
// MUSIC_KEEP_MS (MUSIC_RETRY_MS after a miss), and at once when the light it carries changed
// (util/yellow/keepalive); `get(false)` once when it goes. The log tells each change of answer.
struct KeepAlive {
    const char* tag;  // the log's
    int (*get)(bool on, int light);
    int (*light)();   // what on=1 carries; nullptr: nothing (-1)
    keepalive::State st;
    int lastRc = 0;
};

// The on=1's answer, 0 when none went out.
static int keepAlive(KeepAlive& k, bool want) {
    if (ota::active()) return 0;
    const int light = k.light ? k.light() : -1;
    switch (keepalive::due(k.st, want, millis(), light)) {
    case keepalive::Ask::None: return 0;
    case keepalive::Ask::Off:
        k.st.asked = false;
        k.lastRc = 0;
        Serial.printf("[%s] off (rc=%d)\n", k.tag, k.get(false, -1));
        return 0;
    case keepalive::Ask::On: break;
    }
    const int rc = k.get(true, light);
    if (rc != k.lastRc) Serial.printf("[%s] on=1: rc=%d\n", k.tag, rc);
    k.lastRc = rc;
    k.st.asked = true;
    k.st.sentLight = light;
    k.st.nextAt = millis() + (rc == 200 || rc == 503 ? MUSIC_KEEP_MS : MUSIC_RETRY_MS);
    return rc;
}

// The INSIDE light's swatch for the black deck's karaoke colour, the one this deck's theme takes
// (wantTheme): what it shows (music mode's look while that drives it), or -1 while it is off. Read
// from the deck task without a lock: a look read mid-change is put right on the next pass, 100 ms later.
static int insideLight() {
    const Look l = shownLook(0);
    return l.lit ? l.swatch : -1;
}

// Karaoke: GET /karaoke?on=1&light=N at music mode's pace (the black deck goes back to its normal
// screen 30 s without one) and on a change of light, on=0 once when it goes; each ask counts toward
// deckAddr's misses.
static int karaokeGet(bool on, int light) {
    crashlog::at("karaoke ask");
    char lightText[8];
    snprintf(lightText, sizeof(lightText), "%d", light);
    const sigreq::Param p[] = {{"on", on ? "1" : "0"}, {"light", lightText}};
    char reply[32];
    const int rc = deckGet("/karaoke", p, on ? 2 : 1, reply, sizeof(reply));
    if (rc != NOT_SIGNED) deckAnswered(rc == 200);
    return rc;
}

static void deckTask(void*) {
    static KeepAlive music{"music", musicGet, nullptr, {}, 0}, karaoke{"karaoke", karaokeGet, insideLight, {}, 0};
    watchdog::watch();  // each pass: two GETs of 1.5 s and an mDNS query of 1.5 s at most
    for (;;) {
        watchdog::feed();
        keepAlive(music, musicWant);
        // want before taps: setKaraoke counts the tap first, so a switch on seen here is counted
        static keepalive::Confirm confirm;
        static uint32_t seenTaps = 0;
        const bool want = karaokeWant;
        const uint32_t taps = karaokeTaps;
        if (taps != seenTaps) {
            seenTaps = taps;
            keepalive::start(confirm);
        }
        if (!want) confirm.waiting = false;
        const keepalive::Shown shown = keepalive::answer(confirm, keepAlive(karaoke, want));
        if (shown != keepalive::Shown::None) karaokeShown = static_cast<uint8_t>(shown);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// From the voice task, once WiFi is up. 6 KB: the GETs and the mDNS query; the [mem] line shows
// what's left.
static void beginDeck() {
    if (deckHandle) return;
    xTaskCreatePinnedToCore(deckTask, "deck", 6144, nullptr, 0, &deckHandle, 1);
}

// ---- Music mode: the song's colour on both lights ----
// Its own task (core 1, idle priority, 4 KB), started the first time music mode goes on. While
// musicWant and online it holds a UDP socket on musicwire::PORT (raw lwip, 200 ms receive timeout),
// closed again when music mode goes off or an update has the WiFi. It takes packets only from the
// black deck's address (musicPeer), signed with REMOTE_TOKEN and with a counter above the last one
// (util/wire/musicwire), and publishes the latest PalPkt and its time under musicMux.
// musicTick() (loop) reads them at most MUSIC_TICK_MS apart: the palette's main colour snapped to a
// swatch (musiclights::nearestSwatch) and the inside light's level (musiclights::level: the owner's under
// the palette's cap, a 40 % floor) are the lights' music looks; a new one fades in (fadeTick).
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
        if (!fresh.accept(seq, musicBoot)) continue;  // only the boot the deck proved, counter rising
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
static constexpr int MUSIC_FADE_IN = 6, MUSIC_FADE_OUT = 3;
static constexpr uint32_t MUSIC_FADE_MS = 3000;

// Each loop pass: the fades' due steps. A light out of reach skips its writes (it gets its look
// when it comes back: loop()'s fresh push).
static void fadeTick() {
    const uint32_t now = millis();
    for (int i = 0; i < N_CTRL; i++) {
        LightFade& f = lightFade[i];
        if (f.done >= f.steps) continue;
        if (f.intro.n) {  // the welcome: its next key at its time
            if (now - f.start < f.intro.k[f.done].at) continue;
            f.at = now;
            writeKey(i, f.done);
            f.done++;
            continue;
        }
        if (now - f.at < f.every) continue;
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
        t.pct = static_cast<uint8_t>(i == 0 ? musiclights::level(own.pct, cap) : musiclights::OUTSIDE_PCT);
        if (musicDriving && t == musicLook[i]) continue;
        any = true;
        const Look was = shownLook(i);
        const songpalette::Rgb from = frameNow(i);
        musicLook[i] = t;
        LightFade& f = lightFade[i];
        if (!t.lit) {  // the outside light left off: stays off, nothing sent
            f.done = f.steps;
            continue;
        }
        const int steps = i == 0 ? MUSIC_FADE_IN : MUSIC_FADE_OUT;
        startFade(i, was.lit ? from : songpalette::Rgb{0, 0, 0}, frameOf(t), !was.lit, steps, MUSIC_FADE_MS / static_cast<uint32_t>(steps));
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
        for (LightFade& f : lightFade) f = LightFade();
        if (drove && restore) {
            for (int i = 0; i < N_CTRL; i++) push(i);
            drawLooks();  // the lights band and the theme
        }
    }
    setMessage(on ? ">MUSIC ON" : ">MUSIC OFF");
    drawMessage();
}

// The Menu's MUSIC MODE item again (its ON / OFF tag), after a change that didn't come from a tap on it.
static void drawMusicItem() {
    if (screen != Screen::Menu) return;
    const int item = M_ITEM0 + deckmenu::MUSIC;
    drawButton(item, pressedNow(item));
}

// Karaoke on or off (the KARAOKE item, voice): the deck task follows karaokeWant (on: GET /karaoke?on=1
// at once and every KARAOKE_KEEP_MS, off: on=0 once). Off shows ">KARAOKE OFF"; on shows
// ">KARAOKE: CONNECTING..." until the black deck answers (karaokeAnswerTick). The black deck picks
// its screen per song. The caller redraws the item.
static void setKaraoke(bool on) {
    karaokeOn = on;
    if (on) karaokeTaps = karaokeTaps + 1;
    karaokeWant = on;
    Serial.printf("[karaoke] %s\n", on ? "on" : "off");
    if (on) setMessage(">KARAOKE: CONNECTING...", KARAOKE_WAIT_MS);
    else setMessage(">KARAOKE OFF");
    drawMessage();
}

// What the deck task heard after a switch on (karaokeShown): ">KARAOKE ON" at the black deck's first
// answer, ">BLACK DECK NOT ANSWERING" (ALERT) after keepalive::NO_ANSWER_MISSES misses; it keeps
// asking, and a later answer still says ON. From loop().
static void karaokeAnswerTick() {
    const uint8_t s = karaokeShown;
    if (!s) return;
    karaokeShown = 0;
    if (!karaokeOn) return;
    if (s == static_cast<uint8_t>(keepalive::Shown::Connected)) setMessage(">KARAOKE ON");
    else setMessage(">BLACK DECK NOT ANSWERING", KARAOKE_WAIT_MS, true);
    drawMessage();
}

// The Menu's KARAOKE item again (its ON / OFF tag), after a change that didn't come from a tap on it.
static void drawKaraokeItem() {
    if (screen != Screen::Menu) return;
    const int item = M_ITEM0 + deckmenu::KARAOKE;
    drawButton(item, pressedNow(item));
}

// A manual change (a tap, the PS3 controller, voice, the watch) while music mode is on: the mode
// ends first (the MUSIC item shows OFF), then the change applies to the pages' looks. True when the
// lights showed the song: the caller pushes both lights (the one it doesn't change goes back too).
static bool musicYield() {
    if (!musicOn) return false;
    const bool drove = musicDriving;
    Serial.println("[music] a manual change ends it");
    setMusic(false, false);
    drawMusicItem();
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

// ---- Voice: Siri Shortcuts over WiFi ----
// On the iPhone hotspot the deck sits at 172.20.10.11 (net/wifi pins it). A Shortcut dictates a
// phrase and calls  GET /say?text=<phrase>&k=<REMOTE_TOKEN>; the reply is one line to speak back
// ("Purple, 70 percent"). Without a zone it acts like the BOTH page (Siri: spoken zone words are
// ignored); &zone=inside|outside (the Garmin app) acts on that zone's page only.
// GET /state?k=[&zone=] answers that zone's line (level included) without changing anything.
// A preset ("rose", "preset 2") is applied to both lights and answered with its line; the rest of
// that phrase (a colour, a level, the zone) is ignored. "music" / "music mode" turns music mode on,
// "music off" off, like the MUSIC MODE button ("Music mode on"). GET /presets?k= lists the six
// presets (one ledpresets::wireLine each), GET /status?k= each light's state and music mode's.
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
    if (c.karaoke >= 0) {  // "karaoke", "karaoke off": the KARAOKE item's switch
        if ((c.karaoke == 1) != karaokeOn) {
            setKaraoke(c.karaoke == 1);
            drawKaraokeItem();
        }
        snprintf(line, sizeof(line), "Karaoke %s", karaokeOn ? "on" : "off");
        answerVoice(line, job.seq);
        return;
    }
    if (c.music >= 0) {  // "music", "music off": the MUSIC MODE button's toggle, both lights
        if ((c.music == 1) != musicOn) {
            setMusic(c.music == 1);
            drawMusicItem();
        }
        snprintf(line, sizeof(line), "Music mode %s", musicOn ? "on" : "off");
        noteUnlinked(line, sizeof(line), BOTH);
        answerVoice(line, job.seq);
        return;
    }
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
        char l[48];  // "1 ROSE pink 40 pink 100" is 23
        ledpresets::wireLine(i + 1, l, sizeof(l));
        const int k = snprintf(body + len, sizeof(body) - len, "%s%s", i ? "\n" : "", l);
        if (k > 0) len += static_cast<size_t>(k);
    }
    server->send(200, "text/plain", body);
}

// Each light on, off or out of reach, then music mode and the firmware:
// "inside=on outside=nolink music=off fw=2.0.0".
static void handleStatus() {
    if (!reqauth::pass(*server, gate, "/status", "Wrong key")) return;
    const Published p = snapshot();
    char line[96];
    lightstatus::reply(lightstatus::of(p.up[0], p.looks[INSIDE].lit), lightstatus::of(p.up[1], p.looks[OUTSIDE].lit),
                       musicWant, FW_VERSION, line, sizeof(line));
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
    netclock::begin();  // SNTP, for the Menu's clock and night dimming
    beginDeck();
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void beginVoice() {
    if (!gate.enabled()) {
        Serial.println("[voice] off (REMOTE_TOKEN empty)");
        return;
    }
    reqauth::begin();  // HMAC's self-test: the watch's requests and ours to the black deck are signed
    mdnsLock = xSemaphoreCreateMutex();
    deckLock = xSemaphoreCreateMutex();  // before the voice task, which starts every asker
    voiceQueue = xQueueCreate(1, sizeof(VoiceJob));
    voiceDone = xSemaphoreCreateBinary();
    server = new WebServer(80);
    server->on("/say", HTTP_ANY, handleSay);
    server->on("/state", HTTP_GET, handleState);
    server->on("/presets", HTTP_GET, handlePresets);
    server->on("/status", HTTP_GET, handleStatus);
    // 4 KB: the web server and the joins (6 KB left 4.5 KB unused on the desk); the [mem] line shows what's left.
    xTaskCreatePinnedToCore(voiceTask, "voice", 4096, nullptr, 0, &voiceHandle, 1);
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

static void repaint();

// The Menu's clock and night. Polled each second (a time() call) so the clock turns over on the
// minute; night follows it: the backlight, and a repaint.
static void updateClock() {
    static uint32_t checkedAt = 0;
    static bool first = true;
    if (!first && millis() - checkedAt < 1000) return;
    first = false;
    checkedAt = millis();
    int m = 0, y = 0, month = 0, day = 0;
    const bool synced = netclock::localNow(&m, &y, &month, &day);
    const int minute = synced ? m : -1;
    if (minute != clockMin || day != clockDay) {
        if (minute >= 0 && clockMin < 0) Serial.printf("[clock] synced, local %02d:%02d\n", m / 60, m % 60);
        clockMin = minute;
        clockY = y;
        clockMonth = month;
        clockDay = day;
        if (screen == Screen::Menu) drawClock();
    }
    bool want = synced && daynight::isNight(m);
#ifdef NIGHT_TEST
    want = tuneNight;
#endif
    if (want == night) return;
    night = want;
    backlight::setLevel(night ? nightBacklight : 255);
    Serial.printf("[ui] night %s\n", night ? "on" : "off");
    repaint();
}

// The theme the INSIDE look asks for: its colour while it's on, linked or not, else -1.
static int wantTheme() {
#ifdef NIGHT_TEST
    if (tuneTheme > -2) return tuneTheme;
#endif
#ifdef THEME_TEST
    return THEME_TEST;
#else
    const Look l = shownLook(0);  // in music mode the song's swatch
    return l.lit ? ledlook::gridSwatch(l.swatch) : -1;  // cyan: blue's
#endif
}

// Called before anything draws a changed look (drawLooks, a preset, voice),
// and once more at the end of each loop as a backstop: a new theme repaints the current screen (the
// backlight stays), with a button still under a finger or a controller press drawn pressed again.
// The early return keeps the repaint (which may come back here) from recursing. True when it
// repainted.
static bool updateTheme() {
    const int want = wantTheme();
    if (want == theme) return false;
    theme = want;
    Serial.printf("[ui] theme %d\n", theme);
    repaint();
    return true;
}

// The current screen drawn again, with a button still under a finger or a controller press drawn
// pressed again.
static void repaint() {
    drawAll();
    if (tap.down && tap.id >= 0) drawButton(tap.id, true);
    if (padFlash >= 0) drawButton(padFlash, true);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("[boot] ledpanel v%s\n", FW_VERSION);
    crashlog::begin();  // why the last run ended, and the runs before it (common/core/crashlog)
    ota::checkBoot();  // may boot the other app slot (net/ota), before anything else starts
    // The task watchdog at 90 s (common/core/watchdog), before any task subscribes: the link,
    // voice, deck and music tasks, and this loop from the end of setup().
    watchdog::begin();
    tft.init();
    backlight::begin();  // PWM, full until updateClock says night
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
    theme = wantTheme();  // after the typing (always green): the saved inside look's colour, no link needed
    drawAll();
    // Bluedroid in dual mode: the PS3 library starts the controller and the stack (Classic for the
    // pad), then the lights' GATT client registers on the same stack (yellow/elklink).
    if (!Ps3.begin()) Serial.println("[pad] Bluetooth didn't start");
    irrx::begin(PIN_IR);
    Ps3.attach(onPadReport);  // each report: the held and tapped buttons, L2 / R2 by pressure
    // The radio's time to Bluetooth first: on the default balance the DS3 got ~20 reports a second
    // of its ~100 next to WiFi and the lights, so a quick X or O fell between two that arrived.
    // WiFi carries only the black deck's small polls here.
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
    Serial.printf("[pad] deck address %s (the PS3 controller's master)\n", Ps3.getAddress().c_str());
    elklink::begin();
    for (const Controller& c : controllers) {
        elklink::setAddress(indexOf(c), c.addr);
        if (strcmp(c.addr, NO_MAC) == 0) {
            Serial.printf("[leds] %s: no address in config.h (LED_%s_MAC)\n", c.label, c.label);
        }
    }
    // 5 KB: the lights' connects only wait on the stack (yellow/elklink; 8 KB left 6.4 KB unused on
    // the desk); the [mem] line shows what's left.
    xTaskCreatePinnedToCore(linkTask, "leds-link", 5120, nullptr, 1, &linkHandle, 0);
    publish();
    beginVoice();
    enableLoopWDT();  // the core feeds it before each loop(); BOOT's calibration feeds while it waits
}

#ifdef NIGHT_TEST
// The night tuner, a line on the serial port (tools/deck_shot.py's port, 115200), live, no rebuild:
//   N <theme -1..7> <TEXT> <MID> <DIM> [ALERT]  that theme's night colours (hex RRGGBB); shows it
//   T <theme -1..7>   show that theme (T alone: the next one; T X back to the inside light's)
//   B <0..255>        the night backlight
//   D                 day / night, to compare
//   P                 print every theme's night colours and the backlight
static void tuneLine(const char* s) {
    char cmd = 0;
    int t = 0;
    unsigned v[4] = {0, 0, 0, 0};
    const int n = sscanf(s, " %c %d %x %x %x %x", &cmd, &t, &v[0], &v[1], &v[2], &v[3]);
    if (n < 1) return;
    if (cmd == 'N' && n >= 5 && t >= -1 && t < 8) {
        tunedRgb(termpal::BG);  // filled from termpal first
        for (int k = 0; k < n - 2; k++) tuned[t + 1][termpal::TEXT + k] = v[k] & 0xFFFFFF;
        tuneTheme = t;
        Serial.printf("[tune] theme %d: %06X %06X %06X %06X\n", t, tuned[t + 1][termpal::TEXT], tuned[t + 1][termpal::MID],
                      tuned[t + 1][termpal::DIM], tuned[t + 1][termpal::ALERT]);
        theme = -3;  // forces updateTheme's repaint, also when the theme is already shown
    } else if (cmd == 'T') {
        if (strchr(s, 'X')) tuneTheme = -2;
        else if (n >= 2 && t >= -1 && t < 8) tuneTheme = t;
        else tuneTheme = theme >= 7 ? -1 : theme + 1;
        Serial.printf("[tune] theme %d\n", tuneTheme);
    } else if (cmd == 'B' && n >= 2) {
        nightBacklight = t < 0 ? 0 : t > 255 ? 255 : t;
        if (night) backlight::setLevel(nightBacklight);
        Serial.printf("[tune] backlight %d\n", nightBacklight);
    } else if (cmd == 'D') {
        tuneNight = !tuneNight;
    } else if (cmd == 'P') {
        tunedRgb(termpal::BG);
        for (int k = -1; k < 8; k++) {
            Serial.printf("[tune] N %d %06X %06X %06X %06X\n", k, tuned[k + 1][termpal::TEXT], tuned[k + 1][termpal::MID],
                          tuned[k + 1][termpal::DIM], tuned[k + 1][termpal::ALERT]);
        }
        Serial.printf("[tune] B %d\n", nightBacklight);
    }
}
#endif

// One panel row to the PC: "R<yyy> <hex RGB565>".
static void shotRow(int y) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    static uint8_t rgb[320 * 3 + 1];  // this panel's read comes one byte late: rgb[0] is the dummy's
    static char row[6 + 320 * 4 + 2];
    tft.readRectRGB(0, y, 320, 1, rgb);
    int n = snprintf(row, sizeof row, "R%03d ", y);
    for (int x = 0; x < 320; x++) {
        const uint8_t* p = rgb + x * 3 + 1;
        const unsigned v = ((p[0] & 0xF8U) << 8) | ((p[1] & 0xFCU) << 3) | (p[2] >> 3);
        for (int s = 12; s >= 0; s -= 4) row[n++] = HEX_DIGITS[(v >> s) & 15U];
    }
    row[n++] = '\n';
    Serial.write(reinterpret_cast<const uint8_t*>(row), static_cast<size_t>(n));
}

// "INTRO\n" on the serial port plays the welcome again on each lit light (music mode skips it, as at
// the first link), with no reboot and no car: unlinked lights log their keys as "no link"
// (tools/intro_check.py checks them). ">WELCOME" shows on the prompt line.
static void replayIntro() {
    Serial.println("[intro] replay");
    bool any = false;
    for (int i = 0; i < N_CTRL; i++) {
        if (shownLook(i).lit && !musicDriving) {
            startIntro(i);
            any = true;
        } else {
            Serial.printf("[intro] %s: skipped (%s)\n", controllers[i].label, musicDriving ? "music mode" : "off");
        }
    }
    if (!any) return;
    setMessage(">WELCOME");
    drawMessage();
}

// "SHOT\n" on the serial port sends the panel to the PC (tools/deck_shot.py): "SHOT 320 240", a row
// line each, "SHOT END". Bluedroid's own error prints skip the serial lock and can land inside a row, so
// the PC then asks again for the rows it lost ("R031\n", 2 s of silence or "Q" ends it). About 30 s at
// 115200, in the loop: the screen holds still, touch and drawing wait (the 90 s watchdog doesn't
// mind), the lights' link and the servers keep running. "INTRO\n" is read here too (replayIntro).
static void shotTick() {
    static const char WORD[] = "SHOT\n", INTRO[] = "INTRO\n";
    static uint8_t matched = 0, introMatched = 0;
    bool go = false, intro = false;
#ifdef NIGHT_TEST
    static char line[64];
    static uint8_t len = 0;
#endif
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
#ifdef NIGHT_TEST
        if (c == '\n') {
            line[len] = 0;
            len = 0;
            tuneLine(line);
        } else if (len < sizeof line - 1) {
            line[len++] = c;
        }
#endif
        matched = c == WORD[matched] ? matched + 1 : (c == WORD[0] ? 1 : 0);
        if (WORD[matched] == 0) {
            matched = 0;
            go = true;
        }
        introMatched = c == INTRO[introMatched] ? introMatched + 1 : (c == INTRO[0] ? 1 : 0);
        if (INTRO[introMatched] == 0) {
            introMatched = 0;
            intro = true;
        }
    }
    if (intro) replayIntro();
    if (!go) return;
    Serial.println("SHOT 320 240");
    for (int y = 0; y < 240; y++) shotRow(y);
    Serial.println("SHOT END");
    char ask[5];
    uint8_t n = 0;
    for (uint32_t until = millis() + 2000; static_cast<int32_t>(until - millis()) > 0;) {
        if (!Serial.available()) {
            delay(5);
            continue;
        }
        const char c = static_cast<char>(Serial.read());
        if (c == 'Q') break;
        if (c == '\r') continue;
        if (c != '\n') {
            if (n < sizeof ask - 1) ask[n++] = c;
            continue;
        }
        ask[n] = 0;
        n = 0;
        if (ask[0] == 'R' && atoi(ask + 1) < 240) shotRow(atoi(ask + 1));
        until = millis() + 2000;
    }
}

void loop() {
    shotTick();
    // Up bootguard::HEALTHY_MS and online at that moment (ota::onGoodPoll checks the time; with
    // NO_WIFI_UPDATES there's no listener to wait for): this firmware has proven itself, and the
    // crash count clears (net/ota's boot guard). Checked once a second.
    static uint32_t provenAt = 0;
    if (millis() - provenAt >= 1000) {
        provenAt = millis();
        if (net::isOnline()) ota::onGoodPoll();
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

    // every 10 s: free heap, its largest block, failed allocations and the least stack each task
    // has had to spare; crashlog keeps the last second's heap for after a restart
    crashlog::tick();
    static uint32_t memAt = 0;
    if (millis() - memAt > 10000) {
        memAt = millis();
        Serial.printf("[mem] heap %u (min %u, block %u, fails %u)  stack left: link %u, voice %u, deck %u, music %u, loop %u\n",
                      static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(crashlog::fails()),
                      linkHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(linkHandle)) : 0u,
                      voiceHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(voiceHandle)) : 0u,
                      deckHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(deckHandle)) : 0u,
                      musicHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(musicHandle)) : 0u,
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    handlePad();
    irTick();
    padhid::retry();
    pushTick();
    musicTick();
    fadeTick();
    updateClock();
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
    // The first time since boot a lit light plays the welcome to it instead (startIntro), and
    // ">WELCOME" shows after the link message (it says more).
    static bool welcomed[N_CTRL] = {};
    bool welcomedNow = false;
    static bool shown[N_CTRL] = {};
    static uint32_t edgeAt[N_CTRL] = {};  // when each light last changed link state, and which way
    static bool edgeUp[N_CTRL] = {};
    for (int i = 0; i < N_CTRL; i++) {
        Controller& c = controllers[i];
        const bool up = linkUp(c);
        if (up && c.fresh) {
            c.fresh = false;
            if (!welcomed[i] && shownLook(i).lit && !musicDriving) {
                startIntro(i);
                welcomedNow = true;
            } else {
                push(i);
            }
            welcomed[i] = true;
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
            // the boot's ">LAST TRIP" has its 6 s: a link change doesn't cut it short (the lights still redraw)
            if (!(message[0] && messageMs > MESSAGE_MS && millis() - messageAt < messageMs)) setMessage(m);
            drawLightState();  // STATUS on Lights (the message too), the lights band on the Menu: NO LINK while it's gone
            if (screen == Screen::Menu) drawMessage();
        }
    }
    if (welcomedNow && !(message[0] && messageMs > MESSAGE_MS && millis() - messageAt < messageMs)) {
        setMessage(">WELCOME");
        drawMessage();
    }
    karaokeAnswerTick();
    updateTheme();  // a backstop: every path above has already called it
    publish();  // for /state and /status
    delay(10);
}
