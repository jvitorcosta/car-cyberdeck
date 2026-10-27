#include <unity.h>
#include <initializer_list>
#include "../../src/util/netplan.h"
#include "../../src/util/lyricstatus.h"

using netplan::Step;
void setUp() {}
void tearDown() {}

void test_track_changes_bump_generation() {
    netplan::TrackGen g;
    TEST_ASSERT_EQUAL_UINT32(0, g.gen());
    TEST_ASSERT_TRUE(g.update("Song A"));      // first track
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
    TEST_ASSERT_FALSE(g.update("Song A"));     // same track
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
    TEST_ASSERT_TRUE(g.update("Song B"));      // new track
    TEST_ASSERT_EQUAL_UINT32(2, g.gen());
}
void test_empty_name_is_ignored_and_keeps_last() {
    netplan::TrackGen g;
    g.update("Song A");
    TEST_ASSERT_FALSE(g.update(""));
    TEST_ASSERT_FALSE(g.update(nullptr));
    TEST_ASSERT_FALSE(g.update("Song A"));   // resumed same song: no new Pokemon
    TEST_ASSERT_EQUAL_UINT32(1, g.gen());
}
void test_order_without_prefetched_walker() {
    // Art and lyrics first: they are what the user waits for; then the small genre lookup;
    // the walker is usually prefetched, and when it isn't it can follow.
    const netplan::Work idle{};
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(idle));
    netplan::Work w = netplan::freshWork(false);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w));
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Genre, (int)netplan::next(w));
    netplan::done(w, Step::Genre);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
    netplan::done(w, Step::Walk);
    TEST_ASSERT_EQUAL_INT((int)Step::Prefetch, (int)netplan::next(w));
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w));
}
void test_prefetched_walker_skips_walk_step() {
    netplan::Work w = netplan::freshWork(true);
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w));
    int steps = 0;
    for (Step s = netplan::next(w); s != Step::None; s = netplan::next(w)) {
        TEST_ASSERT_TRUE(s != Step::Walk);
        TEST_ASSERT_TRUE(++steps <= 5);
        netplan::done(w, s);
    }
    TEST_ASSERT_EQUAL_INT(4, steps);                    // Art, Lyrics, Genre, Prefetch
}
void test_link_stays_up_through_isolated_failures() {
    netplan::LinkGate g;
    TEST_ASSERT_FALSE(g.update(false));   // one failed poll is not "no signal"
    TEST_ASSERT_FALSE(g.update(false));
    TEST_ASSERT_FALSE(g.update(true));    // success resets the count
    TEST_ASSERT_FALSE(g.update(false));
    TEST_ASSERT_FALSE(g.update(false));
}
void test_link_down_after_three_consecutive_failures() {
    netplan::LinkGate g;
    g.update(false); g.update(false);
    TEST_ASSERT_TRUE(g.update(false));
    TEST_ASSERT_TRUE(g.update(false));
    TEST_ASSERT_FALSE(g.update(true));    // first good poll clears it
}

