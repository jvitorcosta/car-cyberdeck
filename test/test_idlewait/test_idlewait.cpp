#include <unity.h>
#include "../../src/util/idlewait.h"

void setUp() {}
void tearDown() {}

// Spotify has nothing at all: "Nothing playing" right away.
void test_stopped_is_idle_at_once() {
    idlewait::Clock c;
    TEST_ASSERT_TRUE(c.update(false, true, 1000));
}

// A paused song keeps the deck for a minute, then "Nothing playing".
void test_paused_turns_idle_after_a_minute() {
    idlewait::Clock c;
    TEST_ASSERT_FALSE(c.update(true, false, 5000));
    TEST_ASSERT_FALSE(c.update(true, false, 5000 + idlewait::PAUSE_MS - 1));
    TEST_ASSERT_TRUE(c.update(true, false, 5000 + idlewait::PAUSE_MS));
}

// Playing again resets the wait: the next pause waits a full minute again.
void test_playing_resets_the_wait() {
    idlewait::Clock c;
    c.update(true, false, 0);
    TEST_ASSERT_FALSE(c.update(false, false, 50000));
    TEST_ASSERT_FALSE(c.update(true, false, 55000));
    TEST_ASSERT_FALSE(c.update(true, false, 55000 + idlewait::PAUSE_MS - 1));
    TEST_ASSERT_TRUE(c.update(true, false, 55000 + idlewait::PAUSE_MS));
}

// A pause that goes on into "nothing at all" (Spotify drops the session) stays idle.
void test_paused_then_stopped_stays_idle() {
    idlewait::Clock c;
    c.update(true, false, 0);
    TEST_ASSERT_TRUE(c.update(false, true, 10000));
    TEST_ASSERT_FALSE(c.update(true, false, 20000));   // a paused song comes back: wait again
}

void test_survives_millis_wrap() {
    idlewait::Clock c;
    TEST_ASSERT_FALSE(c.update(true, false, 0xFFFFFF00u));
    TEST_ASSERT_FALSE(c.update(true, false, 1000));
    TEST_ASSERT_TRUE(c.update(true, false, 0xFFFFFF00u + idlewait::PAUSE_MS));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stopped_is_idle_at_once);
    RUN_TEST(test_paused_turns_idle_after_a_minute);
    RUN_TEST(test_playing_resets_the_wait);
    RUN_TEST(test_paused_then_stopped_stays_idle);
    RUN_TEST(test_survives_millis_wrap);
    return UNITY_END();
}
