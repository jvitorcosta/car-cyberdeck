#include <unity.h>
#include "../../src/util/nowplaying.h"

using nowplaying::On204;
void setUp() {}
void tearDown() {}

// Spotify's currently-playing endpoint sometimes answers 204 ("nothing playing") for minutes
// while the iPhone plays (seen twice in 30 min, 2026-10-14; /me/player still reported the song).
// On a 204 the deck asks /me/player and keeps the song on screen if that one says playing.
void test_player_says_playing_keeps_the_song() {
    TEST_ASSERT_TRUE(nowplaying::on204(200, true, true) == On204::KeepTrack);
}
void test_player_says_stopped() {
    TEST_ASSERT_TRUE(nowplaying::on204(200, false, true) == On204::Stopped);
    TEST_ASSERT_TRUE(nowplaying::on204(204, false, true) == On204::Stopped);
}
// A failed /me/player (TLS -76, timeout, 5xx) says nothing either way: the song stays, instead
// of "Nothing playing" while the phone plays (the 2026-10-14 stall's likely path).
void test_player_unreachable_keeps_what_is_shown() {
    TEST_ASSERT_TRUE(nowplaying::on204(-1, false, true) == On204::Unknown);
    TEST_ASSERT_TRUE(nowplaying::on204(0, false, true) == On204::Unknown);
    TEST_ASSERT_TRUE(nowplaying::on204(503, false, true) == On204::Unknown);
    TEST_ASSERT_TRUE(nowplaying::on204(401, false, true) == On204::Stopped);
}
void test_no_song_known() {
    TEST_ASSERT_TRUE(nowplaying::on204(200, true, false) == On204::Stopped);   // nothing to keep
    TEST_ASSERT_TRUE(nowplaying::on204(-1, true, false) == On204::Stopped);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_player_says_playing_keeps_the_song);
    RUN_TEST(test_player_says_stopped);
    RUN_TEST(test_player_unreachable_keeps_what_is_shown);
    RUN_TEST(test_no_song_known);
    return UNITY_END();
}
