// Copied from black/main.cpp's loop (screen.h).
#include "screen.h"
#include <Arduino.h>
#include <cstring>
#include "app_state.h"
#include "art.h"
#include "nowscreen.h"
#include "../board/canvas.h"
#include "../../black/core/shared.h"
#include "../../black/ui/lyricmsg.h"
#include "../../black/ui/theme.h"
#include "../../common/net/clock.h"
#include "../../common/net/wifi.h"
#include "../../util/black/genre.h"
#include "../../util/black/idlewait.h"
#include "../../util/black/interp.h"
#include "../../util/black/linkstatus.h"
#include "../../util/black/netplan.h"
#include "../../util/common/daynight.h"

namespace screen {
namespace {
constexpr uint32_t CD_FRAME_MS = 160;     // spinning CD + Poke Ball, ~6 fps
constexpr uint32_t PROGRESS_MS = 250;     // time, HP/EXP bars, dialogue box
constexpr uint32_t WALK_FRAME_MS = 120;   // walker ~8 fps
constexpr uint32_t MODE_CHECK_MS = 10000; // day/night check (18:00-06:00 local time)

bool s_ready = false;
bool s_whole = true;     // draw everything on the next tick
bool s_dirty = false;    // something was drawn: present it

void pushArtIfValid(uint32_t gen) {
    shared::Guard g;
    if (shared::artValidLocked(gen)) {
        canvas::blit(pokeui::ART_X, pokeui::ART_Y, art::W, art::H, art::bitmap());
        s_dirty = true;
    }
}

// The dialogue box for the track on screen: the lyric line, or a Pokemon-style status message.
void drawDialogue(TFT_eSPI& t, const AppState& view, uint32_t gen) {
    static lyricstatus::Status lastStatus = lyricstatus::Status::Searching;
    static uint32_t lastGen = 0, since = 0;
    static shared::LyricView lv;      // static: keep the loop stack small
    static lyricmsg::Out out;
    shared::lyricView(gen, view.progressMs, lv);
    const uint32_t now = millis();
    if (gen != lastGen || lv.status != lastStatus) {
        lastGen = gen;
        lastStatus = lv.status;
        since = now;
    }
    lyricmsg::In in{lv.status, now - since, view.progressMs, lv.firstLineMs, lv.line, view.pokeName, gen};
    lyricmsg::compose(in, out);
    if (pokeui::drawLyricArea(t, out.text, out.notes)) s_dirty = true;
}

bool updateDayNight(uint32_t now) {
    static uint32_t lastCheck = 0;
    static bool first = true;
    if (!first && now - lastCheck < MODE_CHECK_MS) return false;
    first = false;
    lastCheck = now;
    int m = 0;
    const bool night = netclock::minuteOfDay(&m) && daynight::isNight(m);
    if (night == theme::isNightActive()) return false;
    theme::setNight(night);
    return true;
}

// Nothing to show a song from: why, on the battle screen.
void drawStatus(TFT_eSPI& t, const AppState& st, int mode) {
    static char shown[96] = "";
    static AppState note;   // static: too big for the loop stack
    char title[24] = "Waiting for a song...";
    char detail[48] = "";
    if (mode == 1) {
        const bool wifiUp = net::isOnline();
        const linkstatus::Kind k = linkstatus::classify({wifiUp, true, st.lastPollCode, st.certRejected, st.loginFailed});
        char names[96];
        if (wifiUp) strlcpy(names, net::ssid().c_str(), sizeof names);
        else net::savedNetworks(names, sizeof names);
        linkstatus::describe(k, st.lastPollCode, linkstatus::retryInS(millis(), st.nextPollMs), names, title,
                             sizeof title, detail, sizeof detail);
    }
    char both[96];
    snprintf(both, sizeof both, "%s|%s", title, detail);
    if (!s_whole && strcmp(both, shown) == 0) return;
    strlcpy(shown, both, sizeof shown);
    note = AppState{};
    strlcpy(note.trackName, title, sizeof note.trackName);
    strlcpy(note.artist, "Pokedeck", sizeof note.artist);
    strlcpy(note.context, "-", sizeof note.context);
    strlcpy(note.deviceName, net::isOnline() ? net::deviceIp().c_str() : "offline", sizeof note.deviceName);
    strlcpy(note.pokeName, "Pikachu", sizeof note.pokeName);
    pokeui::drawNow(t, note, genre::NONE);
    pokeui::drawLyricArea(t, detail, false);
    Serial.printf("[screen] %s / %s\n", title, detail);
    s_dirty = true;
}
}  // namespace

bool begin() {
    s_ready = canvas::begin();
    return s_ready;
}

void redraw() { s_whole = true; }

void tick(uint32_t now) {
    if (!s_ready) return;
    TFT_eSPI& t = canvas::screen();
    static AppState st, view;   // static: too big for the loop stack
    shared::snapshot(st);
    view = st;   // interpolate progress for a smooth bar between polls
    view.progressMs = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying, now - st.lastPollMs);

