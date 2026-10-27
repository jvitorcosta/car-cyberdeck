#include <unity.h>
#include "../../src/util/hue.h"

void setUp() {}
void tearDown() {}

static int hueOf(int r, int g, int b, int turn) {
    int h, s;
    hue::hueSat(r, g, b, turn, 100, &h, &s);
    return h;
}
static int satOf(int r, int g, int b, int full) {
    int h, s;
    hue::hueSat(r, g, b, 360, full, &h, &s);
    return s;
}

void test_primaries_and_secondaries_in_degrees_and_tenths() {
    TEST_ASSERT_EQUAL_INT(0, hueOf(255, 0, 0, 360));
    TEST_ASSERT_EQUAL_INT(60, hueOf(255, 255, 0, 360));
    TEST_ASSERT_EQUAL_INT(120, hueOf(0, 255, 0, 360));
    TEST_ASSERT_EQUAL_INT(180, hueOf(0, 255, 255, 360));
    TEST_ASSERT_EQUAL_INT(240, hueOf(0, 0, 255, 360));
    TEST_ASSERT_EQUAL_INT(300, hueOf(255, 0, 255, 360));
    TEST_ASSERT_EQUAL_INT(2400, hueOf(0, 0, 255, 3600));
    TEST_ASSERT_EQUAL_INT(2729, hueOf(140, 0, 255, 3600));  // ledlook's purple LED mix
}

// The step is truncated toward zero, then wrapped: just under red is 352 degrees, not 351.
void test_truncation_below_red() {
    TEST_ASSERT_EQUAL_INT(352, hueOf(7, 0, 1, 360));
    TEST_ASSERT_EQUAL_INT(3515, hueOf(7, 0, 1, 3600));
}

void test_greys_have_no_hue_and_black_no_saturation() {
    TEST_ASSERT_EQUAL_INT(0, hueOf(128, 128, 128, 360));
    TEST_ASSERT_EQUAL_INT(0, satOf(128, 128, 128, 100));
    TEST_ASSERT_EQUAL_INT(0, satOf(0, 0, 0, 255));
}

void test_saturation_out_of_full() {
    TEST_ASSERT_EQUAL_INT(100, satOf(255, 0, 0, 100));
    TEST_ASSERT_EQUAL_INT(255, satOf(255, 0, 0, 255));
    TEST_ASSERT_EQUAL_INT(33, satOf(255, 215, 170, 100));  // ledlook's warm white: (255-170)*100/255
    TEST_ASSERT_EQUAL_INT(85, satOf(255, 215, 170, 255));
}

void test_distance_is_circular() {
    TEST_ASSERT_EQUAL_INT(20, hue::dist(350, 10, 360));
    TEST_ASSERT_EQUAL_INT(20, hue::dist(10, 350, 360));
    TEST_ASSERT_EQUAL_INT(180, hue::dist(0, 180, 360));
    TEST_ASSERT_EQUAL_INT(0, hue::dist(0, 360, 360));
    TEST_ASSERT_EQUAL_INT(200, hue::dist(3500, 100, 3600));
    TEST_ASSERT_EQUAL_INT(1800, hue::dist(3000, 1200, 3600));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_primaries_and_secondaries_in_degrees_and_tenths);
    RUN_TEST(test_truncation_below_red);
    RUN_TEST(test_greys_have_no_hue_and_black_no_saturation);
    RUN_TEST(test_saturation_out_of_full);
    RUN_TEST(test_distance_is_circular);
    return UNITY_END();
}
