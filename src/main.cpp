// The UI loop (core 1): picks the screen each pass and lets it draw. setup() is in app/boot.cpp;
// the screens are ui/deckview (now playing) and ui/statusview (offline, waiting for a song).
#include <Arduino.h>
#include "app_state.h"
#include "app/display.h"
#include "core/shared.h"
#include "net/wifi.h"
#include "net/clock.h"
#include "net/ota.h"
#include "audio/greeting.h"
#include "ui/backlight.h"
#include "ui/bootscene.h"
#include "ui/deckview.h"
#include "ui/otascreen.h"
#include "ui/statusview.h"
#include "ui/theme.h"
#include "util/daynight.h"
#include "util/idlewait.h"
#include "util/interp.h"
#include "util/netplan.h"

constexpr uint32_t STALE_MS = 20000;      // "No signal" once the last good poll is this old
constexpr uint32_t MODE_CHECK_MS = 10000; // day/night check (18:00-06:00 local time)

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
    int mode = 0;   // 0 = deck, 1 = offline, 2 = nothing playing (statusview::Mode)
    if (!net::isOnline() || st.status == PlaybackStatus::Offline ||
        netplan::stale(millis(), st.lastPollOkMs, STALE_MS)) mode = statusview::OFFLINE;
    else if (idleNow) mode = statusview::WAITING;
    static int lastMode = -1;
    if (ota::active()) {   // an update over WiFi: its progress, nothing else (net/ota)
        static int shownPct = -1;
        const int pct = ota::progress();
        if (lastMode != -2 || pct != shownPct) ui::drawUpdate(app::tft(), pct, lastMode != -2);
        shownPct = pct;
        lastMode = -2;     // no screen's mode: the one it covered comes back in full
        delay(20);
        return;
    }
    if (lastMode == -2) lastMode = -1;
    statusview::musicBox(mode == statusview::WAITING && !greeting::tunesMuted());

    if (updateDayNight()) lastMode = -1;   // redraw the deck or the status screen in the new palette
    if (mode != 0) statusview::draw(st, (statusview::Mode)mode, lastMode != mode);
    else deckview::draw(st, view, lastMode != 0);
    lastMode = mode;
    delay(10);
}
