#include <Arduino.h>
#include <TFT_eSPI.h>
#include "board/board.h"
#include "pins.h"
#include "net/wifi.h"
#include "net/clock.h"
#include "spotify/client.h"
#include "app_state.h"
#include "core/shared.h"
#include "core/nettask.h"
#include "core/mem.h"
#include "ui/screen_now.h"
#include "ui/cjkdata.h"
#include "ui/pokeball.h"
#include "ui/lyricmsg.h"
#include "ui/backlight.h"
#include "ui/bootscene.h"
#include "ui/intro.h"
#include "audio/greeting.h"
#include "ui/theme.h"
#include "util/daynight.h"
#include "util/linkstatus.h"
#include "net/remote.h"
#include "util/genre.h"
#include "util/version.h"
#include "util/interp.h"
#include "util/netplan.h"
#include "images/art.h"
#include "images/walksprite.h"
#include "images/cache.h"
#include <esp_system.h>
#include <esp_task_wdt.h>

// Task watchdog: 30 s instead of the default 5 s. SpotifyArduino's response loops spin
// (yield() never lets the lower-priority idle task run) and on a reset TLS connection one spun
// ~11 s on core 0, so the idle task missed the 5 s watchdog and the board rebooted
// (the "unexplained rst:0xc"). A genuine hang still reboots, after 30 s.
static const uint32_t TASK_WDT_S = 30;

static const char* resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_SW:       return "software (self-heal restart)";
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_INT_WDT:  return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT:      return "other watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_EXT:      return "external pin";
        default:               return "other";
    }
}

// UI loop (core 1): draws only. All network/SD work runs in core/nettask on core 0 and
// arrives through core/shared (README "Design & performance history").
static TFT_eSPI s_tft = TFT_eSPI();

constexpr uint32_t STALE_MS = 20000;      // "No signal" once the last good poll is this old
constexpr uint32_t CD_FRAME_MS = 160;     // spinning CD + Poke Ball, ~6 fps
constexpr uint32_t PROGRESS_MS = 250;     // time, HP/EXP bars, dialogue box
constexpr uint32_t NOTES_MS = 330;        // note icons bob ~3 steps/s
constexpr uint32_t WALK_FRAME_MS = 120;   // walker ~8 fps
constexpr uint32_t MODE_CHECK_MS = 10000; // day/night check (18:00-06:00 local time)

// The art box and the decoded bitmap are the same 92x92 area (pushArtIfValid pushes it whole).
static_assert(ui::ART_W == art::W && ui::ART_H == art::H, "art box and bitmap sizes differ");

static char s_topSig[96] = "";                 // last drawn top-strip state

// Pushes the album-art bitmap if it is valid for the track on screen. Check and push happen
// under one lock so the network task cannot start overwriting the bitmap in between.
static void pushArtIfValid(uint32_t gen) {
    shared::Guard g;
    if (shared::artValidLocked(gen))
        s_tft.pushImage(ui::ART_X, ui::ART_Y, art::W, art::H, art::bitmap());
}

static bool s_notesDance = false;   // a timed lyric line is on screen (set by drawDialogue)

// Dialogue box for the track on screen: the lyric line, or a Pokemon-style status message
// (searching / retrying / intro / no lyrics). The status age drives the FAIL -> IDLE switch.
static void drawDialogue(const AppState& view, uint32_t gen) {
    static lyricstatus::Status lastStatus = lyricstatus::Status::Searching;
    static uint32_t lastGen = 0, since = 0;
    static shared::LyricView lv;      // static: keep the loop stack small
    static lyricmsg::Out out;
    shared::lyricView(gen, view.progressMs, lv);
    uint32_t now = millis();
    if (gen != lastGen || lv.status != lastStatus) {
        lastGen = gen;
        lastStatus = lv.status;
        since = now;
    }
    lyricmsg::In in{lv.status, now - since, view.progressMs, lv.firstLineMs, lv.line,
                    view.pokeName, gen};
    lyricmsg::compose(in, out);
    ui::drawLyricArea(s_tft, out.text, out.notes);
    s_notesDance = out.dance;
}

