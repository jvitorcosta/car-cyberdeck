// Companion screen (env cyd_companion): the CYD beside the S3. With an avatar (data/avatar.bin)
// she is the home screen and the menu (LYRICS / SOUNDS / TRIP, touch); without one it shows the
// lyrics big, as before. It reads the S3's GET /state over the hotspot. With the car there (the
// OBD adapter live) it's the gauge cluster with a lyric strip while moving, her lyrics with a
// gauge strip when stopped, or the view pinned on the phone.
// Specs: docs/superpowers/specs/2026-10-13-companion-screen-design.md,
//        docs/superpowers/specs/2026-10-14-avatar-home-design.md,
//        docs/superpowers/specs/2026-10-21-companion-drive-design.md
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <cmath>
#include "../board/board.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../ui/backlight.h"
#include "../ui/theme.h"
#include "../util/carstatus.h"
#include "../util/carwarn.h"
#include "../util/daynight.h"
#include "../util/driveview.h"
#include "../util/lyricclock.h"
#include "../util/menunav.h"
#include "../util/obdplan.h"
#include "../util/press.h"
#include "../util/version.h"
#include "avatarview.h"
#include "carstrip.h"
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
static obdplan::Hold s_obdHold;   // the car is there (util/obdplan: at once, gone 5 s after)
static driveview::Pin s_pin = driveview::Pin::Auto;   // the view pinned on the phone (NVS)
static driveview::Mode s_mode;
static driveview::Show s_show = driveview::Show::Menu;  // as drawn
static carwarn::Watch s_warn;

// The pinned view survives power-off (NVS "companion"/"pin").
static void loadPin() {
    Preferences p;
    if (!p.begin("companion", false)) return;            // read-write: creates it, no NOT_FOUND
    const uint8_t v = p.getUChar("pin", 0);
    p.end();
    if (v <= (uint8_t)driveview::Pin::Lyrics) s_pin = (driveview::Pin)v;
}

static void savePin() {
    Preferences p;
    if (!p.begin("companion", false)) return;
    p.putUChar("pin", (uint8_t)s_pin);
    p.end();
}

#ifdef GAUGE_DEMO
// -D GAUGE_DEMO: the gauge cluster with fixed values and no car, to see the screen on the desk.
static void gaugeDemo(obdlink::View& car, uint32_t now) {
    using obd::Pid;
    const struct { Pid p; float v; } VALUES[] = {{Pid::Rpm, 3850}, {Pid::Speed, 87}, {Pid::Coolant, 89},
                                                 {Pid::Fuel, 61}, {Pid::Battery, 14.1f}, {Pid::Maf, 12.3f},
                                                 {Pid::PedalD, 52}, {Pid::Throttle, 38}};
    car.link = obdplan::Link::Live;
    for (const auto& x : VALUES) {
        car.value[(int)x.p] = x.v;
        car.have[(int)x.p] = true;
        car.atMs[(int)x.p] = now;
    }
    car.kmlNow = 13.8f;
    car.kmlAvg = 11.9f;
    car.kmLeft = 320;
    car.tripKm = 42.6f;
    car.pedalPct = 62;
    car.throttlePct = 41;
    car.atStart = 10.4f;
}
#endif

