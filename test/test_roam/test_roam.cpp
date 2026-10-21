#include <unity.h>
#include "../../src/util/roam.h"

void setUp() {}
void tearDown() {}

// The hotspot (index 0) is the car's network: three tries at boot, while the phone may still be
// locked; two after a short blip; any other network one.
void test_tries_per_network() {
    TEST_ASSERT_EQUAL_INT(3, roam::tries(0, -1));   // boot: time to unlock the phone
    TEST_ASSERT_EQUAL_INT(2, roam::tries(0, 0));    // dropped off the hotspot: two tries
    TEST_ASSERT_EQUAL_INT(1, roam::tries(0, 1));    // was on the fallback: one
    TEST_ASSERT_EQUAL_INT(1, roam::tries(1, 0));
    TEST_ASSERT_EQUAL_INT(1, roam::tries(1, -1));   // the fallback at boot
}

// On a fallback the deck scans for the hotspot once a minute; on the hotspot, or offline, never.
void test_looks_for_the_hotspot_only_from_a_fallback() {
    TEST_ASSERT_TRUE(roam::lookForPreferred(1, roam::LOOK_MS));
    TEST_ASSERT_TRUE(roam::lookForPreferred(2, roam::LOOK_MS + 5000));
    TEST_ASSERT_FALSE(roam::lookForPreferred(1, roam::LOOK_MS - 1));   // not yet
    TEST_ASSERT_FALSE(roam::lookForPreferred(0, roam::LOOK_MS * 10));  // already on it
    TEST_ASSERT_FALSE(roam::lookForPreferred(-1, roam::LOOK_MS * 10)); // offline: reconnect does it
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tries_per_network);
    RUN_TEST(test_looks_for_the_hotspot_only_from_a_fallback);
    return UNITY_END();
}