void test_link_down_at_once_on_a_definite_failure() {
    // A 429 is Spotify's answer, not a blip, and the next poll may be an hour away.
    netplan::LinkGate g;
    TEST_ASSERT_TRUE(g.update(false, true));
    TEST_ASSERT_TRUE(g.update(false));      // still down
    TEST_ASSERT_FALSE(g.update(true));
    TEST_ASSERT_FALSE(g.update(false));     // an ordinary failure is debounced again
}
void test_can_run_gates_on_largest_block() {
    using netplan::canRun;
    TEST_ASSERT_TRUE(canRun(Step::Art, 6000));          // plain HTTP: small
    TEST_ASSERT_FALSE(canRun(Step::Art, 5999));
    TEST_ASSERT_TRUE(canRun(Step::Lyrics, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Lyrics, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::Genre, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Genre, netplan::TLS_NEED - 1));
    // Walker downloads use the fixed scratch buffer (no malloc): they need only TLS.
    TEST_ASSERT_TRUE(canRun(Step::Walk, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Walk, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::Prefetch, netplan::TLS_NEED));
    TEST_ASSERT_FALSE(canRun(Step::Prefetch, netplan::TLS_NEED - 1));
    TEST_ASSERT_TRUE(canRun(Step::None, 0));
}
using Act = netplan::Health::Action;
void test_health_two_failures_pause_optional_until_success() {
    netplan::Health h;
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(true, true, false, 1000));   // success: nothing to do
    TEST_ASSERT_FALSE(h.optionalPaused());
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, true, false, 5000));
    TEST_ASSERT_EQUAL_INT((int)Act::PauseOptional, (int)h.onPoll(false, true, false, 9000));
    TEST_ASSERT_TRUE(h.optionalPaused());
    h.onPoll(true, true, false, 13000);
    TEST_ASSERT_FALSE(h.optionalPaused());
}
void test_health_restart_after_180s_when_memory_starved() {
    netplan::Health h;
    h.onPoll(true, true, false, 1000);
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, true, 1000 + 179000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, true, 1000 + 180000));
}
// The restart clock spans millis() wrapping (every ~49.7 days of uptime).
void test_health_restart_clock_survives_millis_wrap() {
    netplan::Health h;
    const uint32_t ok = 0xFFFFF000u;                     // last good poll just before the wrap
    h.onPoll(true, true, false, ok);
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, true, ok + 179000u));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, true, ok + 180000u));
}
void test_health_dead_zone_without_memory_evidence_never_restarts() {
    // hotspot up, no mobile data (or 401/429): a restart can't fix it -> no reboot loop
    netplan::Health h;
    h.onPoll(true, true, false, 1000);
    for (uint32_t t = 5000; t < 3600000; t += 4000)
        TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, false, t));
}
void test_health_wifi_down_backstop_after_15_min() {
    netplan::Health h;
    h.onPoll(true, true, false, 1000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)h.onPoll(false, false, false, 500000));
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, false, false, 500000 + 899000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, false, false, 500000 + 900000));
}
void test_health_wifi_back_resets_backstop_and_memory_clock() {
    netplan::Health h;
    h.onPoll(true, true, false, 1000);
    h.onPoll(false, false, false, 500000);                 // WiFi down at 500 s
    h.onPoll(false, true, true, 1000000);                  // back up at 1000 s, starved
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, true, 1000000 + 179000));
    TEST_ASSERT_EQUAL_INT((int)Act::Restart, (int)h.onPoll(false, true, true, 1000000 + 180000));
    netplan::Health g;
    g.onPoll(false, false, false, 0);
    g.onPoll(false, true, false, 800000);                  // WiFi returned: backstop clock cleared
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)g.onPoll(false, false, false, 1000000));
}
void test_health_first_call_failing_starts_clock() {
    netplan::Health h;
    TEST_ASSERT_NOT_EQUAL((int)Act::Restart, (int)h.onPoll(false, true, true, 900000));
}
void test_stale_after_limit_and_handles_wrap() {
    TEST_ASSERT_FALSE(netplan::stale(10000, 0, 20000));        // never polled: not "stale"
    TEST_ASSERT_FALSE(netplan::stale(30000, 10000, 20000));
    TEST_ASSERT_TRUE(netplan::stale(30001, 10000, 20000));
    TEST_ASSERT_TRUE(netplan::stale(5000, 0xFFFFF000u, 2000)); // millis() wrapped
}
using lyricstatus::Result;
using lyricstatus::Status;
void test_lyrics_retry_after_5s_then_20s_then_gives_up() {
    netplan::LyricsRetry r;
    TEST_ASSERT_TRUE(r.ready(0));
    TEST_ASSERT_FALSE(r.onResult(Result::TempError, 1000));
    TEST_ASSERT_FALSE(r.ready(1000 + 4999));
    TEST_ASSERT_TRUE(r.ready(1000 + 5000));
    TEST_ASSERT_FALSE(r.onResult(Result::TempError, 7000));
    TEST_ASSERT_FALSE(r.ready(7000 + 19999));
    TEST_ASSERT_TRUE(r.ready(7000 + 20000));
    TEST_ASSERT_TRUE(r.onResult(Result::TempError, 30000));   // third: give up
    TEST_ASSERT_EQUAL_INT(3, r.attempts());
}
void test_lyrics_other_results_are_final() {
    for (Result x : {Result::Synced, Result::Plain, Result::Instrumental, Result::NotFound}) {
        netplan::LyricsRetry r;
        TEST_ASSERT_TRUE(r.onResult(x, 0));
    }
}
void test_lyrics_retry_reset_on_track_change() {
    netplan::LyricsRetry r;
    r.onResult(Result::TempError, 1000);
    r.reset();
    TEST_ASSERT_TRUE(r.ready(1001));
    TEST_ASSERT_EQUAL_INT(0, r.attempts());
}
void test_lyrics_retry_ready_survives_millis_wrap() {
    netplan::LyricsRetry r;
    r.onResult(Result::TempError, 0xFFFFF000u);
    TEST_ASSERT_FALSE(r.ready(0x00000100u));                 // 4352 ms later
    TEST_ASSERT_TRUE(r.ready(0xFFFFF000u + 5000u));
}
void test_next_skips_lyrics_while_waiting() {
    netplan::Work w = netplan::freshWork(false);
    netplan::done(w, Step::Art);
    TEST_ASSERT_EQUAL_INT((int)Step::Genre, (int)netplan::next(w, false));
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w, true));
    netplan::done(w, Step::Genre);
    netplan::done(w, Step::Walk);
    netplan::done(w, Step::Prefetch);
    TEST_ASSERT_EQUAL_INT((int)Step::None, (int)netplan::next(w, false));
}
void test_genre_done_skips_to_walk() {
    // A cached genre marks the step done at the track change: the walker comes right after lyrics.
    netplan::Work w = netplan::freshWork(false);
    netplan::done(w, Step::Genre);
    netplan::done(w, Step::Art);
    netplan::done(w, Step::Lyrics);
    TEST_ASSERT_EQUAL_INT((int)Step::Walk, (int)netplan::next(w));
}
void test_status_for_results() {
    TEST_ASSERT_EQUAL_INT((int)Status::Retrying, (int)lyricstatus::statusFor(Result::TempError, false));
    TEST_ASSERT_EQUAL_INT((int)Status::None, (int)lyricstatus::statusFor(Result::TempError, true));
}
void test_walk_retry_out_of_memory_then_gives_up() {
    netplan::WalkRetry r;
    TEST_ASSERT_FALSE(r.onResult(false, true));    // decoder didn't fit: run the step again
    TEST_ASSERT_FALSE(r.onResult(false, true));
    TEST_ASSERT_TRUE(r.onResult(false, true));     // third attempt: give up (no walker this time)
    TEST_ASSERT_FALSE(r.onResult(false, true));    // a fresh count for the next step
}

