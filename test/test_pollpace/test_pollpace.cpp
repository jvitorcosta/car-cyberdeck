#include <unity.h>
#include "../../src/util/yellow/pollpace.h"

void setUp() {}
void tearDown() {}

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
    RUN_TEST(test_poll_pace_slows_after_ten_misses_and_recovers);
    RUN_TEST(test_menu_pace_backs_off_after_ten_misses);
    RUN_TEST(test_lookups_are_spaced_only_while_the_deck_keeps_missing);
    RUN_TEST(test_a_new_pace_looks_up_at_once);
    return UNITY_END();
}
