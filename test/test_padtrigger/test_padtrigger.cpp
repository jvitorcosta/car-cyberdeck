#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padtrigger.h"

void setUp() {}
void tearDown() {}

void test_a_light_touch_on_a_trigger_is_not_a_press() {
    padtrigger::Trigger t;
    TEST_ASSERT_FALSE(t.feed(30));   // a resting finger
    TEST_ASSERT_FALSE(t.feed(119));
    TEST_ASSERT_TRUE(t.feed(120));   // past half travel
}

void test_a_trigger_releases_only_well_below_its_press() {
    padtrigger::Trigger t;
    t.feed(200);
    TEST_ASSERT_TRUE(t.feed(100));   // eased off, still held
    TEST_ASSERT_TRUE(t.feed(40));
    TEST_ASSERT_FALSE(t.feed(39));
    TEST_ASSERT_FALSE(t.feed(100));  // back over RELEASE_AT, not PRESS_AT: still up
}

void test_a_full_squeeze_in_one_report_is_a_press() {
    padtrigger::Trigger t;
    TEST_ASSERT_TRUE(t.feed(255));   // a quick squeeze: one report at ~19 a second
    TEST_ASSERT_FALSE(t.feed(0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_light_touch_on_a_trigger_is_not_a_press);
    RUN_TEST(test_a_trigger_releases_only_well_below_its_press);
    RUN_TEST(test_a_full_squeeze_in_one_report_is_a_press);
    return UNITY_END();
}
