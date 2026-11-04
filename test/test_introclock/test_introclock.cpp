#include <unity.h>
#include "../../src/util/black/introclock.h"

void setUp() {}
void tearDown() {}

// The S3's intro video plays by the clock: frame i is due at i * 1000 / fps; a late
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

// The video's clock starts with its sound's first sample (lips match the voice): frame 0 holds
// until then, at most SOUND_WAIT_MS (a sound that never starts doesn't stall the video).
void test_clock_follows_the_sound() {
    using introclock::elapsed;
    TEST_ASSERT_EQUAL_UINT32(0, elapsed(1050, 1000, 0, true));        // sound not out yet: hold
    TEST_ASSERT_EQUAL_UINT32(0, elapsed(1100, 1000, 1100, true));     // its first sample
    TEST_ASSERT_EQUAL_UINT32(400, elapsed(1500, 1000, 1100, true));
    TEST_ASSERT_EQUAL_UINT32(0, elapsed(1000 + introclock::SOUND_WAIT_MS, 1000, 0, true));
    TEST_ASSERT_EQUAL_UINT32(50, elapsed(1050 + introclock::SOUND_WAIT_MS, 1000, 0, true));   // gave up
    TEST_ASSERT_EQUAL_UINT32(0, elapsed(1050, 1000, 900, true));      // an older clip's start: not ours
    TEST_ASSERT_EQUAL_UINT32(50, elapsed(1050, 1000, 0, false));      // a silent video: its own clock
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_frames_follow_the_clock);
    RUN_TEST(test_done_after_the_last_frame);
    RUN_TEST(test_no_clip_is_done_at_once);
    RUN_TEST(test_clock_follows_the_sound);
    return UNITY_END();
}