void setup() {
    Serial.begin(115200);
    delay(200);
#ifdef BOARD_S3
    Serial.printf("[boot] cardeck v%s (s3)\n", FW_VERSION);
#else
    Serial.printf("[boot] cardeck v%s (cyd)\n", FW_VERSION);
#endif
    Serial.printf("[boot] reset reason: %s\n", resetReason());
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
#ifdef SD_CACHE   // parked: a mounted card costs the RAM TLS needs (docs/BACKLOG.md)
    cache::begin();
#endif
    s_tft.init(); s_tft.invertDisplay(true); s_tft.setRotation(1); s_tft.fillScreen(TFT_BLACK);
    backlight::begin();
    if (!spclient::hasToken()) {
        // The token comes from a PC (Spotify only redirects to loopback addresses), so the
        // deck can't do anything without one.
        s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        s_tft.drawString("No Spotify token.", 10, 10, 2);
        s_tft.drawString("Run tools/spotify_auth.py on a PC,", 10, 40, 2);
        s_tft.drawString("then flash again.", 10, 60, 2);
        Serial.println("[spotify] no refresh token in src/config.h: run tools/spotify_auth.py");
        for (;;) delay(1000);
    }
#ifdef BOARD_S3
    // The intro video (ui/intro, with its sound, if a clip was built) plays first, on its own task, while
    // WiFi connects and the network task logs into Spotify underneath; then the greeting and the
    // night drive. The S3 has heap to spare (~230 KB largest block), so the login and polls run
    // under the scenes (the network task's late-WiFi path: spclient::begin + netclock::begin).
    // setup() never blocks on the login: when the scene ends the deck shows at once, the song as
    // soon as the first poll lands. (Logging in here left the scene's faded last frame, i.e. a
    // black screen, up for as long as a slow hotspot took.)
    intro::start(s_tft);
    const bool wifi = net::connectAny();
    if (wifi) {
        mem::log("boot+wifi");
        netclock::begin();                // the clock (night palette) right away, not after login
        nettask::start(false, nullptr);   // logs in, then polls; no TLS hold
    }
    intro::wait();                        // the display is free for the night drive
    greeting::play();                     // waits for the intro's sound to finish (one output)
    bootscene::start(s_tft, net::isOnline);
    if (wifi) {
        bootscene::setCaption("Connecting to Spotify...");
        bootscene::waitDone();            // the display is ours again
    } else {
        // The scene keeps running; the network task retries and the car leaves once WiFi is up.
        bootscene::setCaption("WiFi not found - retrying...");
        nettask::start(false, nullptr);
    }
#else
    greeting::play();          // boot sound plays while WiFi connects
    // Night drive while WiFi connects: the scene draws until the car leaves (>= 5 s, WiFi up).
    bootscene::start(s_tft, net::isOnline);
    bootscene::setCaption("Connecting WiFi...");

    bool spotifyReady = false;
    if (net::connectAny()) {
        mem::log("boot+wifi");
        netclock::begin();
        bootscene::waitDone();               // the display is ours again
        s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        s_tft.fillScreen(TFT_BLACK);
        s_tft.drawString("Spotify auth...", 10, 10, 2);
        spclient::begin();
        spotifyReady = true;
    } else {
        // The scene keeps running; the network task retries and the car leaves once WiFi is up.
        bootscene::setCaption("WiFi not found - retrying...");
    }
    nettask::start(spotifyReady, bootscene::active);   // always: it keeps retrying WiFi if the hotspot is late
#endif
    remote::begin(bootscene::active);
}

// Status screen text when Spotify can't be polled: whose fault it is (util/linkstatus).
static linkstatus::Kind offlineText(const AppState& st, char* title, size_t tn, char* detail, size_t dn) {
    const bool wifiUp = net::isOnline();
    linkstatus::Kind k = linkstatus::classify({wifiUp, true, st.lastPollCode, st.certRejected});
    char names[96];
    if (wifiUp) strlcpy(names, net::ssid().c_str(), sizeof names);
    else net::savedNetworks(names, sizeof names);
    linkstatus::describe(k, st.lastPollCode, linkstatus::retryInS(millis(), st.nextPollMs), names,
                         title, tn, detail, dn);
    return k;
}