// Moves to screen s and repaints it whole.
static void enter(menunav::Screen s) {
    if (s_show != driveview::Show::Menu) { s_screen = s; return; }   // the car's view; s shows on return
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
    if (s_show != driveview::Show::Menu) return;  // the car's view owns the screen (e.g. a phone RESET)
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

// The stopped view whole: her lyrics screen under the gauge strip, her line in the message bar.
static void paintStopped() {
    s_tft.fillScreen(theme::pal().top);
    screens::invalidate();
    avatarview::place(4, gauges::TOP_H + 4, 1);
    avatarview::invalidate();
    dashboard::setTop(gauges::TOP_H);
    dashboard::invalidate();
    gauges::invalidateTop();
}

// Paints view `to` whole: the car came or went, moved off or stopped, or the pin changed. She
// greets the car, and on leaving tells the ignition (CarOff) from a dropped adapter link.
static void show(driveview::Show to, const decklink::View& v, obdplan::Link link, uint32_t now) {
    const driveview::Show from = s_show;
    s_show = to;
    switch (to) {
    case driveview::Show::Cluster:
        s_tft.fillScreen(TFT_BLACK);              // the cluster owns the whole screen
        avatarview::place(carstrip::PORTRAIT_X, gauges::STRIP_Y + 1, avatarview::HALF);
        avatarview::invalidate();
        gauges::invalidate();
        carstrip::invalidate();
        break;
    case driveview::Show::Stopped:
        paintStopped();
        break;
    case driveview::Show::Menu:
        dashboard::setTop(0);
        enter(s_screen);                          // back to the screen it left
        avatarview::say(link == obdplan::Link::Off || link == obdplan::Link::Connecting
                            ? avatarlines::Event::AdapterLost : avatarlines::Event::CarOff, v, now);
        return;
    }
    if (from == driveview::Show::Menu) avatarview::say(avatarlines::Event::CarOn, v, now);
}

// The cluster's warnings, said by her once each (util/carwarn).
static void carWarnings(const obdlink::View& car, const decklink::View& v, uint32_t now) {
    const float c = obdlink::freshValue(car, obd::Pid::Coolant, now);
    const float volts = obdlink::freshValue(car, obd::Pid::Battery, now);
    const float rpm = obdlink::freshValue(car, obd::Pid::Rpm, now);
    const bool hot = !std::isnan(c) && obdplan::coolantWarn(c);
    const bool low = !std::isnan(volts) && obdplan::batteryWarn(volts, rpm > 0.0f);
    const carwarn::Warn w = s_warn.update(hot, low, now);
    if (w == carwarn::Warn::CoolantHot) avatarview::say(avatarlines::Event::CoolantHot, v, now);
    else if (w == carwarn::Warn::BatteryLow) avatarview::say(avatarlines::Event::BatteryLow, v, now);
}

// What the phone page's GET /status says, twice a second.
static void publishStatus(const obdlink::View& car, const decklink::View& v, const char* herLine,
                          uint32_t now) {
    static uint32_t last = 0;
    if (now - last < 500) return;
    last = now;
    const char* link = car.link == obdplan::Link::Live         ? "live"
                       : car.link == obdplan::Link::NoEngine   ? "noengine"
                       : car.link == obdplan::Link::Connecting ? "connecting" : "off";
    const float kmh = obdlink::freshValue(car, obd::Pid::Speed, now);
    const char* line = herLine;
    if (!line && v.polledAtMs) line = lyricclock::pick(v.deck, now - v.polledAtMs).big;
    static char json[carstatus::MAX_JSON];
    carstatus::write({link, std::isnan(kmh) ? -1 : (int)(kmh + 0.5f), driveview::name(s_pin),
                      driveview::name(s_show), line},
                     json, sizeof json);
    phone::setStatus(json);
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
        loadPin();
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
            if (s_show == driveview::Show::Cluster) {
                // the gauges are green day and night: nothing to repaint (the screen it left
                // repaints in the new palette on return)
            } else if (s_show == driveview::Show::Stopped) {
                paintStopped();
            } else if (avatarview::on()) enter(s_screen);
            else {
                s_tft.fillScreen(theme::pal().top);
                dashboard::invalidate();
            }
        }
    }
    decklink::View v;
    decklink::snapshot(v);
    obdlink::View car;
    obdlink::snapshot(car);
    const uint32_t now = millis();                // after the snapshots: no reading is newer
    if (!avatarview::on()) {                      // no avatar: lyrics only, touch ignored
        dashboard::draw(s_tft, v, now);
        delay(20);
        return;
    }
#ifdef GAUGE_DEMO
    gaugeDemo(car, now);
#endif
    driveview::Pin picked;
    if (phone::takePin(&picked) && picked != s_pin) {
        s_pin = picked;
        savePin();
        Serial.printf("[companion] view: %s\n", driveview::name(s_pin));
    }
    const bool carShown = s_obdHold.update(car.link == obdplan::Link::Live, now);
    const driveview::Show want =
        s_mode.update(s_pin, carShown, obdlink::freshValue(car, obd::Pid::Speed, now), now);
    if (want != s_show) show(want, v, car.link, now);   // the car came or went, moved or stopped
    if (s_show != driveview::Show::Menu) {
        fire(phone::take());                      // the phone still works; screens apply on return
        handleResult(v, now);
        carWarnings(car, v, now);
        const char* line = avatarview::update(s_tft, v, now);
        if (s_show == driveview::Show::Cluster) {
            gauges::draw(s_tft, car, now);
            carstrip::draw(s_tft, v, line, avatarview::warning(), now);
        } else {
            gauges::drawTop(s_tft, car, now);
            dashboard::draw(s_tft, v, now);
            screens::bar(s_tft, line ? line
                                : s_waiting ? "..."
                                            : menunav::idleText(menunav::Screen::Lyrics, v.deckOk, v.wrongKey));
        }
        publishStatus(car, v, line, now);
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
    publishStatus(car, v, line, now);
    delay(20);
}
