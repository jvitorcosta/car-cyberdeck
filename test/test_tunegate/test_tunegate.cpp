#include <unity.h>
#include "../../src/util/tunegate.h"

using tunegate::Act;
using tunegate::Gate;
void setUp() {}
void tearDown() {}

constexpr unsigned PLENTY = 100000, START = 32000, KEEP = 24000;

static Act step(Gate& g, bool want, bool playing, uint32_t fails, unsigned largest, uint32_t now,
                bool busy = false) {
    return g.update({want, playing, busy, fails, largest, START, KEEP}, now);
}

void test_starts_when_wanted_with_memory() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 0) == Act::Play);
    TEST_ASSERT_TRUE(step(g, true, true, 0, PLENTY, 20) == Act::None);       // playing: leave it
}

void test_stops_when_not_wanted() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, false, true, 0, PLENTY, 0) == Act::Stop);
    TEST_ASSERT_TRUE(step(g, false, false, 0, PLENTY, 20) == Act::None);
}

// Another sound (a howl, the last tune fading): wait for it, next pass.
void test_waits_for_another_sound() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 0, true) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 20) == Act::Play);
}

// Low memory: not started, and tried again only a minute later (as before).
void test_low_memory_backs_off_a_minute() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, true, false, 0, START - 1, 0) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, tunegate::RETRY_MS - 1) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, tunegate::RETRY_MS) == Act::Play);
}

// Playing and memory runs low: stop, then a minute's rest.
void test_low_memory_while_playing_stops() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, true, true, 0, KEEP - 1, 1000) == Act::Stop);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 1000 + tunegate::RETRY_MS - 1) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 1000 + tunegate::RETRY_MS) == Act::Play);
}

// The audio didn't start (the codec not answering): told once, then a minute's rest instead of a
// new task, I2S install and codec reset every 20 ms; then one more try.
void test_audio_failure_backs_off_a_minute() {
    Gate g;
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 0) == Act::Play);
    TEST_ASSERT_TRUE(step(g, true, false, 1, PLENTY, 20) == Act::AudioFailed);
    TEST_ASSERT_TRUE(step(g, true, false, 1, PLENTY, 40) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 1, PLENTY, 20 + tunegate::RETRY_MS - 1) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 1, PLENTY, 20 + tunegate::RETRY_MS) == Act::Play);
    TEST_ASSERT_TRUE(step(g, true, false, 2, PLENTY, 20 + tunegate::RETRY_MS + 20) == Act::AudioFailed);
}

// Leaving the waiting screen forgets the rest: coming back starts at once.
void test_leaving_resets_the_rest() {
    Gate g;
    step(g, true, false, 0, START - 1, 0);
    step(g, false, false, 0, PLENTY, 100);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 200) == Act::Play);
}

void test_rest_survives_millis_wrap() {
    Gate g;
    step(g, true, false, 0, START - 1, 0xFFFFF000u);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 0xFFFFF000u + tunegate::RETRY_MS - 1) == Act::None);
    TEST_ASSERT_TRUE(step(g, true, false, 0, PLENTY, 0xFFFFF000u + tunegate::RETRY_MS) == Act::Play);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_when_wanted_with_memory);
    RUN_TEST(test_stops_when_not_wanted);
    RUN_TEST(test_waits_for_another_sound);
    RUN_TEST(test_low_memory_backs_off_a_minute);
    RUN_TEST(test_low_memory_while_playing_stops);
    RUN_TEST(test_audio_failure_backs_off_a_minute);
    RUN_TEST(test_leaving_resets_the_rest);
    RUN_TEST(test_rest_survives_millis_wrap);
    return UNITY_END();
}