void test_walk_retry_other_results_are_final() {
    netplan::WalkRetry r;
    TEST_ASSERT_TRUE(r.onResult(true, false));
    TEST_ASSERT_TRUE(r.onResult(false, false));    // missing / unsupported / network: not memory
    TEST_ASSERT_FALSE(r.onResult(false, true));
    TEST_ASSERT_TRUE(r.onResult(true, false));     // success ends the retries
    TEST_ASSERT_FALSE(r.onResult(false, true));    // and starts the count over
}

// A failed album art download (timeout, 5xx) runs again once, WAIT_MS later.
void test_art_retry_once_then_gives_up() {
    netplan::ArtRetry r;
    TEST_ASSERT_FALSE(r.onResult(false, true, 1000));    // failed: try once more
    TEST_ASSERT_TRUE(r.onResult(false, true, 7000));     // second failure: no cover for this song
    TEST_ASSERT_FALSE(r.onResult(false, true, 9000));    // a fresh count for the next song
}

// The second try waits WAIT_MS (a poll in between): a 5xx or refused connection retried at
// once would fail the same way.
void test_art_retry_waits_before_the_second_try() {
    netplan::ArtRetry r;
    TEST_ASSERT_TRUE(r.ready(0));
    r.onResult(false, true, 1000);
    TEST_ASSERT_FALSE(r.ready(1000 + netplan::ArtRetry::WAIT_MS - 1));
    TEST_ASSERT_TRUE(r.ready(1000 + netplan::ArtRetry::WAIT_MS));
    netplan::ArtRetry w;                                    // across the millis() wrap
    w.onResult(false, true, 0xFFFFF000u);
    TEST_ASSERT_FALSE(w.ready(0xFFFFF000u + netplan::ArtRetry::WAIT_MS - 1));
    TEST_ASSERT_TRUE(w.ready(0xFFFFF000u + netplan::ArtRetry::WAIT_MS));
}

