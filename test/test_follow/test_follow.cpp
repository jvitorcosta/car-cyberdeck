#include <unity.h>
#include "../../src/util/follow.h"

void setUp() {}
void tearDown() {}

void test_a_reachable_deck_never_switches() {
    follow::Follow f;
    for (uint32_t t = 0; t < 600000; t += 1000) TEST_ASSERT_FALSE(f.update(true, t));
}
void test_switches_after_thirty_seconds_lost() {
    follow::Follow f;
    f.update(true, 0);
    TEST_ASSERT_FALSE(f.update(false, 1000));
    TEST_ASSERT_FALSE(f.update(false, 1000 + follow::LOST_MS - 1));
    TEST_ASSERT_TRUE(f.update(false, 1000 + follow::LOST_MS));
    TEST_ASSERT_FALSE(f.update(false, 1000 + follow::LOST_MS + 1000));   // not again at once
}
void test_more_switches_back_off() {
    follow::Follow f;
    f.update(false, 0);
    TEST_ASSERT_TRUE(f.update(false, 30000));                 // 1st switch
    TEST_ASSERT_FALSE(f.update(false, 30000 + 59999));
    TEST_ASSERT_TRUE(f.update(false, 30000 + 60000));          // 2nd after 1 min
    TEST_ASSERT_FALSE(f.update(false, 90000 + 119999));
    TEST_ASSERT_TRUE(f.update(false, 90000 + 120000));         // 3rd after 2 min
    TEST_ASSERT_FALSE(f.update(false, 210000 + 299999));
    TEST_ASSERT_TRUE(f.update(false, 210000 + 300000));        // then every 5 min
    TEST_ASSERT_TRUE(f.update(false, 510000 + 300000));
}
void test_finding_the_deck_resets_everything() {
    follow::Follow f;
    f.update(false, 0);
    f.update(false, 30000);
    f.update(true, 40000);                                    // found it
    TEST_ASSERT_FALSE(f.update(false, 50000));
    TEST_ASSERT_TRUE(f.update(false, 50000 + follow::LOST_MS));  // a fresh 30 s, no back-off
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_reachable_deck_never_switches);
    RUN_TEST(test_switches_after_thirty_seconds_lost);
    RUN_TEST(test_more_switches_back_off);
    RUN_TEST(test_finding_the_deck_resets_everything);
    return UNITY_END();
}
