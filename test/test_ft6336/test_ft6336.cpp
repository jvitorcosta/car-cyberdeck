#include <unity.h>
#include "../../src/util/ft6336.h"

void setUp() {}
void tearDown() {}

void test_parse_one_touch() {
    // 1 touch, event "contact" (0x80) in XH's top bits, x = 0x0AB = 171; touch id in YH's top bits, y = 0x12C = 300
    const uint8_t r[5] = {0x01, 0x80 | 0x00, 0xAB, 0x10 | 0x01, 0x2C};
    ft6336::Raw p = ft6336::parse(r);
    TEST_ASSERT_TRUE(p.down);
    TEST_ASSERT_EQUAL_INT(171, p.x);
    TEST_ASSERT_EQUAL_INT(300, p.y);
}

void test_no_touch_or_bad_count_is_up() {
    const uint8_t none[5] = {0x00, 0, 0, 0, 0};
    const uint8_t bad[5] = {0x0F, 0, 5, 0, 5};
    TEST_ASSERT_FALSE(ft6336::parse(none).down);
    TEST_ASSERT_FALSE(ft6336::parse(bad).down);
    const uint8_t two[5] = {0x02, 0x00, 0x10, 0x00, 0x20};   // two fingers: the first one counts
    TEST_ASSERT_TRUE(ft6336::parse(two).down);
}

void test_to_screen_orientations() {
    const ft6336::Raw p{true, 10, 300};   // portrait 240 x 320
    int x, y;
    ft6336::toScreen(p, {true, false, true}, &x, &y);    // swap, then flip y
    TEST_ASSERT_EQUAL_INT(300, x);
    TEST_ASSERT_EQUAL_INT(229, y);
    ft6336::toScreen(p, {true, true, false}, &x, &y);
    TEST_ASSERT_EQUAL_INT(19, x);
    TEST_ASSERT_EQUAL_INT(10, y);
    ft6336::toScreen({true, 239, 319}, {true, false, false}, &x, &y);
    TEST_ASSERT_EQUAL_INT(319, x);
    TEST_ASSERT_EQUAL_INT(239, y);
    ft6336::toScreen({true, 400, -5}, {true, false, false}, &x, &y);   // out of range: clamped
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(239, y);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_one_touch);
    RUN_TEST(test_no_touch_or_bad_count_is_up);
    RUN_TEST(test_to_screen_orientations);
    return UNITY_END();
}
