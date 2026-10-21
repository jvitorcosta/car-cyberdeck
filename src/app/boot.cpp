// setup(): the board, the boot opening, WiFi and the network task. loop() is in main.cpp.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "display.h"
#include "../board/board.h"
#include "pins.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../net/remote.h"
#include "../net/ota.h"
#include "../spotify/client.h"
#include "../core/shared.h"
#include "../core/nettask.h"
#include "../core/mem.h"
#include "../core/media.h"
#include "../ui/cjkdata.h"
#include "../ui/backlight.h"
#include "../ui/bootscene.h"
#include "../ui/intro.h"
#include "../ui/carlook.h"
#include "../audio/greeting.h"
#include "../board/s3/touch_ft6336.h"
#include "../images/art.h"
#include "../images/walksprite.h"
#include "../util/idlesprite.h"
#include "../util/intropick.h"
#include "../util/version.h"
MEDIA_DECL(data_idle_bin);   // Sonic on "Waiting for a song..." (tools/gen_idle_sprite.py), else empty

// Task watchdog: 30 s instead of the default 5 s. It was raised for SpotifyArduino's spinning
// response loops (one spun ~11 s on a reset TLS connection: the "unexplained rst:0xc"); that
// library is gone, but a TLS handshake plus reads can still take 8 + 8 s on a weak hotspot.
// A genuine hang still reboots, after 30 s.
static const uint32_t TASK_WDT_S = 30;

static const char* resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_SW:       return "software (self-heal, update or rollback)";
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_INT_WDT:  return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT:      return "other watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_EXT:      return "external pin";
        default:               return "other";
    }
}

// This boot's opening (util/intropick): random, never last boot's (NVS "boot"/"last"), only
// from what the build has. -D INTRO_FORCE=\"kiss\" (or honda, mcqueen) forces one for testing.
static intropick::Opening pickOpening(bool haveKiss, bool haveMcQueen) {
    Preferences p;
    p.begin("boot", false);
    const int last = p.getInt("last", -1);
    int o = -1;
#ifdef INTRO_FORCE
    o = intropick::forced(intropick::parse(INTRO_FORCE), haveKiss, haveMcQueen);
#endif
    if (o < 0) o = intropick::pick(esp_random(), last, haveKiss, haveMcQueen);
    p.putInt("last", o);
    p.end();
    Serial.printf("[boot] opening: %s\n", intropick::name((intropick::Opening)o));
    return (intropick::Opening)o;
}

void setup() {
    TFT_eSPI& tft = app::tft();
    Serial.begin(115200);
    delay(200);
    Serial.printf("[boot] cardeck v%s (s3)\n", FW_VERSION);
    Serial.printf("[boot] reset reason: %s\n", resetReason());
    ota::checkBoot();   // may boot the other app slot (net/ota)
    board::begin();
    esp_task_wdt_init(TASK_WDT_S, true);   // reconfigures the already-running watchdog
    mem::installFailHook();
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);
    shared::begin();
    walk::begin();            // allocate both walker slots before the heap fragments
    art::begin();             // fixed album-art bitmap (before WiFi)
    {   // CJK font sanity check (glyphs stay in flash)
        int w = 0;
        bool ok = ui::cjk().glyph(0x3042, &w) != nullptr;   // HIRAGANA LETTER A
        Serial.printf("[cjk] ranges=%d glyph(U+3042)=%s w=%d\n", ui::cjk().rangeCount(),
                      ok ? "ok" : "missing", w);
    }
    tft.init(); tft.invertDisplay(true); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    backlight::begin();
    touchft::begin();   // the music box's speaker button
    if (!spclient::hasToken()) {
        // The token comes from a PC (Spotify only redirects to loopback addresses), so the
        // deck can't do anything without one.
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("No Spotify token.", 10, 10, 2);
        tft.drawString("Run tools/spotify_auth.py on a PC,", 10, 40, 2);
        tft.drawString("then flash again.", 10, 60, 2);
        Serial.println("[spotify] no refresh token in src/config.h: run tools/spotify_auth.py");
        for (;;) delay(1000);
    }
    // One random opening (pickOpening): the kiss video (ui/intro, its own task, the boot terminal
    // over it), or the night drive with the Honda or McQueen and their greeting; all of them end
    // in the photo montage. WiFi connects and the network task logs into Spotify underneath. The S3 has heap to spare (~230 KB largest block), so the login and polls run
    // under the scenes (the network task's late-WiFi path: spclient::begin + netclock::begin).
    // setup() never blocks on the login: when the scene ends the deck shows at once, the song as
    // soon as the first poll lands. (Logging in here left the scene's faded last frame, i.e. a
    // black screen, up for as long as a slow hotspot took.)
    media::begin();
    const media::Blob idle = MEDIA(data_idle_bin, "idle.bin");
    Serial.println(idlesprite::parse(idle.data, idle.size)
                       ? "[idle] Sonic waits on Nothing playing" : "[idle] none: Pikachu");
    carlook::Look mcqueen;
    const bool haveMcQueen = carlook::mcqueen(&mcqueen);
    const intropick::Opening opening = pickOpening(intro::available(), haveMcQueen);
    greeting::choose(intropick::greetingFor(opening));
    const bootscene::Opts opts{opening == intropick::MCQUEEN ? mcqueen : carlook::honda(),
                               opening != intropick::KISS};
    if (opening == intropick::KISS) {
        intro::start(tft, net::isOnline);
    } else {                              // the drive covers the WiFi connect (the screen'd be black)
        greeting::play();
        bootscene::start(tft, net::isOnline, opts);
    }
    const bool wifi = net::connectAny();
    if (wifi) {
        mem::log("boot+wifi");
        netclock::begin();                // the clock (night palette) right away, not after login
        nettask::start();                 // logs in, then polls
    } else {
        // The network task retries WiFi underneath; the terminal says RETRYING... (the kiss holds
        // its last frame, the car cruises) until it's up. Started before intro::wait(), which
        // waits for WiFi.
        bootscene::setCaption("WiFi not found - retrying...");
        nettask::start();
    }
    if (opening == intropick::KISS) {
        intro::wait();                    // over, and online (the last frame holds till then)
        greeting::play();                 // no greeting: waits for the intro's sound (one output)
        bootscene::start(tft, net::isOnline, opts);   // straight to the montage
    }
    if (wifi) bootscene::setCaption("Connecting to Spotify...");
    bootscene::waitDone();                // the display is ours again
    remote::begin(bootscene::active);
    ota::begin(bootscene::active);
}