// Success, or no URL to fetch (some podcasts), is final at once.
void test_art_ok_or_no_url_is_final() {
    netplan::ArtRetry ok;
    TEST_ASSERT_TRUE(ok.onResult(true, true, 0));
    netplan::ArtRetry noUrl;
    TEST_ASSERT_TRUE(noUrl.onResult(false, false, 0));
    TEST_ASSERT_TRUE(noUrl.ready(0));
}

// A new song mid-retry starts the count over and doesn't wait: it doesn't inherit a spent retry.
void test_art_retry_reset_on_track_change() {
    netplan::ArtRetry r;
    TEST_ASSERT_FALSE(r.onResult(false, true, 1000));
    r.reset();
    TEST_ASSERT_TRUE(r.ready(1001));
    TEST_ASSERT_FALSE(r.onResult(false, true, 1001));
}

// While the art retry waits, the other steps go ahead (art first again once it's ready).
void test_next_skips_art_while_waiting() {
    netplan::Work w = netplan::freshWork(false);
    TEST_ASSERT_EQUAL_INT((int)Step::Lyrics, (int)netplan::next(w, true, false));
    TEST_ASSERT_EQUAL_INT((int)Step::Art, (int)netplan::next(w, true, true));
}

void test_backoff_doubles_on_429_up_to_a_minute() {
    netplan::PollBackoff b;
    TEST_ASSERT_EQUAL_UINT32(4000, b.interval());
    TEST_ASSERT_FALSE(b.active());
    const uint32_t want[] = {8000, 16000, 32000, 60000, 60000};
    for (uint32_t w : want) {
        b.onPoll(429);
        TEST_ASSERT_EQUAL_UINT32(w, b.interval());
        TEST_ASSERT_TRUE(b.active());
    }
}

void test_backoff_only_success_returns_to_base() {
    const int ok[] = {200, 204};
    for (int code : ok) {
        netplan::PollBackoff b;
        b.onPoll(429);
        b.onPoll(429);
        b.onPoll(code);
        TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::BASE_MS, b.interval());
        TEST_ASSERT_FALSE(b.active());
    }
}

void test_backoff_other_failures_neither_reset_nor_keep_a_long_wait() {
    const int others[] = {401, 500, -1};
    for (int code : others) {
        netplan::PollBackoff b;
        b.onPoll(429);
        b.onPoll(429);                                  // 16 s
        b.onPoll(code);                                 // a timeout mid-episode: keep backing off
        TEST_ASSERT_EQUAL_UINT32(16000, b.interval());
        b.onPoll(429, 6537);                            // 1h 49m
        b.onPoll(code);                                 // first poll after the wait times out
        TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::MAX_MS, b.interval());   // not 4 s, not 1h 49m
    }
    netplan::PollBackoff fresh;                         // outside an episode a failure changes nothing
    fresh.onPoll(-1);
    TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::BASE_MS, fresh.interval());
}

void test_backoff_honours_spotifys_retry_after() {
    netplan::PollBackoff b;
    b.onPoll(429, 6537);                                   // "come back in 1h 49m"
    TEST_ASSERT_EQUAL_UINT32(6538000, b.interval());       // + 1 s margin
    b.onPoll(429, 2);                                      // tiny wait: never below the doubling
    TEST_ASSERT_EQUAL_UINT32(60000, b.interval());
    b.onPoll(429, 999999);                                 // absurd: capped at a day
    TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::RETRY_AFTER_MAX_MS, b.interval());
    b.onPoll(200, 0);
    TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::BASE_MS, b.interval());
}

