#include <unity.h>
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_off_is_normal);
    RUN_TEST(test_synced_shows_karaoke);
    RUN_TEST(test_no_lyrics_for_30s_then_normal);
    RUN_TEST(test_plain_counts_as_none);
    RUN_TEST(test_retrying_does_not_count);
    RUN_TEST(test_a_new_song_restarts_the_timer);
    RUN_TEST(test_keepalive_lapse_turns_it_off);
    RUN_TEST(test_stop_is_at_once);
    return UNITY_END();
}
