#include <unity.h>
#include <initializer_list>
#include "../../src/util/black/karaokemode.h"

using karaokemode::Mode;
using karaokemode::Screen;
using lyricstatus::Status;

void setUp() {}
void tearDown() {}

void test_off_is_normal() {
    Mode m;
    TEST_ASSERT_TRUE(m.screen(Status::Synced, 1, 0) == Screen::Normal);
}

void test_synced_shows_karaoke() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.screen(Status::Synced, 1, 100) == Screen::Karaoke);
    TEST_ASSERT_TRUE(m.screen(Status::Searching, 1, 100) == Screen::Karaoke);
}

void test_no_lyrics_for_30s_then_normal() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 1000) == Screen::NoLyrics);
    m.keepAlive(20000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 30999) == Screen::NoLyrics);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 31000) == Screen::Normal);
}

// Switched off and on again during a song known to have no lyrics: the message again, not the
// normal screen at once (the tap would seem to do nothing).
void test_switching_on_again_shows_no_lyrics_again() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 1000) == Screen::NoLyrics);
    m.keepAlive(20000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 31000) == Screen::Normal);
    m.stop();
    m.keepAlive(40000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 40000) == Screen::NoLyrics);
}

// A keep-alive while already on keeps the timer: the message still ends.
void test_keepalive_while_on_keeps_the_timer() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 1000) == Screen::NoLyrics);
    m.keepAlive(25000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 31000) == Screen::Normal);
}

void test_plain_counts_as_none() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.screen(Status::Plain, 1, 0) == Screen::NoLyrics);
    TEST_ASSERT_TRUE(m.screen(Status::Instrumental, 1, 29999) == Screen::NoLyrics);
    m.keepAlive(25000);
    TEST_ASSERT_TRUE(m.screen(Status::Plain, 1, 30000) == Screen::Normal);
}

void test_retrying_does_not_count() {
    Mode m;
    for (uint32_t t = 0; t <= 25000; t += 5000) {
        if (t % 10000 == 0) m.keepAlive(t);
        TEST_ASSERT_TRUE(m.screen(Status::Retrying, 1, t) == Screen::Karaoke);
    }
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 25000) == Screen::NoLyrics);
    m.keepAlive(30000);
    m.keepAlive(40000);
    m.keepAlive(50000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 54999) == Screen::NoLyrics);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 55000) == Screen::Normal);
}

void test_a_new_song_restarts_the_timer() {
    Mode m;
    m.keepAlive(0);
    m.screen(Status::None, 1, 0);
    m.keepAlive(25000);
    TEST_ASSERT_TRUE(m.screen(Status::None, 1, 30000) == Screen::Normal);
    TEST_ASSERT_TRUE(m.screen(Status::Synced, 2, 30001) == Screen::Karaoke);
    TEST_ASSERT_TRUE(m.screen(Status::None, 3, 30002) == Screen::NoLyrics);
}

void test_keepalive_lapse_turns_it_off() {
    Mode m;
    m.keepAlive(0);
    TEST_ASSERT_TRUE(m.on(29999));
    TEST_ASSERT_FALSE(m.on(30000));
    TEST_ASSERT_TRUE(m.screen(Status::Synced, 1, 30000) == Screen::Normal);
}

void test_stop_is_at_once() {
    Mode m;
    m.keepAlive(0);
    m.stop();
    TEST_ASSERT_TRUE(m.screen(Status::Synced, 1, 1) == Screen::Normal);
}

void test_parse_light_reads_swatches() {
    TEST_ASSERT_EQUAL_INT(0, karaokemode::parseLight("0"));
    TEST_ASSERT_EQUAL_INT(8, karaokemode::parseLight("8"));
    TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight("-1"));
}
void test_parse_light_rejects_junk() {
    for (const char* s : {"", "12", "9", "-5", "abc", "3x", " 3", "-"})
        TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight(s));
    TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight(nullptr));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_off_is_normal);
    RUN_TEST(test_synced_shows_karaoke);
    RUN_TEST(test_no_lyrics_for_30s_then_normal);
    RUN_TEST(test_switching_on_again_shows_no_lyrics_again);
    RUN_TEST(test_keepalive_while_on_keeps_the_timer);
    RUN_TEST(test_plain_counts_as_none);
    RUN_TEST(test_retrying_does_not_count);
    RUN_TEST(test_a_new_song_restarts_the_timer);
    RUN_TEST(test_keepalive_lapse_turns_it_off);
    RUN_TEST(test_stop_is_at_once);
    RUN_TEST(test_parse_light_reads_swatches);
    RUN_TEST(test_parse_light_rejects_junk);
    return UNITY_END();
}