void test_retry_after_parses_seconds_only() {
    TEST_ASSERT_EQUAL_INT(6537, netplan::parseRetryAfter("6537"));
    TEST_ASSERT_EQUAL_INT(30, netplan::parseRetryAfter(" 30 "));
    TEST_ASSERT_EQUAL_INT(-1, netplan::parseRetryAfter(""));
    TEST_ASSERT_EQUAL_INT(-1, netplan::parseRetryAfter(nullptr));
    TEST_ASSERT_EQUAL_INT(-1, netplan::parseRetryAfter("Wed, 21 Oct 2026 07:28:00 GMT"));   // HTTP date
    TEST_ASSERT_EQUAL_INT(-1, netplan::parseRetryAfter("12abc"));
    TEST_ASSERT_EQUAL_INT(7 * 24 * 3600, netplan::parseRetryAfter("99999999999"));        // clamped to a week
}

void test_idle_poll_keeps_the_base_pace_while_playing() {
    netplan::IdlePoll p;
    TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::BASE_MS, p.every(netplan::PollBackoff::BASE_MS));
    p.onPoll(false, false, 1000);                       // playing
    TEST_ASSERT_FALSE(p.idle());
    TEST_ASSERT_EQUAL_UINT32(4000, p.every(4000));
}

void test_idle_poll_slows_at_once_when_nothing_plays() {
    netplan::IdlePoll p;
    p.onPoll(false, true, 1000);                        // 204 / no song: "Nothing playing" at once
    TEST_ASSERT_TRUE(p.idle());
    TEST_ASSERT_EQUAL_UINT32(netplan::IdlePoll::IDLE_MS, p.every(4000));
    TEST_ASSERT_TRUE(netplan::IdlePoll::IDLE_MS >= 10000 && netplan::IdlePoll::IDLE_MS <= 12000);
    p.onPoll(false, false, 11000);                      // a song started on the phone
    TEST_ASSERT_FALSE(p.idle());
    TEST_ASSERT_EQUAL_UINT32(4000, p.every(4000));
}

void test_idle_poll_slows_a_pause_only_after_the_idle_switch() {
    netplan::IdlePoll p;
    p.onPoll(true, false, 0);                           // paused: a short pause keeps the 4 s pace
    TEST_ASSERT_EQUAL_UINT32(4000, p.every(4000));
    p.onPoll(true, false, 29999);
    TEST_ASSERT_EQUAL_UINT32(4000, p.every(4000));
    p.onPoll(true, false, 30000);                       // the UI's "Nothing playing" switch
    TEST_ASSERT_EQUAL_UINT32(netplan::IdlePoll::IDLE_MS, p.every(4000));
    p.onPoll(false, false, 40000);                      // resumed
    TEST_ASSERT_EQUAL_UINT32(4000, p.every(4000));
}

void test_idle_poll_never_shortens_a_backoff() {
    netplan::IdlePoll p;
    p.onPoll(false, true, 0);
    TEST_ASSERT_EQUAL_UINT32(netplan::IdlePoll::IDLE_MS, p.every(8000));   // 429 doubling below idle
    TEST_ASSERT_EQUAL_UINT32(32000, p.every(32000));                       // longer 429 wait wins
}

void test_idle_poll_picks_a_new_song_up_within_the_idle_interval() {
    // Polls at the paced interval while nothing plays; the phone starts a song at any moment:
    // the next poll (which sees it) is never more than IDLE_MS later.
    for (uint32_t start = 0; start < 60000; start += 700) {
        netplan::IdlePoll p;
        uint32_t t = 0;
        p.onPoll(false, true, t);
        while (t <= start) t += p.every(netplan::PollBackoff::BASE_MS);
        TEST_ASSERT_TRUE(t - start <= netplan::IdlePoll::IDLE_MS);
        p.onPoll(false, false, t);                      // that poll sees it playing
        TEST_ASSERT_EQUAL_UINT32(netplan::PollBackoff::BASE_MS, p.every(netplan::PollBackoff::BASE_MS));
    }
}

void test_idle_one_slow_failed_poll_is_not_no_signal() {
    // Idle: a good poll ends at 1.0 s; the next starts 10 s after the last start (0.7 s), times
    // out after 9.5 s, and its retry (due at once) answers 1.5 s later.
    const uint32_t lastOk = 1000, failEnd = 700 + netplan::IdlePoll::IDLE_MS + 9500, okEnd = failEnd + 1500;
    TEST_ASSERT_TRUE(netplan::stale(okEnd, lastOk, netplan::STALE_MS));   // the plain 20 s trips
    for (uint32_t now = lastOk; now <= okEnd; now += 100)
        TEST_ASSERT_FALSE(netplan::stale(now, lastOk, netplan::staleLimit(true)));
}