// Night mode: every MODE_CHECK_MS, picks the palette and backlight for the local time. True
// when the mode changed (the caller redraws the current screen). No clock yet -> day.
static bool updateDayNight() {
    static uint32_t lastCheck = 0;
    static bool first = true, loggedSync = false;
    uint32_t now = millis();
    if (!first && now - lastCheck < MODE_CHECK_MS) return false;
    first = false;
    lastCheck = now;
    int m = 0;
    bool synced = netclock::minuteOfDay(&m);
    if (synced && !loggedSync) {
        loggedSync = true;
        Serial.printf("[clock] synced, local %02d:%02d\n", m / 60, m % 60);
    }
    bool night = synced && daynight::isNight(m);
    if (night == theme::isNightActive()) return false;
    theme::setNight(night);
    backlight::set(night);
    Serial.printf("[ui] night mode %s\n", night ? "on" : "off");
    return true;
}

void loop() {
    if (bootscene::active()) {   // boot scene still on screen (WiFi wasn't up at boot)
        delay(20);
        return;
    }
    static uint32_t lastStackLog = 0;      // the loop task's stack use (8 KB default), once a minute
    if (millis() - lastStackLog >= 60000) {
        lastStackLog = millis();
        Serial.printf("[mem] loop stackfree=%u\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    AppState st;
    shared::snapshot(st);
    AppState view = st;   // interpolate progress for a smooth bar between polls
    view.progressMs = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying,
                                                millis() - st.lastPollMs);

    // Screen mode: deck when playing/paused; a status screen when offline or stopped.
    int mode = 0;   // 0 = deck, 1 = offline, 2 = nothing playing
    if (!net::isOnline() || st.status == PlaybackStatus::Offline ||
        netplan::stale(millis(), st.lastPollOkMs, STALE_MS)) mode = 1;
    else if (st.status == PlaybackStatus::Stopped) mode = 2;

    static int lastMode = -1;
    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    static uint8_t shownGenre = genre::NONE;   // badge for the track on screen
    if (updateDayNight()) lastMode = -1;   // redraw the deck or the status screen in the new palette
    if (mode != 0) {
        static char shownText[80] = "", shownTitle[24] = "";
        static char title[24], detail[48];
        static uint32_t textAt = 0, frameAt = 0;
        static linkstatus::Kind kind = linkstatus::Kind::Ok, shownKind = linkstatus::Kind::Ok;
        char text[80];   // title|detail: 24 + 1 + 48
        // Re-read the cause at most once a second: net::ssid() allocates a String.
        if (lastMode != mode || millis() - textAt >= 1000) {
            textAt = millis();
            strlcpy(title, "Nothing playing", sizeof title);
            detail[0] = '\0';
            kind = mode == 1 ? offlineText(st, title, sizeof title, detail, sizeof detail)
                             : linkstatus::Kind::Ok;
        }
        const statusscene::Look look = statusscene::lookFor(kind);
        snprintf(text, sizeof text, "%s|%s", title, detail);
        if (lastMode != mode || kind != shownKind) {   // new screen: everything, once
            ui::drawOffline(s_tft, title, detail, look, millis());
            frameAt = millis();
        } else if (strcmp(text, shownText) != 0) {      // same screen, new text (countdown)
            const char* shownDetail = strchr(shownText, '|');   // same two-line layout as before?
            if (strcmp(title, shownTitle) == 0 && detail[0] && shownDetail && shownDetail[1])
                ui::drawOfflineDetail(s_tft, detail);
            else ui::drawOfflineText(s_tft, title, detail);
        } else if (millis() - frameAt >= statusscene::FRAME_MS) {   // Pokemon bob, bubble, ball
            frameAt = millis();
            ui::drawOfflineScene(s_tft, look, frameAt);
        }
        if (lastMode != mode || strcmp(title, shownTitle) != 0)   // log the screen, not each tick
            Serial.printf("[screen] %s / %s\n", title, detail);
        if (lastMode != mode || strcmp(text, shownText) != 0) remote::setStatus(title, detail);
        strlcpy(shownText, text, sizeof shownText);
        strlcpy(shownTitle, title, sizeof shownTitle);
        shownKind = kind;
        lastMode = mode;
    } else if (lastMode != 0 || st.trackGen != shownGen) {
        // Full deck redraw: immediately on track change (text + Pokemon name; art, lyric
        // and walker fill in as the network task delivers them), or back from a status screen.
        bool newTrack = st.trackGen != shownGen;
        if (lastMode != 0) remote::setStatus("", "");   // the phone page: no status, the song
        lastMode = 0;
        if (newTrack) {
            shownGenre = genre::NONE;
            shownGen = st.trackGen;
            walkerOn = false;
        }
        uint8_t cached;   // a cached badge is posted with the track change: draw it with the title
        if (shared::takeGenre(shownGen, &cached)) shownGenre = cached;
        ui::drawNow(s_tft, view, shownGenre);
        s_topSig[0] = '\0';
        pushArtIfValid(shownGen);   // back from a status screen: same track's art is still valid
        drawDialogue(view, shownGen);
        mem::log("track");
    } else {
        // Media arriving from the network task for the track on screen.
        if (shared::takeArt(shownGen)) pushArtIfValid(shownGen);
        if (shared::takeWalker(shownGen)) walkerOn = true;
        uint8_t g;
        if (shared::takeGenre(shownGen, &g)) {   // genre arrived (or was cached): artist row only
            shownGenre = g;
            ui::drawArtistRow(s_tft, view, g);
        }

        static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0, lastTick = 0, animMs = 0;
        static int walkStep = 0, cdFrame = 0, ballFrame = 0;
        uint32_t now = millis();
        uint32_t dt = now - lastTick;
        lastTick = now;
        if (st.isPlaying) animMs += dt;   // walk cycle runs only while playing

        // top strip: redraw only when device / shuffle / repeat change
        char sig[96];
        snprintf(sig, sizeof(sig), "%s|%s|%d|%d|%d", st.deviceName, st.deviceType,
                 (int)st.shuffle, st.repeat, (int)st.isPlaying);
        if (strcmp(sig, s_topSig) != 0) {
            strcpy(s_topSig, sig);
            ui::drawTopStrip(s_tft, st);
        }
        if (st.isPlaying && now - lastCd >= CD_FRAME_MS) {   // spinning CD ~6 fps
            lastCd = now;
            ui::drawCdFrame(s_tft, cdFrame = (cdFrame + 1) & 3);
            ui::drawPokeballFrame(s_tft, ballFrame = (ballFrame + 1) % pokeball::FRAMES);
        }
        if (now - lastDraw >= PROGRESS_MS) {
            lastDraw = now;
            ui::drawProgressRegion(s_tft, view);
            drawDialogue(view, shownGen);
        }
        // Note icons bob while timed lyrics are shown and the music plays; at rest otherwise.
        static uint32_t lastNotes = 0;
        static int noteFrame = 0;
        static bool notesMoved = false;
        if (s_notesDance && st.isPlaying) {
            if (now - lastNotes >= NOTES_MS) {            // ~3 steps/s: alive, not distracting
                lastNotes = now;
                ui::drawNoteFrame(s_tft, ++noteFrame & 0x7FFF);
                notesMoved = true;
            }
        } else if (notesMoved) {
            ui::drawNoteFrame(s_tft, -1);
            notesMoved = false;
        }
        if (walkerOn && now - lastWalk >= WALK_FRAME_MS) {     // ~8 fps walker
            lastWalk = now;
            if (st.isPlaying) walkStep++;
            ui::drawWalker(s_tft, view, animMs, walkStep);
        }
    }
    delay(10);
}
