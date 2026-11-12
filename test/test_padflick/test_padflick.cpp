#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padflick.h"

void setUp() {}
void tearDown() {}

void test_a_flick_right_steps_once() {
    padflick::Flick f;
    TEST_ASSERT_EQUAL(0, f.feed(0));
    TEST_ASSERT_EQUAL(0, f.feed(50));
    TEST_ASSERT_EQUAL(1, f.feed(95));
    TEST_ASSERT_EQUAL(0, f.feed(127));
}

void test_a_held_flick_steps_once() {
    padflick::Flick f;
    int steps = 0;
    for (int i = 0; i < 20; i++) steps += f.feed(127);
    TEST_ASSERT_EQUAL(1, steps);
}

void test_it_rearms_at_centre() {
    padflick::Flick f;
    TEST_ASSERT_EQUAL(1, f.feed(127));
    TEST_ASSERT_EQUAL(0, f.feed(40));   // not back to centre yet
    TEST_ASSERT_EQUAL(0, f.feed(127));
    TEST_ASSERT_EQUAL(0, f.feed(10));
    TEST_ASSERT_EQUAL(1, f.feed(127));
}

void test_drift_never_flicks() {
    padflick::Flick f;
    for (int v = -20; v <= 20; v++) TEST_ASSERT_EQUAL(0, f.feed(static_cast<int8_t>(v)));
}

void test_left_is_minus_one() {
    padflick::Flick f;
    TEST_ASSERT_EQUAL(-1, f.feed(-128));
    TEST_ASSERT_EQUAL(0, f.feed(-128));
    TEST_ASSERT_EQUAL(0, f.feed(0));
    TEST_ASSERT_EQUAL(-1, f.feed(-90));
}

void test_reset_rearms() {
    padflick::Flick f;
    TEST_ASSERT_EQUAL(1, f.feed(127));
    f.reset();
    TEST_ASSERT_EQUAL(1, f.feed(127));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_flick_right_steps_once);
    RUN_TEST(test_a_held_flick_steps_once);
    RUN_TEST(test_it_rearms_at_centre);
    RUN_TEST(test_drift_never_flicks);
    RUN_TEST(test_left_is_minus_one);
    RUN_TEST(test_reset_rearms);
    return UNITY_END();
}