void test_a_real_outage_still_shows_no_signal_in_time() {
    const uint32_t lastOk = 1000;   // every poll after this one fails
    TEST_ASSERT_EQUAL_UINT32(netplan::STALE_MS, netplan::staleLimit(false));
    TEST_ASSERT_FALSE(netplan::stale(lastOk + 20000, lastOk, netplan::staleLimit(false)));
    TEST_ASSERT_TRUE(netplan::stale(lastOk + 20001, lastOk, netplan::staleLimit(false)));   // playing: 20 s
    TEST_ASSERT_TRUE(netplan::stale(lastOk + 26001, lastOk, netplan::staleLimit(true)));    // idle: 26 s
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_track_changes_bump_generation);
    RUN_TEST(test_backoff_doubles_on_429_up_to_a_minute);
    RUN_TEST(test_backoff_only_success_returns_to_base);
    RUN_TEST(test_backoff_other_failures_neither_reset_nor_keep_a_long_wait);
    RUN_TEST(test_backoff_honours_spotifys_retry_after);
    RUN_TEST(test_retry_after_parses_seconds_only);
    RUN_TEST(test_empty_name_is_ignored_and_keeps_last);
    RUN_TEST(test_order_without_prefetched_walker);
    RUN_TEST(test_prefetched_walker_skips_walk_step);
    RUN_TEST(test_link_stays_up_through_isolated_failures);
    RUN_TEST(test_link_down_after_three_consecutive_failures);
    RUN_TEST(test_link_down_at_once_on_a_definite_failure);
    RUN_TEST(test_can_run_gates_on_largest_block);
    RUN_TEST(test_health_two_failures_pause_optional_until_success);
    RUN_TEST(test_health_restart_after_180s_when_memory_starved);
    RUN_TEST(test_health_restart_clock_survives_millis_wrap);
    RUN_TEST(test_health_dead_zone_without_memory_evidence_never_restarts);
    RUN_TEST(test_health_wifi_down_backstop_after_15_min);
    RUN_TEST(test_health_wifi_back_resets_backstop_and_memory_clock);
    RUN_TEST(test_health_first_call_failing_starts_clock);
    RUN_TEST(test_stale_after_limit_and_handles_wrap);
    RUN_TEST(test_lyrics_retry_after_5s_then_20s_then_gives_up);
    RUN_TEST(test_lyrics_other_results_are_final);
    RUN_TEST(test_lyrics_retry_reset_on_track_change);
    RUN_TEST(test_lyrics_retry_ready_survives_millis_wrap);
    RUN_TEST(test_next_skips_lyrics_while_waiting);
    RUN_TEST(test_status_for_results);
    RUN_TEST(test_genre_done_skips_to_walk);
    RUN_TEST(test_walk_retry_out_of_memory_then_gives_up);
    RUN_TEST(test_walk_retry_other_results_are_final);
    RUN_TEST(test_art_retry_once_then_gives_up);
    RUN_TEST(test_art_retry_waits_before_the_second_try);
    RUN_TEST(test_next_skips_art_while_waiting);
    RUN_TEST(test_art_ok_or_no_url_is_final);
    RUN_TEST(test_art_retry_reset_on_track_change);
    RUN_TEST(test_idle_poll_keeps_the_base_pace_while_playing);
    RUN_TEST(test_idle_poll_slows_at_once_when_nothing_plays);
    RUN_TEST(test_idle_poll_slows_a_pause_only_after_the_idle_switch);
    RUN_TEST(test_idle_poll_never_shortens_a_backoff);
    RUN_TEST(test_idle_poll_picks_a_new_song_up_within_the_idle_interval);
    RUN_TEST(test_idle_one_slow_failed_poll_is_not_no_signal);
    RUN_TEST(test_a_real_outage_still_shows_no_signal_in_time);
    return UNITY_END();
}
