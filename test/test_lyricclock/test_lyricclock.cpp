#include <unity.h>
#include <cstring>
#include "../../src/util/lyricclock.h"
#include "../../src/util/pollpace.h"

void setUp() {}
void tearDown() {}

static stateparse::Deck deck() {
    stateparse::Deck d{};
    d.trackGen = 3;
    d.playing = true;
    d.progressMs = 61000;
    d.lineStartMs = 59000;
    d.nextStartMs = 64000;
    strcpy(d.lyricKind, "synced");
    strcpy(d.track, "Made-up Song");
    strcpy(d.artist, "Made-up Artist");
    strcpy(d.line, "first made-up line");
    strcpy(d.next, "second made-up line");
    return d;
}

void test_shows_the_line_until_the_next_one_starts() {
    stateparse::Deck d = deck();
    lyricclock::View v = lyricclock::pick(d, 2000);           // 63.0 s
    TEST_ASSERT_EQUAL_STRING("first made-up line", v.big);
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.small);
    v = lyricclock::pick(d, 3000);                             // 64.0 s: next starts
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.big);
    TEST_ASSERT_EQUAL_STRING("", v.small);                     // unknown until the next poll
}

void test_paused_song_doesnt_advance() {
    stateparse::Deck d = deck();
    d.playing = false;
    TEST_ASSERT_EQUAL_STRING("first made-up line", lyricclock::pick(d, 9000).big);
}

void test_intro_shows_the_upcoming_first_line_small() {
    stateparse::Deck d = deck();
    d.line[0] = '\0';
    d.lineStartMs = 0;
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Made-up Song", v.big);           // title while nothing is sung
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.small);
}

void test_no_lyrics_shows_title_and_artist() {
    stateparse::Deck d = deck();
    strcpy(d.lyricKind, "none");
    d.line[0] = d.next[0] = '\0';
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Made-up Song", v.big);
    TEST_ASSERT_EQUAL_STRING("Made-up Artist", v.small);
}

void test_song_change_drops_the_old_lines() {
    // The deck already moved on (new trackGen) but still searches lyrics: no old lines.
    stateparse::Deck d = deck();
    d.trackGen = 4;
    strcpy(d.lyricKind, "searching");
    d.line[0] = d.next[0] = '\0';
    strcpy(d.track, "Next Made-up Song");
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Next Made-up Song", v.big);
    TEST_ASSERT_TRUE(strcmp(v.small, "second made-up line") != 0);
}

void test_poll_pace_slows_after_ten_misses_and_recovers() {
    pollpace::Pace p;
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    for (int i = 0; i < 9; ++i) p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::SLOW_MS, p.intervalMs());
    p.onResult(true);
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    TEST_ASSERT_EQUAL_INT(0, p.misses());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_shows_the_line_until_the_next_one_starts);
    RUN_TEST(test_paused_song_doesnt_advance);
    RUN_TEST(test_intro_shows_the_upcoming_first_line_small);
    RUN_TEST(test_no_lyrics_shows_title_and_artist);
    RUN_TEST(test_song_change_drops_the_old_lines);
    RUN_TEST(test_poll_pace_slows_after_ten_misses_and_recovers);
    return UNITY_END();
}
