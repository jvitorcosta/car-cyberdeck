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
#include "util/idlesprite.h"
#include "core/media.h"
MEDIA_DECL(data_idle_bin);   // Sonic on "Waiting for a song..." (tools/gen_idle_sprite.py), else empty
MEDIA_DECL(data_backdrop_bin); // Green Hill Zone behind the update screen (tools/gen_backdrop.py)
MEDIA_DECL(data_shadow_bin); // Shadow skating on its rail (tools/gen_shadow_rider.py)
#include "util/backdrop.h"
#include "util/ridersprite.h"
#include "audio/greeting.h"
#include "ui/theme.h"
#include "util/daynight.h"
#include "util/linkstatus.h"
#include "net/remote.h"
#include "net/remote_config.h"
#include "net/ota.h"
#include "ui/otascreen.h"
#include "util/qrfit.h"
#include <qrcode.h>
#include "util/genre.h"
#include "util/version.h"
#include "util/interp.h"
#include "util/netplan.h"
#include "util/idlewait.h"
#include <Preferences.h>
#include "util/intropick.h"
#include "ui/carlook.h"
#include "images/art.h"
#include "images/walksprite.h"
#include <esp_system.h>
#include <esp_task_wdt.h>

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

// UI loop (core 1): draws only. All network work runs in core/nettask on core 0 and
// arrives through core/shared (README "Design & performance history").
static TFT_eSPI s_tft = TFT_eSPI();

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

// This boot's opening (util/intropick): random, never last boot's (NVS "boot"/"last"), only
// from what the build has. -D INTRO_FORCE=\"kiss\" (or honda, mcqueen, max) forces one for testing.
static intropick::Opening pickOpening(bool haveKiss, bool haveMcQueen, bool haveMax) {
    Preferences p;
    p.begin("boot", false);
    const int last = p.getInt("last", -1);
    int o = -1;
#ifdef INTRO_FORCE
    o = intropick::forced(intropick::parse(INTRO_FORCE), haveKiss, haveMcQueen, haveMax);
#endif
    if (o < 0) o = intropick::pick(esp_random(), last, haveKiss, haveMcQueen, haveMax);
    p.putInt("last", o);
    p.end();
    Serial.printf("[boot] opening: %s\n", intropick::name((intropick::Opening)o));
    return (intropick::Opening)o;
}