    // The deck's screen while a song plays (or is paused under 30 s); else the status one.
    static idlewait::Clock idle;
    const bool idleNow = idle.update(st.status == PlaybackStatus::Paused, st.status == PlaybackStatus::Stopped, now);
    int mode = 0;   // 0 = deck, 1 = offline, 2 = waiting for a song
    if (!net::isOnline() || st.status == PlaybackStatus::Offline || st.status == PlaybackStatus::Unknown ||
        netplan::stale(now, st.lastPollOkMs, netplan::staleLimit(idleNow)))
        mode = 1;
    else if (idleNow) mode = 2;
    static int lastMode = -1;
    if (updateDayNight(now)) s_whole = true;
    if (mode != lastMode) s_whole = true;

    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    static uint8_t shownGenre = genre::NONE;
    static uint32_t animMs = 0, lastTick = 0;
    static int walkStep = 0;
    if (mode == 0 && st.isPlaying) animMs += now - lastTick;
    lastTick = now;

    if (mode != 0) {
        static uint32_t textAt = 0;
        if (s_whole || now - textAt >= 1000) {   // the cause, at most once a second
            textAt = now;
            drawStatus(t, st, mode);
        }
    } else if (s_whole || st.trackGen != shownGen) {
        if (st.trackGen != shownGen) {
            shownGenre = genre::NONE;
            shownGen = st.trackGen;
            walkerOn = false;
        }
        uint8_t cached;
        if (shared::takeGenre(shownGen, &cached)) shownGenre = cached;
        pokeui::drawNow(t, view, shownGenre);
        pushArtIfValid(shownGen);
        drawDialogue(t, view, shownGen);
        s_dirty = true;
    } else {
        // What the network task delivered since, and what moves.
        if (shared::takeArt(shownGen)) pushArtIfValid(shownGen);
        if (shared::takeWalker(shownGen)) walkerOn = true;
        uint8_t g;
        if (shared::takeGenre(shownGen, &g)) {
            shownGenre = g;
            pokeui::drawArtistRow(t, view, g);
            s_dirty = true;
        }
        static char topSig[96] = "";
        char sig[96];
        snprintf(sig, sizeof sig, "%s|%s|%d|%d|%d", st.deviceName, st.deviceType, static_cast<int>(st.shuffle),
                 st.repeat, static_cast<int>(st.isPlaying));
        if (strcmp(sig, topSig) != 0) {
            strlcpy(topSig, sig, sizeof topSig);
            pokeui::drawTopStrip(t, st);
            s_dirty = true;
        }
        static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0;
        static int cdFrame = 0, ballFrame = 0;
        if (st.isPlaying && now - lastCd >= CD_FRAME_MS) {
            lastCd = now;
            pokeui::drawCdFrame(t, cdFrame = (cdFrame + 1) & 3);
            pokeui::drawPokeballFrame(t, ballFrame = (ballFrame + 1) % 8);
            s_dirty = true;
        }
        if (now - lastDraw >= PROGRESS_MS) {
            lastDraw = now;
            pokeui::drawProgressRegion(t, view);
            drawDialogue(t, view, shownGen);
            s_dirty = true;
        }
        if (walkerOn && now - lastWalk >= WALK_FRAME_MS) {
            lastWalk = now;
            if (st.isPlaying) walkStep++;
            pokeui::drawWalker(view, animMs, walkStep);
            s_dirty = true;
        }
    }
    lastMode = mode;
    s_whole = false;
    static uint32_t presentedAt = 0;
    if (s_dirty && now - presentedAt >= 40) {   // at most 25 frames a second
        presentedAt = now;
        s_dirty = false;
        canvas::present();
    }
}
}  // namespace screen
