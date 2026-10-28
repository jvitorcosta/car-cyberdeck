#include "nettask.h"
#include <Arduino.h>
#include <esp_system.h>
#include "shared.h"
#include "mem.h"
#include "../../common/net/wifi.h"
#include "../../common/net/clock.h"
#include "../../common/net/ota.h"
#include "../../common/core/watchdog.h"
#include "../spotify/client.h"
#include "../images/art.h"
#include "../audio/musicsvc.h"
#include "../images/walksprite.h"
#include "../lyrics/lrclib.h"
#include "../pokemon/pick.h"
#include "../pokemon/dex.h"
#include "../../util/black/netplan.h"
#include "../../util/black/quietboot.h"
#include "../../util/black/genre.h"
#include "../../util/black/genrebadge.h"
#include "../genre/apple.h"
#include "../../util/lights/ledlook.h"
#include "../../util/black/songmood.h"
#include "../../util/wire/songpalette.h"

// History (README "Design & performance history"): these calls used to run inline in
// loop(). Each is a fresh TLS handshake (poll ~1.5 s, player ~1.4 s, PokeAPI 4.2 s, art
// 2.5 s, lyrics 3.8 s, walk sheet 3.5 s), so the display froze during every poll and a
// track change took ~19 s to appear. Here they block only this task.
namespace nettask {

static AppState s_st{};            // this task's private copy; published after changes
static netplan::TrackGen s_gen;
static netplan::Work s_work{};
static netplan::LinkGate s_link;   // "No signal" only after several failed polls in a row
static netplan::PollBackoff s_backoff;   // Spotify 429: poll less often until it passes
static netplan::IdlePoll s_idle;         // "Waiting for a song...": poll every 10 s, not 4 s
static netplan::Health s_health;   // pause optional work / restart when polls keep failing
static netplan::WalkRetry s_walkRetry;       // PNG decoder didn't fit: retry after the polls
static netplan::LyricsRetry s_lyricsRetry;   // 5 s / 20 s retries after LRCLIB hiccups
static netplan::ArtRetry s_artRetry;         // a failed cover download: one more try, 5 s later
static genre::Cache s_genres;   // last 32 artists' badges: repeats cost no request
static uint32_t s_failsAtOk = 0;   // mem::failTotal() at the last good poll
static bool s_spotifyReady = false;   // WiFi + spclient::begin() done
static int s_prefetchDex = 0;      // dex whose walker sits in the staged slot
static uint32_t s_t0 = 0;

// The self-heal restart's note (util/quietboot): survives the restart, not a power-on.
RTC_NOINIT_ATTR static uint32_t s_noteMagic;
static bool s_noteRead = false, s_quiet = false;

bool quietBoot() {
    if (!s_noteRead) {
        s_noteRead = true;
        const bool sw = esp_reset_reason() == ESP_RST_SW;
        s_quiet = quietboot::quiet(s_noteMagic, sw);
        s_noteMagic = 0;   // read once: an update's restart later on plays the opening again
    }
    return s_quiet;
}

// Restarts the deck, quietly: the next boot skips the opening (util/quietboot).
static void selfHealRestart() {
    s_noteMagic = quietboot::MAGIC;
    delay(200);
    ESP.restart();
}

constexpr uint32_t WIFI_RETRY_MS = 5000;    // WiFi not up at boot: retry period
constexpr uint32_t LOOP_DELAY_MS = 20;      // yield between work items

static void tick() { s_t0 = millis(); }
static void tock(const char* what) {
    Serial.printf("[net] %s %lums\n", what, (unsigned long)(millis() - s_t0));
}

// Music mode's colour for the song (util/songmood), decided here, where the title, artist, album,
// playlist and genre are: at the track change, again once the cover is in (or failed) and once the
// genre is known. One [colour] line per song, once neither is pending.
static songpalette::Rgb s_cover[3];
static int s_coverCount = 0;
static bool s_coverIn = false;           // s_cover holds this song's cover
static uint32_t s_colourLogged = 0;      // the gen last logged

static void decideColour(uint32_t gen) {
    const uint8_t badge = shared::genreNow(gen);
    const bool playlist = spclient::fromPlaylist();
    const songmood::Song song{s_st.trackName, s_st.artist, s_st.album, playlist ? s_st.context : "",
                              spclient::playlistDescription()};
    const songmood::Choice c = songmood::choose(song, s_cover, s_coverIn ? s_coverCount : 0, badge);
    musicsvc::onChoice(gen, c, s_coverIn);
    if (s_work.art || s_work.genre || s_colourLogged == gen) return;
    s_colourLogged = gen;
    char cover[24];
    snprintf(cover, sizeof cover, "%s", s_coverIn ? " grey" : " none");   // grey: art without a usable colour
    for (int i = 0; s_coverIn && i < s_coverCount && i < 3; ++i)
        snprintf(cover + 7 * i, sizeof cover - 7 * i, " %02X%02X%02X", s_cover[i].r, s_cover[i].g, s_cover[i].b);
    int minute = -1;
    if (!netclock::minuteOfDay(&minute)) minute = -1;
    uint8_t cap, depth;
    songpalette::timeOfDay(minute, &cap, &depth);
    const bool moodRule = c.rule == songmood::Rule::PlaylistMood || c.rule == songmood::Rule::GenreMood;
    char line[512];   // built whole and printed once (printf in pieces came out cut); not in a TLS call
    snprintf(line, sizeof line, "[colour] \"%s\" | %s | %s | %s%s%s | genre %s | cover%s | %s%s%s%s%s | -> %s cap %u\n",
             s_st.trackName, s_st.artist, s_st.album, playlist ? "\"" : "", playlist ? s_st.context : "-",
             playlist ? "\"" : "", badge == genre::NONE ? "NONE" : genrebadge::at(badge).label, cover,
             songmood::ruleName(c.rule), moodRule ? " " : "", moodRule ? c.mood : "", c.genre ? ", genre " : "",
             c.genre ? c.genre : "", ledlook::SWATCHES[c.swatch].name, cap);
    Serial.print(line);
}

static void onTrackChange() {
    s_st.trackGen = s_gen.gen();
    bool prefetched = s_prefetchDex > 0 && walk::stagedDex() == s_prefetchDex;
    pick::choose(s_st, prefetched ? s_prefetchDex : dex::fromRandom(esp_random()));
    s_prefetchDex = 0;
    shared::publish(s_st);                 // UI shows the new title + Pokemon right away
    if (prefetched) {
        { shared::Guard g; walk::promote(); }
        shared::postWalker(s_st.trackGen); // walker appears together with the title
    }
    s_work = netplan::freshWork(prefetched);
    s_lyricsRetry.reset();               // UI shows "searching" until the first result
    s_artRetry.reset();
    musicsvc::onTrack(s_st.trackGen);
    uint8_t badge;
    if (s_genres.find(s_st.artist, &badge)) {   // known artist (or none): no request
        netplan::done(s_work, netplan::Step::Genre);
        if (badge != genre::NONE) shared::postGenre(s_st.trackGen, badge);
    }
    s_coverIn = false;
    decideColour(s_st.trackGen);         // music mode's colour without the cover, until it comes
    Serial.printf("[net] track gen=%u%s\n", (unsigned)s_st.trackGen,
                  prefetched ? " (prefetched walker)" : "");
}

// Runs one step; false when the step must run again later (lyrics retry).
static bool doStep(netplan::Step step) {
    const uint32_t gen = s_st.trackGen;
    switch (step) {
        case netplan::Step::Walk: {
            tick();
            bool ok = walk::loadPmd(s_st.pokedexNum) ||
                      (!walk::outOfMemory() && walk::loadFallback(s_st.pokeSpriteUrl, s_st.pokedexNum));
            if (!s_walkRetry.onResult(ok, walk::outOfMemory())) {
                tock("walk (no heap, retry later)");
                return false;
            }
            tock("walk");
            if (ok) {
                { shared::Guard g; walk::promote(); }
                shared::postWalker(gen);
            }
            break;
        }
        case netplan::Step::Art: {
            tick();
            shared::artInvalidate();          // UI stops pushing the bitmap before we overwrite it
            bool ok = art::fetch(s_st.albumArtUrl);
            if (ok) {
                // music mode's cover colours (~8.5k pixels), before anything can rewrite the bitmap
                s_coverCount = songpalette::fromCover(art::bitmap(), art::W, art::H, s_cover);
                s_coverIn = true;
                shared::postArt(gen);
            }
            tock(ok ? "art" : "art failed");
            mem::log("art");
            if (!s_artRetry.onResult(ok, s_st.albumArtUrl[0] != '\0', millis())) return false;   // once more, later
            break;
        }
        case netplan::Step::Lyrics: {
            tick();
            shared::lyricsInvalidate();       // UI stops reading the arena before we overwrite it
            lyricbuf::Lyrics* arena = lyricsvc::arena();   // nullptr: no memory yet, retry later
            lyricstatus::Result r = arena ? lyricsvc::fetchInto(s_st, *arena, s_lyricsRetry.attempts() + 1)
                                          : lyricstatus::Result::TempError;
            bool final = s_lyricsRetry.onResult(r, millis());
            if (r == lyricstatus::Result::Synced || r == lyricstatus::Result::Plain)
                shared::postLyrics(gen);      // lines first, then the status that points at them
            shared::postLyricsStatus(gen, lyricstatus::statusFor(r, final));
            tock(final ? "lyrics" : "lyrics (retry later)");
            mem::log("lyrics");
            if (!final) return false;
            break;
        }
        case netplan::Step::Prefetch: {
            // The Pokemon is random and independent of the song, so the next one can be
            // chosen and its walker loaded now, while this song plays.
            int n = dex::fromRandom(esp_random());
            char url[160];
            dex::spriteUrl(n, url, sizeof(url));
            tick();
            bool ok = walk::loadPmd(n) || (!walk::outOfMemory() && walk::loadFallback(url, n));
            if (!s_walkRetry.onResult(ok, walk::outOfMemory())) {
                tock("prefetch (no heap, retry later)");
                return false;
            }
            s_prefetchDex = ok ? n : 0;
            tock(ok ? "prefetch" : "prefetch failed");
            break;
        }
        case netplan::Step::Genre: {
            tick();
            uint32_t id = 0;
            int rc = 0;
            applegenre::Result r = applegenre::lookup(s_st.artist, &id, &rc);
            if (r == applegenre::Result::Found) {
                uint8_t badge = genrebadge::forGenreId(id);
                s_genres.store(s_st.artist, badge);
                shared::postGenre(gen, badge);
                Serial.printf("[genre] \"%s\": %u -> %s\n", s_st.artist, (unsigned)id,
                              genrebadge::at(badge).label);
            } else if (r == applegenre::Result::None) {
                s_genres.store(s_st.artist, genre::NONE);   // Apple doesn't know: don't ask again
                Serial.printf("[genre] \"%s\": none\n", s_st.artist);
            } else {
                Serial.printf("[genre] \"%s\": error rc=%d\n", s_st.artist, rc);   // not cached
            }
            tock("genre");
            break;
        }
        case netplan::Step::None:
            break;
    }
    netplan::done(s_work, step);
    if (step == netplan::Step::Art || step == netplan::Step::Genre) decideColour(gen);
    return true;
}

static void run(void*) {
    uint32_t lastPoll = 0;
    bool first = true;
    // Watched (common/core/watchdog): a hang here also stops Health, the self-heal below, so the
    // watchdog's reset is the backstop. Fed each pass, after the WiFi check and before each request.
    watchdog::watch();
    for (;;) {
        watchdog::feed();
        if (!s_spotifyReady) {                 // log in once WiFi is up (setup's, or a late hotspot)
            if (net::isOnline() || net::connectAny()) {
                spclient::begin();
                netclock::begin();
                s_spotifyReady = true;
                mem::log("spotify login");
            } else {
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
                continue;
            }
        }
        if (ota::active()) {                   // an update has the WiFi: no requests beside it
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        net::loop();
        watchdog::feed();   // a reconnect can take ~25 s
        if (net::isOnline()) ota::onOnline();   // a pending update: confirmed once up a minute, online
        uint32_t now = millis();
        // WiFi down: keep the normal pace (polls fail fast) so Health's 15-minute backstop runs
        // even during an hours-long Retry-After wait.
        const uint32_t every = net::isOnline() ? s_idle.every(s_backoff.interval()) : netplan::PollBackoff::BASE_MS;
        if (first || now - lastPoll >= every) {
            first = false;
            lastPoll = now;
            PlaybackStatus before = s_st.status;
            tick();
            spclient::poll(s_st);
            tock("poll");
            const uint32_t was = s_idle.every(s_backoff.interval());
            s_backoff.onPoll(s_st.lastPollCode, s_st.retryAfterS);
            bool ok = s_st.status != PlaybackStatus::Offline;
            if (ok) { s_st.lastPollOkMs = millis(); s_failsAtOk = mem::failTotal(); ota::onGoodPoll(); }
            // Restart only helps a starved heap: allocations failed since the last good poll,
            // or no block big enough for TLS. A dead zone or a 401/429 is not that.
            bool starved = s_st.lastPollCode != 429 &&   // rate limited: a reboot only asks again
                           (mem::failTotal() != s_failsAtOk || mem::byteLargest() < netplan::TLS_NEED);
            if (!ok) {
                mem::log("poll failed");
                net::diagnose(s_st.lastPollCode, s_st.lastPollOkMs ? millis() - s_st.lastPollOkMs : 0);
            }
            const bool wifiUp = net::isOnline();   // one reading for the decision and the log
            if (s_health.onPoll(ok, wifiUp, starved, now) == netplan::Health::Action::Restart) {
                mem::log("restart");
                Serial.printf("[net] self-heal restart (%s), quietly\n",
                              wifiUp ? "heap starved, no good poll for 180 s" : "WiFi down for 15 min");
                selfHealRestart();
            }
            // isolated failure: keep last state; a 429 is an answer, shown at once (next poll may be hours away)
            if (!s_link.update(ok, s_st.lastPollCode == 429) && !ok) s_st.status = before;
            // a failed poll goes back to 4 s: its retry is due at once, and LinkGate counts at that pace
            s_idle.onPoll(ok && s_st.status == PlaybackStatus::Paused, ok && s_st.status == PlaybackStatus::Stopped, now);
            const uint32_t next = s_idle.every(s_backoff.interval());
            s_st.nextPollMs = (now + next) | 1;   // the status screen's countdown; never 0
            if (next != was)
                Serial.printf("[net] poll every %lus (HTTP %d)\n", (unsigned long)(next / 1000), s_st.lastPollCode);
            const char* id = s_st.trackUri[0] ? s_st.trackUri : s_st.trackName;   // URI: same-title songs differ
            if (s_gen.update(id)) onTrackChange();
            else shared::publish(s_st);
        } else {
            netplan::Step step = netplan::next(s_work, s_lyricsRetry.ready(now), s_artRetry.ready(now));
            static bool deferred = false;
            if (step != netplan::Step::None) {
                bool optional = step != netplan::Step::Art;
                if (!(optional && s_health.optionalPaused()) &&
                    netplan::canRun(step, (unsigned)mem::byteLargest())) {
                    deferred = false;
                    doStep(step);
                } else if (!deferred) {            // log once per deferral, retry after polls
                    deferred = true;
                    mem::log("defer step");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}

// Stack: 10 KB (was 16 KB; the 6 KB went back to the heap, which album-art and TLS buffers
// were short of). Lowest free seen ~3.7 KB, i.e. ~6.3 KB used by TLS + JSON + the lyrics
// reader ([mem] stackfree=, README "Design & performance history" item 12).
void start() {
    xTaskCreatePinnedToCore(run, "net", 10240, nullptr, 1, nullptr, 0);
}

}