void setup() {
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
    // One random opening (pickOpening): the kiss or Max Verstappen video (ui/intro, its own task,
    // the boot terminal over it), or the night drive with the Honda or McQueen and their greeting; all of them end
    // in the photo montage. WiFi connects and the network task logs into Spotify underneath. The S3 has heap to spare (~230 KB largest block), so the login and polls run
    // under the scenes (the network task's late-WiFi path: spclient::begin + netclock::begin).
    // setup() never blocks on the login: when the scene ends the deck shows at once, the song as
    // soon as the first poll lands. (Logging in here left the scene's faded last frame, i.e. a
    // black screen, up for as long as a slow hotspot took.)
    media::begin();
    const media::Blob idle = MEDIA(data_idle_bin, "idle.bin");
    Serial.println(idlesprite::parse(idle.data, idle.size)
                       ? "[idle] Sonic waits on Nothing playing" : "[idle] none: Pikachu");
    const media::Blob bkd = MEDIA(data_backdrop_bin, "backdrop.bin");
    Serial.println(backdrop::parse(bkd.data, bkd.size) ? "[ota] update screen: Green Hill Zone"
                                                       : "[ota] update screen: battle box");
    const media::Blob rider = MEDIA(data_shadow_bin, "shadow.bin");
    if (ridersprite::parse(rider.data, rider.size)) Serial.println("[ota] Shadow skates the rail");
    carlook::Look mcqueen;
    const bool haveMcQueen = carlook::mcqueen(&mcqueen);
    const intropick::Opening opening = pickOpening(intro::available(intropick::KISS), haveMcQueen,
                                                   intro::available(intropick::MAX));
    const bool video = intropick::isVideo(opening);
    greeting::choose(intropick::greetingFor(opening));
    const bootscene::Opts opts{opening == intropick::MCQUEEN ? mcqueen : carlook::honda(),
                               !video};
    if (video) {
        intro::start(s_tft, net::isOnline, opening);
    } else {                              // the drive covers the WiFi connect (the screen'd be black)
        greeting::play();
        bootscene::start(s_tft, net::isOnline, opts);
    }
    const bool wifi = net::connectAny();
    if (wifi) {
        mem::log("boot+wifi");
        netclock::begin();                // the clock (night palette) right away, not after login
        nettask::start();                 // logs in, then polls
    } else {
        // The network task retries WiFi underneath; the terminal says RETRYING... (a video holds
        // its last frame, the car cruises) until it's up. Started before intro::wait(), which
        // waits for WiFi.
        bootscene::setCaption("WiFi not found - retrying...");
        nettask::start();
    }
    if (video) {
        intro::wait();                    // over, and online (the last frame holds till then)
        greeting::play();                 // no greeting: waits for the intro's sound (one output)
        bootscene::start(s_tft, net::isOnline, opts);   // straight to the montage
    }
    if (wifi) bootscene::setCaption("Connecting to Spotify...");
    bootscene::waitDone();                // the display is ours again
    remote::begin(bootscene::active);
    ota::begin(REMOTE_NAME, bootscene::active);
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

// The phone-remote link as a QR code for the "Waiting for a song..." screen (esp_qrcode, built into
// the core), from the deck's current address: 172.20.10.10 on the iPhone hotspot, the DHCP one
// elsewhere. Rebuilt when the link changes (another network, a reconnect, the hotspot address
// pinned after DHCP), checked when the screen appears and then at most every 5 s:
// net::deviceIp() allocates a String.
constexpr int QR_N = qrfit::modules(qrfit::MAX_VERSION);   // modules per side (29)
static uint8_t s_qrBits[(QR_N * QR_N + 7) / 8];
static qrcard::Qr s_qr{s_qrBits, 0};
static uint32_t s_qrGen = 0;   // bumped when the code changes: the screen redraws it

static void copyQr(esp_qrcode_handle_t h) {
    const int n = esp_qrcode_get_size(h);
    s_qr.size = 0;
    if (n > QR_N) return;
    memset(s_qrBits, 0, sizeof s_qrBits);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (esp_qrcode_get_module(h, x, y)) {
                const int i = y * n + x;
                s_qrBits[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
            }
    s_qr.size = n;
}

static const qrcard::Qr* phoneQr(bool recheck) {
    static char built[96] = "";
    static uint32_t checkedAt = 0;
    if (recheck || millis() - checkedAt >= 5000) {
        checkedAt = millis();
        char url[96];
        if (!remote::link(url, sizeof url)) {
            if (built[0]) {
                Serial.println("[qr] none (remote off or offline)");
                ++s_qrGen;
            }
            built[0] = '\0';
            s_qr.size = 0;
        } else if (strcmp(url, built) != 0) {
            strlcpy(built, url, sizeof built);
            esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
            cfg.display_func = copyQr;
            cfg.max_qrcode_version = qrfit::MAX_VERSION;
            cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
            s_qr.size = 0;
            esp_qrcode_generate(&cfg, url);
            ++s_qrGen;
            const char* key = strstr(url, "?k=");                  // the key stays off the log
            Serial.printf("[qr] %.*s?k=<REMOTE_TOKEN> (%d modules)\n",
                          key ? (int)(key - url) : (int)strlen(url), url, s_qr.size);
        }
    }
    return s_qr.size ? &s_qr : nullptr;
}

#ifdef OTA_DEMO
// -D OTA_DEMO: the update screen on a loop with no upload, to see it on the desk (the home WiFi
// won't let the PC reach the deck): 0..99 % over 15 s, the Done flash for 1 s (no reboot), then
// "Update failed" at 40 % for 3 s. Spotify keeps polling underneath.
static ota::View otaDemo(uint32_t ms) {
    const uint32_t t = ms % 19000;
    if (t < 15000) return {otaview::Phase::Receiving, (int)(t / 150)};
    if (t < 16000) return {otaview::Phase::Done, 100};
    return {otaview::Phase::Failed, 40};
}
#endif

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

    // Screen mode: deck when playing (or paused under 30 s); a status screen when offline, when
    // Spotify has nothing, or when the song has stayed paused 30 s (util/idlewait).
    static idlewait::Clock idle;
    const bool idleNow = idle.update(st.status == PlaybackStatus::Paused,
                                     st.status == PlaybackStatus::Stopped, millis());
    int mode = 0;   // 0 = deck, 1 = offline, 2 = nothing playing
    if (!net::isOnline() || st.status == PlaybackStatus::Offline ||
        netplan::stale(millis(), st.lastPollOkMs, netplan::staleLimit(idleNow))) mode = 1;   // idle: 10 s polls
    else if (idleNow) mode = 2;
    static int lastMode = -1;
    // An update over WiFi (net/ota): its screen, nothing else. With the Green Hill Zone file also
    // its flash and "Update failed"; the battle box only while it receives, as before.
#ifdef OTA_DEMO
    const ota::View ov = otaDemo(millis());
#else
    const ota::View ov = ota::view();
#endif
    const bool artScreen = backdrop::width() > 0;
    if (ov.phase != otaview::Phase::Idle && (artScreen || ov.phase != otaview::Phase::Failed)) {
        static int shownPct = -1;
        static otaview::Phase shownPhase = otaview::Phase::Idle;
        // Whole screen when it appears, and after the Done flash (only the demo gets past it).
        const bool full = lastMode != -2 || (shownPhase == otaview::Phase::Done && ov.phase != shownPhase);
        if (!ui::drawShadowUpdate(s_tft, ov, millis(), full) && (full || ov.pct != shownPct))
            ui::drawUpdate(s_tft, ov.pct, full);
        shownPct = ov.pct;
        shownPhase = ov.phase;
        lastMode = -2;     // no screen's mode: the one it covered comes back in full
        delay(20);
        return;
    }
    if (lastMode == -2) lastMode = -1;

    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    static uint8_t shownGenre = genre::NONE;   // badge for the track on screen
    if (updateDayNight()) lastMode = -1;   // redraw the deck or the status screen in the new palette
    if (mode != 0) {
        static char shownText[80] = "", shownTitle[24] = "";
        static char title[24], detail[48];
        static uint32_t textAt = 0, shownAt = 0, shownKey = 0;   // shownAt: the screen appeared
        static linkstatus::Kind kind = linkstatus::Kind::Ok, shownKind = linkstatus::Kind::Ok;
        static uint32_t shownQrGen = 0;   // the phone link's QR on screen (s_qrGen)
        char text[80];   // title|detail: 24 + 1 + 48
        // Re-read the cause at most once a second: net::ssid() allocates a String.
        if (lastMode != mode || millis() - textAt >= 1000) {
            textAt = millis();
            strlcpy(title, "Waiting for a song...", sizeof title);
            detail[0] = '\0';
            kind = mode == 1 ? offlineText(st, title, sizeof title, detail, sizeof detail)
                             : linkstatus::Kind::Ok;
            // A phone that dropped off Spotify plays on unseen (the account has no active device
            // until the app is touched): now and then, how to wake it (util/idlewait).
            if (mode == 2 && lastMode == mode && idlewait::hintNow(millis() - shownAt))
                strlcpy(detail, idlewait::PHONE_HINT, sizeof detail);
        }
        statusscene::Look look = statusscene::lookFor(kind);
        look.night = theme::isNightActive();   // Sonic's sunset turns to night with the palette
        const qrcard::Qr* qr = mode == 2 ? phoneQr(lastMode != mode) : nullptr;   // the card, top left
        snprintf(text, sizeof text, "%s|%s", title, detail);
        if (lastMode != mode || kind != shownKind) {   // new screen: everything, once
            shownAt = millis();
            shownKey = statusscene::frameKey(0, look);
            shownQrGen = s_qrGen;
            ui::drawOffline(s_tft, title, detail, look, 0, qr);
        } else if (strcmp(text, shownText) != 0) {   // new text (countdown, phone hint)
            const char* shownDetail = strchr(shownText, '|');   // same two-line layout as before?
            if (strcmp(title, shownTitle) == 0 && detail[0] && shownDetail && shownDetail[1])
                ui::drawOfflineDetail(s_tft, detail);
            else ui::drawOfflineText(s_tft, title, detail);
        } else if (statusscene::frameKey(millis() - shownAt, look) != shownKey ||
                   s_qrGen != shownQrGen) {   // next frame, or the link's QR changed
            const uint32_t t = millis() - shownAt;   // Pokemon bob, bubble, ball; Sonic waiting
            if (s_qrGen != shownQrGen) {   // the card once more, or the scene back where it was
                shownQrGen = s_qrGen;
                ui::drawQrCard(s_tft, look, t, qr);
            }
            if (statusscene::frameKey(t, look) != shownKey) {   // only what moves (frameRect)
                shownKey = statusscene::frameKey(t, look);
                ui::drawOfflineScene(s_tft, look, t, false, qr);
            }
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
