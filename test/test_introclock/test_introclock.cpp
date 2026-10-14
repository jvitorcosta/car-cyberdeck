#include <unity.h>
#include "../../src/util/introclock.h"

void setUp() {}
void tearDown() {}

// The companion's intro video plays by the clock: frame i is due at i * 1000 / fps; a late
// decode skips ahead instead of slowing the clip down.
void test_frames_follow_the_clock() {
    TEST_ASSERT_EQUAL_INT(0, introclock::frameAt(0, 10, 100));
    TEST_ASSERT_EQUAL_INT(0, introclock::frameAt(99, 10, 100));
    TEST_ASSERT_EQUAL_INT(1, introclock::frameAt(100, 10, 100));
    TEST_ASSERT_EQUAL_INT(57, introclock::frameAt(5750, 10, 100));
}
void test_done_after_the_last_frame() {
    TEST_ASSERT_EQUAL_INT(99, introclock::frameAt(9999, 10, 100));
    TEST_ASSERT_EQUAL_INT(100, introclock::frameAt(10000, 10, 100));   // == count: finished
    TEST_ASSERT_EQUAL_INT(100, introclock::frameAt(60000, 10, 100));
}
void test_no_clip_is_done_at_once() {
    TEST_ASSERT_EQUAL_INT(0, introclock::frameAt(0, 10, 0));
    TEST_ASSERT_EQUAL_INT(50, introclock::frameAt(0, 0, 50));         // bad fps: done at once
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_frames_follow_the_clock);
    RUN_TEST(test_done_after_the_last_frame);
    RUN_TEST(test_no_clip_is_done_at_once);
    return UNITY_END();
}
