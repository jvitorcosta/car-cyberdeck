#pragma once
#include <cstdint>
#include "../wire/lyricstatus.h"
#include "idlewait.h"
// What the network task does next. PURE, host-tested.
// The task polls Spotify on a fixed cadence and, between polls, does ONE per-track
// work step at a time so a slow download never delays the next poll by more than
// one step (see README "Design & performance history").
namespace netplan {

// Track-change detection from successive now-playing ids (the track URI; the name only when
// there is no URI), so two songs with the same title still count as a change.
class TrackGen {
public:
    // True (and the generation increments) when `track` is non-empty and differs
    // from the last non-empty id seen.
    bool update(const char* track);
    uint32_t gen() const { return gen_; }
private:
    char last_[96] = {0};
    uint32_t gen_ = 0;
};

// "No signal" debounce. A single failed poll (TLS hiccup, HTTP -1) used to flip the
// whole deck to the No-signal screen; only a run of failures means the link is down.
class LinkGate {
public:
    static constexpr int FAILS_FOR_DOWN = 3;
    // Feed each poll result; returns true while the link should be shown as down. definite: a
    // failure that is an answer, not a blip (Spotify's 429), shows as down at once.
    bool update(bool ok, bool definite = false) {
        fails_ = ok ? 0 : definite && fails_ < FAILS_FOR_DOWN ? FAILS_FOR_DOWN : fails_ + 1;
        return fails_ >= FAILS_FOR_DOWN;
    }
private:
    int fails_ = 0;
};

// Poll interval for the now-playing request. Spotify answers 429 when the app (shared by
// every deck using it) asks too often; polling on at 4 s keeps the limit running. Each 429
// doubles the wait up to a minute; when the 429 says how long to wait (Retry-After, seconds)
// the deck stays quiet that long instead (capped at a day). Any other answer returns to the
// base interval.
class PollBackoff {
public:
    static constexpr uint32_t BASE_MS = 4000;
    static constexpr uint32_t MAX_MS = 60000;
    static constexpr uint32_t RETRY_AFTER_MAX_MS = 24UL * 3600 * 1000;
    void onPoll(int httpCode, int retryAfterS = -1) {
        if (httpCode == 200 || httpCode == 204) { interval_ = BASE_MS; return; }   // episode over
        if (httpCode != 429) {   // timeout / 5xx: not proof the limit is gone, nor a reason to wait hours
            if (interval_ > MAX_MS) interval_ = MAX_MS;
            return;
        }
        interval_ = interval_ >= MAX_MS / 2 ? MAX_MS : interval_ * 2;
        if (retryAfterS > 0) {
            uint32_t ms = (uint32_t)retryAfterS >= RETRY_AFTER_MAX_MS / 1000 - 1
                              ? RETRY_AFTER_MAX_MS
                              : (uint32_t)retryAfterS * 1000 + 1000;   // + 1 s margin
            if (ms > interval_) interval_ = ms;
        }
    }
    uint32_t interval() const { return interval_; }
    bool active() const { return interval_ != BASE_MS; }   // backing off: skip other Spotify calls
private:
    uint32_t interval_ = BASE_MS;
};

// Spotify login: the access-token refresh at accounts.spotify.com (spotify/client). A refused
// refresh token (4xx: revoked, expired, the client secret rotated) or an accounts outage (5xx) was
// tried again at every poll: a TLS handshake every 4 s for as long as it lasted. Now each answer
// that isn't a token doubles the wait before the next try, BASE_MS up to MAX_MS; a token resets it.
// No answer at all (< 0: no WiFi, DNS, TLS) is the network's fault: the login keeps the poll's pace
// then, so it follows the network back at once, and a wait already running is left as it is, with
// its answer (a blip during a refused login's wait keeps "Login failed" on screen).
class LoginBackoff {
public:
    static constexpr uint32_t BASE_MS = 4000;
    static constexpr uint32_t MAX_MS = 300000;
    bool due(uint32_t nowMs) const { return wait_ == 0 || nowMs - at_ >= wait_; }   // wrap-safe
    void onResult(int httpCode, uint32_t nowMs) {
        if (httpCode < 0) {
            if (wait_ == 0) last_ = httpCode;
            return;
        }
        last_ = httpCode;
        if (httpCode == 200) { wait_ = 0; return; }
        wait_ = wait_ == 0 ? BASE_MS : wait_ >= MAX_MS / 2 ? MAX_MS : wait_ * 2;
        at_ = nowMs;
    }
    // Spotify refused the refresh token (a 4xx, not a 429): only tools/spotify_auth.py fixes that.
    bool refused() const { return last_ >= 400 && last_ < 500 && last_ != 429; }
    int lastCode() const { return last_; }   // the last try's answer (< 0: none)
    uint32_t waitMs() const { return wait_; }   // 0: no wait
private:
    uint32_t wait_ = 0, at_ = 0;
    int last_ = 0;
};

// Idle polling. While the deck shows "Nothing playing" (idlewait: no device or no song at once, a
// pause after PAUSE_MS) every 4 s poll got the same answer (449 of them in an idle half hour), so
// the poll waits IDLE_MS instead; a song started on the phone shows within that. Any other poll
// (playing, a short pause, a failure: LinkGate and Health keep their pace) returns to the
// backoff's interval, and a 429 wait longer than IDLE_MS still wins.
class IdlePoll {
public:
    static constexpr uint32_t IDLE_MS = 10000;
    // Feed each poll's resulting state: paused = a song there but not playing; stopped = nothing.
    void onPoll(bool paused, bool stopped, uint32_t nowMs) { idle_ = clock_.update(paused, stopped, nowMs); }
    bool idle() const { return idle_; }
    uint32_t every(uint32_t backoffMs) const { return idle_ && backoffMs < IDLE_MS ? IDLE_MS : backoffMs; }
private:
    idlewait::Clock clock_;
    bool idle_ = false;
};

// Retry-After header value in seconds (at most a week); -1 when missing or not plain digits
// (the HTTP-date form).
int parseRetryAfter(const char* v);

// Self-healing ladder. A heap so fragmented that TLS can't allocate never recovered by itself
// (README "Design & performance history"), so:
//  - 2 failed polls in a row with WiFi up -> pause the optional downloads;
//  - restart only on memory evidence (memStarved: failed allocations since the last good poll,
//    or the largest block below TLS_NEED) after 180 s without a good poll — a dead zone or a
//    401/429 can't be fixed by rebooting, and must not become a reboot loop;
//  - WiFi down for 15 min straight -> restart (backstop if the WiFi driver can't come back). The
//    restart is quiet (util/quietboot: no opening). Health only runs once the task has been
//    online, so it is one restart per outage: after it, a boot with no WiFi stays in the task's
//    connect loop (retrying, no reboot) until WiFi is up again.
class Health {
public:
    enum class Action { None, PauseOptional, Restart };
    static constexpr int FAILS_TO_PAUSE = 2;
    static constexpr uint32_t RESTART_AFTER_MS = 180000;
    static constexpr uint32_t WIFI_DOWN_RESTART_MS = 900000;
    Action onPoll(bool ok, bool wifiUp, bool memStarved, uint32_t nowMs) {
        if (!started_) { started_ = true; lastOk_ = nowMs; }
        if (ok) {
            fails_ = 0; lastOk_ = nowMs; paused_ = false; wifiDown_ = false;
            return Action::None;
        }
        if (!wifiUp) {
            lastOk_ = nowMs;   // the memory clock only runs while WiFi is up
            if (!wifiDown_) { wifiDown_ = true; downSince_ = nowMs; }
            return nowMs - downSince_ >= WIFI_DOWN_RESTART_MS ? Action::Restart : Action::None;
        }
        if (wifiDown_) { wifiDown_ = false; lastOk_ = nowMs; }   // WiFi back: memory clock restarts
        ++fails_;
        if (memStarved && nowMs - lastOk_ >= RESTART_AFTER_MS) return Action::Restart;
        if (fails_ >= FAILS_TO_PAUSE) { paused_ = true; return Action::PauseOptional; }
        return Action::None;
    }
    bool optionalPaused() const { return paused_; }
private:
    int fails_ = 0;
    uint32_t lastOk_ = 0;
    uint32_t downSince_ = 0;
    bool started_ = false;
    bool paused_ = false;
    bool wifiDown_ = false;
};

// True when a successful poll happened (lastOkMs != 0) more than limitMs ago (wrap-safe).
inline bool stale(uint32_t nowMs, uint32_t lastOkMs, uint32_t limitMs) {
    return lastOkMs != 0 && nowMs - lastOkMs > limitMs;
}

// "No signal" once the last good poll is this old. While idle (IdlePoll) the next poll only
// starts IDLE_MS after the last one, so one slow failure (8 s read timeout, more with a connect
// and handshake) plus its 4 s retry would cross 20 s: the limit grows by the extra wait.
constexpr uint32_t STALE_MS = 20000;
inline uint32_t staleLimit(bool idle) {
    return idle ? STALE_MS + (IdlePoll::IDLE_MS - PollBackoff::BASE_MS) : STALE_MS;
}

enum class Step { None, Walk, Art, Lyrics, Prefetch, Genre };
struct Work { bool walk, art, lyrics, prefetch, genre; };   // true = still to do

// Work list for a new track. walkerReady = a prefetched walker was promoted.
Work freshWork(bool walkerReady);
// Next step in priority order: Art, Lyrics, Genre, Walk, Prefetch; None when all done.
// Art and lyrics are what the listener waits for; the walker is usually prefetched.
// lyricsReady / artReady false (waiting to retry): that step is skipped for now.
Step next(const Work& w, bool lyricsReady = true, bool artReady = true);
void done(Work& w, Step s);

// Memory gate (largest free byte-addressable block, mem::byteLargest()). mbedTLS needs a
// 16.7 KB input buffer plus ~3 KB per handshake, so TLS steps wait below TLS_NEED. Walker
// downloads use the fixed scratch buffer, so they need no more than that. Art is plain HTTP.
constexpr unsigned TLS_NEED = 20000;
constexpr unsigned ART_NEED = 6000;    // plain-HTTP art: socket + tjpgd chunks only
bool canRun(Step s, unsigned largest);

// Lyrics retries: a temporary error (HTTP 5xx/429, connect/TLS failure, timeout, stalled read)
// retries after RETRY1_MS, then RETRY2_MS; the third gives up. Other results are final.
// LRCLIB returned 503 on several requests while lyrics went missing (spec 2026-10-07).
class LyricsRetry {
public:
    static constexpr uint32_t RETRY1_MS = 5000;
    static constexpr uint32_t RETRY2_MS = 20000;
    void reset() { attempts_ = 0; waiting_ = false; }   // new track
    // Records one attempt; true when no further attempt will be made for this track.
    bool onResult(lyricstatus::Result r, uint32_t nowMs);
    bool ready(uint32_t nowMs) const { return !waiting_ || nowMs - since_ >= wait_; }   // wrap-safe
    int attempts() const { return attempts_; }
private:
    int attempts_ = 0;
    bool waiting_ = false;
    uint32_t since_ = 0, wait_ = 0;
};

// Walker loads: the PNG decoder (~45 KB) is allocated only for the decode. When it doesn't fit,
// the step runs again after the next polls, MAX_TRIES attempts in all; other results are final.
class WalkRetry {
public:
    static constexpr int MAX_TRIES = 3;
    // Records one attempt; true when the step is finished (no further attempt).
    bool onResult(bool ok, bool outOfMemory);
private:
    int tries_ = 0;
};

// Album art: a failed download (timeout, 5xx) is tried again WAIT_MS later (a poll in between: a
// fast failure retried at once fails the same way), MAX_TRIES attempts in all; success or no URL
// (some podcasts) is final. Until ready(), next() runs the other steps. reset() on a new track.
class ArtRetry {
public:
    static constexpr int MAX_TRIES = 2;
    static constexpr uint32_t WAIT_MS = 5000;
    void reset() { tries_ = 0; waiting_ = false; }
    // Records one attempt; true when the step is finished (no further attempt).
    bool onResult(bool ok, bool hasUrl, uint32_t nowMs) {
        if (!ok && hasUrl && ++tries_ < MAX_TRIES) {
            waiting_ = true;
            since_ = nowMs;
            return false;
        }
        reset();
        return true;
    }
    bool ready(uint32_t nowMs) const { return !waiting_ || nowMs - since_ >= WAIT_MS; }   // wrap-safe
private:
    int tries_ = 0;
    bool waiting_ = false;
    uint32_t since_ = 0;
};
}
