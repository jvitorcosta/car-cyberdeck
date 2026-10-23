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

void test_menu_pace_backs_off_after_ten_misses() {
    pollpace::Pace p;
    TEST_ASSERT_EQUAL_UINT32(pollpace::MENU_MS, p.menuIntervalMs());
    for (int i = 0; i < 9; i++) p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::MENU_MS, p.menuIntervalMs());
    p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::MENU_SLOW_MS, p.menuIntervalMs());
    p.onResult(true);  // the deck answers: back at once
    TEST_ASSERT_EQUAL_UINT32(pollpace::MENU_MS, p.menuIntervalMs());
}

void test_lookups_are_spaced_only_while_the_deck_keeps_missing() {
    pollpace::Pace p;
    TEST_ASSERT_TRUE(p.mayLookup(1000));
    p.lookedUp(1000);
    TEST_ASSERT_TRUE(p.mayLookup(1100));  // a few misses: look again at once
    for (int i = 0; i < 10; i++) p.onResult(false);
    TEST_ASSERT_FALSE(p.mayLookup(1100));
    TEST_ASSERT_FALSE(p.mayLookup(1000 + pollpace::LOOKUP_MS - 1));
    TEST_ASSERT_TRUE(p.mayLookup(1000 + pollpace::LOOKUP_MS));
    p.lookedUp(40000);
    TEST_ASSERT_FALSE(p.mayLookup(41000));
    p.onResult(true);  // answered: the next loss looks up at once
    for (int i = 0; i < 10; i++) p.onResult(false);
    TEST_ASSERT_TRUE(p.mayLookup(41000));
}

void test_a_new_pace_looks_up_at_once() {  // KARAOKE opened: pace = Pace()
    pollpace::Pace p;
    for (int i = 0; i < 12; i++) p.onResult(false);
    p.lookedUp(5);
    p = pollpace::Pace();
    TEST_ASSERT_TRUE(p.mayLookup(6));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_shows_the_line_until_the_next_one_starts);
    RUN_TEST(test_paused_song_doesnt_advance);
    RUN_TEST(test_intro_shows_the_upcoming_first_line_small);
    RUN_TEST(test_no_lyrics_shows_title_and_artist);
    RUN_TEST(test_song_change_drops_the_old_lines);
    RUN_TEST(test_poll_pace_slows_after_ten_misses_and_recovers);
    RUN_TEST(test_menu_pace_backs_off_after_ten_misses);
    RUN_TEST(test_lookups_are_spaced_only_while_the_deck_keeps_missing);
    RUN_TEST(test_a_new_pace_looks_up_at_once);
    return UNITY_END();
}
