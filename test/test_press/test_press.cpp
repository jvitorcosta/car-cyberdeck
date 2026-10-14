#include <unity.h>
#include "../../src/util/press.h"

void setUp() {}
void tearDown() {}

void test_tap_fires_on_release_inside() {
    press::Press p;
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 0));
    TEST_ASSERT_EQUAL_INT(3, p.pressed());
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 60));
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120));   // up: wait out a bounce
    TEST_ASSERT_EQUAL_INT(3, p.update(false, 0, false, 120 + press::RELEASE_MS));
    TEST_ASSERT_EQUAL_INT(0, p.pressed());
}
void test_sliding_out_cancels_the_tap() {
    press::Press p;
    p.update(true, 3, false, 0);
    p.update(true, 0, false, 60);                       // finger moved off the button
    p.update(false, 0, false, 120);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120 + press::RELEASE_MS));
}
void test_sliding_onto_another_button_fires_nothing() {
    press::Press p;
    p.update(true, 3, false, 0);
    p.update(true, 5, false, 60);
    p.update(false, 0, false, 120);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120 + press::RELEASE_MS));
}
void test_down_on_nothing_fires_nothing() {
    press::Press p;
    p.update(true, 0, false, 0);
    p.update(true, 3, false, 60);                       // sliding onto a button later
    p.update(false, 0, false, 120);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120 + press::RELEASE_MS));
}
void test_hold_fires_once_at_one_second() {
    press::Press p;
    p.update(true, 7, true, 1000);
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 1999));
    TEST_ASSERT_EQUAL_INT(7, p.update(true, 7, true, 2000));
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 2500));    // still held: not again
    p.update(false, 0, false, 2600);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 2600 + press::RELEASE_MS));   // no tap on release
}
void test_short_tap_on_a_hold_button_does_nothing() {
    press::Press p;
    p.update(true, 7, true, 0);
    p.update(true, 7, true, 999);
    p.update(false, 0, false, 1000);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 1000 + press::RELEASE_MS));
}
// Review Focus 2
void test_hold_restarts_after_sliding_back() {
    press::Press p;
    p.update(true, 7, true, 0);
    p.update(true, 0, true, 600);                       // slid off
    p.update(true, 7, true, 700);                       // back on: the second starts again
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 1600));
    TEST_ASSERT_EQUAL_INT(7, p.update(true, 7, true, 1700));
}
void test_hold_progress() {
    press::Press p;
    p.update(true, 7, true, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, p.holdProgress(500));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, p.holdProgress(3000));
    p.update(true, 0, true, 600);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, p.holdProgress(600));   // off the button
    press::Press q;
    q.update(true, 3, false, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, q.holdProgress(500));   // not a hold button
}
// Review Focus 1 (final review I1): a one-sample lift mid-press is a bounce, not a release:
// across the whole touch, the button fires exactly once.
void test_bounce_is_a_new_press_not_two_fires_of_one() {
    press::Press p;
    int fires = 0;
    fires += p.update(true, 3, false, 0) == 3;
    fires += p.update(false, 0, false, 20) == 3;       // the bounce
    fires += p.update(true, 3, false, 40) == 3;        // the same press continues
    fires += p.update(true, 3, false, 60) == 3;
    fires += p.update(false, 0, false, 200) == 3;      // the real release
    fires += p.update(false, 0, false, 200 + press::RELEASE_MS) == 3;
    fires += p.update(false, 0, false, 400) == 3;
    TEST_ASSERT_EQUAL_INT(1, fires);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tap_fires_on_release_inside);
    RUN_TEST(test_sliding_out_cancels_the_tap);
    RUN_TEST(test_sliding_onto_another_button_fires_nothing);
    RUN_TEST(test_down_on_nothing_fires_nothing);
    RUN_TEST(test_hold_fires_once_at_one_second);
    RUN_TEST(test_short_tap_on_a_hold_button_does_nothing);
    RUN_TEST(test_hold_restarts_after_sliding_back);
    RUN_TEST(test_hold_progress);
    RUN_TEST(test_bounce_is_a_new_press_not_two_fires_of_one);
    return UNITY_END();
}
