// Companion screen (env cyd_companion): the CYD beside the S3. With an avatar (data/avatar.bin)
// she is the home screen and the menu (LYRICS / SOUNDS / TRIP, touch); without one it shows the
// lyrics big, as before. It reads the S3's GET /state over the hotspot.
// Specs: docs/superpowers/specs/2026-10-13-companion-screen-design.md,
//        docs/superpowers/specs/2026-10-14-avatar-home-design.md
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../board/board.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../ui/backlight.h"
#include "../ui/theme.h"
#include "../util/daynight.h"
#include "../util/menunav.h"
#include "../util/obdplan.h"
#include "../util/press.h"
#include "../util/version.h"
#include "avatarview.h"
#include "dashboard.h"
#include "link.h"
#include "gauges.h"
#include "obdlink.h"
#include "phone.h"
#include "screens.h"
#include "touch.h"

static TFT_eSPI s_tft;
static menunav::Screen s_screen = menunav::Screen::Home;
static menunav::Button s_cursor = menunav::Lyrics;   // last menu choice (the ▶ on Home)
static bool s_vroom = false;                          // the new deck offers VROOM (as drawn)
static bool s_waiting = false;                        // an action is on its way to the deck
static press::Press s_press;
static obdplan::Hold s_obdHold;   // gauges on screen (util/obdplan: shown at once, gone 5 s after)
static bool s_gauges = false;     // as drawn

// Moves to screen s and repaints it whole.
static void enter(menunav::Screen s) {
    if (s_gauges) { s_screen = s; return; }       // the gauges own the screen; s shows on return
    s_screen = s;
    screens::draw(s_tft, s, s_vroom, s_cursor);
    if (s == menunav::Screen::Home) avatarview::place(68, 8, 2);      // centred, 184x184
    else if (s == menunav::Screen::Lyrics) avatarview::place(4, 4, 1);
    else avatarview::place(0, 0, 0);
    avatarview::invalidate();
    dashboard::invalidate();
}

static bool findBtn(menunav::Button id, menunav::Btn* out) {
    menunav::Btn b[menunav::MAX_BTNS];
    const int n = menunav::buttons(s_screen, s_vroom, b);
    for (int i = 0; i < n; ++i)
        if (b[i].id == id) { *out = b[i]; return true; }
    return false;
}

static void redrawButton(menunav::Button id, bool pressed, float fill) {
    menunav::Btn b;
    if (id != menunav::None && findBtn(id, &b))
        screens::button(s_tft, b, pressed, fill,
                        s_screen == menunav::Screen::Home && id == s_cursor);
}

// A button fired, by touch or by the phone: navigate, or send an action to the deck. From the
// phone, LYRICS / SOUNDS / TRIP open their screen from anywhere.
static void fire(menunav::Button b) {
    if (!menunav::offered(b, s_vroom)) return;    // e.g. VROOM from the phone, not on the deck
    switch (b) {
    case menunav::None: return;
    case menunav::Howl: decklink::request(decklink::Action::Howl); s_waiting = true; return;
    case menunav::Vroom: decklink::request(decklink::Action::Vroom); s_waiting = true; return;
    case menunav::Reset:
        decklink::request(decklink::Action::TripReset);
        s_waiting = true;
        redrawButton(menunav::Reset, false, 0.0f);
        return;
    case menunav::Back: enter(menunav::Screen::Home); return;
    default:                                      // Lyrics / Sounds / Trip
        s_cursor = b;
        enter(menunav::next(menunav::Screen::Home, b));
        return;
    }
}

// Touch -> press -> a button fires.
static void handleTouch(uint32_t now) {
    int x = 0, y = 0;
    const bool down = touch::read(&x, &y);
    const menunav::Button under = down ? menunav::hit(s_screen, s_vroom, x, y) : menunav::None;
    const menunav::Button before = (menunav::Button)s_press.pressed();
    const menunav::Button fired =
        (menunav::Button)s_press.update(down, under, menunav::isHold(under), now);
    const menunav::Button after = (menunav::Button)s_press.pressed();
    if (before != after) {                        // pressed look follows the finger
        redrawButton(before, false, 0.0f);
        redrawButton(after, true, 0.0f);
    }
    static uint32_t lastFill = 0;
    if (after == menunav::Reset && now - lastFill >= 50) {   // RESET fills while held
        lastFill = now;
        redrawButton(after, true, s_press.holdProgress(now));
    }
    fire(fired);
}

