#include <unity.h>
#include "../../src/util/common/bootguard.h"

void setUp() {}
void tearDown() {}

using bootguard::MAGIC;

void test_crashes_count_up_in_a_row() {
    uint32_t n = bootguard::afterBoot(0x12345678, 999, true);   // first crash: RTC held noise
    TEST_ASSERT_EQUAL_UINT32(1, n);
    n = bootguard::afterBoot(MAGIC, n, true);
    n = bootguard::afterBoot(MAGIC, n, true);
    TEST_ASSERT_EQUAL_UINT32(3, n);
}

void test_any_other_boot_clears_the_count() {
    TEST_ASSERT_EQUAL_UINT32(0, bootguard::afterBoot(MAGIC, 2, false));   // power-on, restart
    TEST_ASSERT_EQUAL_UINT32(1, bootguard::afterBoot(MAGIC, 77, true));   // a count past LIMIT is noise
}

void test_rolls_back_only_new_firmware_with_somewhere_to_go() {
    TEST_ASSERT_TRUE(bootguard::rollBack(3, false, true));
    TEST_ASSERT_FALSE(bootguard::rollBack(2, false, true));    // not yet
    TEST_ASSERT_FALSE(bootguard::rollBack(3, true, true));     // proven firmware: crashes are bugs, not the update
    TEST_ASSERT_FALSE(bootguard::rollBack(3, false, false));   // the other slot is empty
}

void test_healthy_needs_both_time_and_a_poll() {
    TEST_ASSERT_FALSE(bootguard::healthy(59999, true));
    TEST_ASSERT_FALSE(bootguard::healthy(600000, false));
    TEST_ASSERT_TRUE(bootguard::healthy(60000, true));
}

// Proven (both decks): healthy, and able to take the next update: its listener is up, unless
// updates are off (then nothing could ever prove itself). An image that broke the listener but
// polls fine must not become "good": USB would be the only way back.
void test_proven_needs_the_update_listener_when_updates_are_on() {
    TEST_ASSERT_TRUE(bootguard::proven(60000, true, true, true));
    TEST_ASSERT_FALSE(bootguard::proven(60000, true, true, false));    // listener down
    TEST_ASSERT_TRUE(bootguard::proven(60000, true, false, false));    // updates off
    TEST_ASSERT_FALSE(bootguard::proven(59999, true, true, true));     // not up long enough
    TEST_ASSERT_FALSE(bootguard::proven(60000, false, true, true));    // no sign it works
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_crashes_count_up_in_a_row);
    RUN_TEST(test_any_other_boot_clears_the_count);
    RUN_TEST(test_rolls_back_only_new_firmware_with_somewhere_to_go);
    RUN_TEST(test_healthy_needs_both_time_and_a_poll);
    RUN_TEST(test_proven_needs_the_update_listener_when_updates_are_on);
    return UNITY_END();
}