// The deck answered an action: she reacts.
static void handleResult(const decklink::View& v, uint32_t now) {
    decklink::Action a = decklink::Action::None;
    const decklink::Result r = decklink::takeResult(&a);
    if (r == decklink::Result::Pending || r == decklink::Result::Idle) return;
    s_waiting = false;
    avatarlines::Event ev = avatarlines::Event::Unreachable;
    if (r == decklink::Result::Busy) ev = avatarlines::Event::Busy;
    else if (r == decklink::Result::Ok)
        ev = a == decklink::Action::Howl ? avatarlines::Event::Howl
           : a == decklink::Action::Vroom ? avatarlines::Event::Vroom : avatarlines::Event::TripReset;
    avatarview::say(ev, v, now);
}

void setup() {
    Serial.begin(115200);
    board::begin();
    Serial.printf("[boot] cardeck companion v%s\n", FW_VERSION);
    s_tft.init(); s_tft.invertDisplay(true); s_tft.setRotation(1);
    s_tft.fillScreen(theme::pal().top);
    backlight::begin();
    s_tft.setTextColor(theme::pal().topText, theme::pal().top);
    s_tft.drawString("Connecting to WiFi...", 10, 10, 2);
    net::connectAny();          // if it fails, the link task keeps retrying (net::loop)
    netclock::begin();          // night palette
    decklink::start();
    obdlink::start();           // Bluetooth to the ELM327 (its own task); gauges when it answers
    if (avatarview::begin()) {
        touch::begin();
        phone::begin();
        enter(menunav::Screen::Home);
    } else {
        s_tft.fillScreen(theme::pal().top);
        dashboard::invalidate();
    }
}

void loop() {
    static uint32_t lastNightCheck = 0;
    if (lastNightCheck == 0 || millis() - lastNightCheck > 60000) {
        lastNightCheck = millis() | 1;
        int minute = 0;
        const bool night = netclock::minuteOfDay(&minute) && daynight::isNight(minute);
        if (night != theme::isNightActive()) {
            theme::setNight(night);
            backlight::set(night);
            if (avatarview::on()) enter(s_screen);
            else {
                s_tft.fillScreen(theme::pal().top);
                dashboard::invalidate();
            }
        }
    }
    decklink::View v;
    decklink::snapshot(v);
    const uint32_t now = millis();
    if (!avatarview::on()) {                      // no avatar: lyrics only, touch ignored
        dashboard::draw(s_tft, v, now);
        delay(20);
        return;
    }
    obdlink::View car;
    obdlink::snapshot(car);
    const bool showGauges = s_obdHold.update(car.link == obdplan::Link::Live, now);
    if (showGauges != s_gauges) {                 // the car came or went
        s_gauges = showGauges;
        if (s_gauges) {
            s_tft.fillRect(0, 0, 320, menunav::BAR.y - 2, theme::pal().top);
            avatarview::place(0, 0, 0);           // she leaves the screen, her bar stays
            gauges::invalidate();
            screens::invalidate();
            avatarview::say(avatarlines::Event::CarOn, v, now);
        } else {
            enter(s_screen);                      // back to the screen it left
            avatarview::say(avatarlines::Event::CarOff, v, now);
        }
    }
    if (s_gauges) {
        fire(phone::take());                      // the phone still works; screens apply on return
        handleResult(v, now);
        const char* line = avatarview::update(s_tft, v, now);
        gauges::draw(s_tft, car, now);
        screens::bar(s_tft, line ? line : "Live from the car");
        delay(20);
        return;
    }
    if (v.deck.hasVroom != s_vroom) {             // the deck's clip list changed
        s_vroom = v.deck.hasVroom;
        if (s_screen == menunav::Screen::Sounds) enter(s_screen);
    }
    handleTouch(now);
    fire(phone::take());
    handleResult(v, now);
    const char* line = avatarview::update(s_tft, v, now);
    if (s_screen == menunav::Screen::Lyrics) dashboard::draw(s_tft, v, now);
    if (s_screen == menunav::Screen::Trip && v.polledAtMs)       // ticks locally between polls
        screens::tripTime(s_tft, v.deck.tripS + (now - v.polledAtMs) / 1000);
    screens::bar(s_tft, line ? line : s_waiting ? "..." : menunav::idleText(s_screen, v.deckOk, v.wrongKey));
    delay(20);
}
