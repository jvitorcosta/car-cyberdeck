#include <unity.h>
#include <cstdint>
#include "../../src/black/ui/sunset.h"
#include "../../src/black/ui/statusscene.h"

using sunset::DAY;
using sunset::NIGHT;

void setUp() {}
void tearDown() {}

static int red(uint16_t c) { return c >> 11; }
static int blue(uint16_t c) { return c & 31; }

// Day: purple at the top, warm orange at the horizon. Night: navy throughout.
void test_sky_runs_purple_to_orange_by_day_navy_by_night() {
    const uint16_t top = sunset::pixel(10, sunset::SKY_Y + 2, false);
    const uint16_t low = sunset::pixel(30, sunset::GROUND_Y - 60, false);   // clear of buildings
    TEST_ASSERT_EQUAL_HEX16(DAY.sky[0], top);
    TEST_ASSERT_TRUE(red(DAY.sky[6]) > red(DAY.sky[0]));
    TEST_ASSERT_TRUE(red(low) > red(top));
    for (int i = 0; i < 7; ++i) TEST_ASSERT_TRUE(blue(NIGHT.sky[i]) > red(NIGHT.sky[i]));
}

// Band edges are dithered, Gen-3 style: the boundary row mixes both bands in a checker.
void test_band_edges_are_dithered() {
    const int edge = sunset::SKY_Y + 14;   // first row of band 1
    int up = 0, down = 0;
    for (int x = 0; x < 40; ++x) {
        uint16_t v = sunset::pixel(x, edge, false);
        up += v == DAY.sky[0];
        down += v == DAY.sky[1];
    }
    TEST_ASSERT_EQUAL_INT(20, up);
    TEST_ASSERT_EQUAL_INT(20, down);
}

// The sun sits low on the right by day, cut by stripes in its lower half; it is gone at night.
void test_sun_by_day_only_with_stripes() {
    TEST_ASSERT_EQUAL_HEX16(DAY.sun, sunset::pixel(sunset::SUN_X, sunset::SUN_Y - 20, false));
    TEST_ASSERT_NOT_EQUAL(DAY.sun, sunset::pixel(sunset::SUN_X, sunset::SUN_Y - 20, true));
    bool gap = false;
    for (int y = sunset::SUN_Y - 4; y < sunset::GROUND_Y; ++y) {
        uint16_t v = sunset::pixel(sunset::SUN_X - 20, y, false);
        if (v != DAY.sun && v != DAY.near && v != DAY.far) gap = true;   // sky through a stripe
    }
    TEST_ASSERT_TRUE(gap);
}

// Stars only at night, in the upper sky.
void test_stars_only_at_night() {
    int day = 0, night = 0;
    for (int y = sunset::SKY_Y; y < 90; ++y)
        for (int x = 0; x < 320; ++x) {
            day += sunset::pixel(x, y, false) == NIGHT.star;
            night += sunset::pixel(x, y, true) == NIGHT.star;
        }
    TEST_ASSERT_EQUAL_INT(0, day);
    TEST_ASSERT_TRUE(night > 20 && night < 200);   // a sprinkle, not snow
}

// The skyline stands on the ground line; more windows are lit at night.
void test_skyline_with_more_windows_lit_at_night() {
    int dayLit = 0, nightLit = 0;
    for (int y = 60; y < sunset::GROUND_Y; ++y)
        for (int x = 0; x < 320; ++x) {
            dayLit += sunset::pixel(x, y, false) == DAY.window;
            nightLit += sunset::pixel(x, y, true) == NIGHT.window;
        }
    TEST_ASSERT_TRUE(dayLit > 40);
    TEST_ASSERT_TRUE(nightLit > dayLit * 3 / 2);
    int buildings = 0;   // just above the ground: mostly buildings, little sky
    for (int x = 0; x < 320; ++x) {
        uint16_t v = sunset::pixel(x, sunset::GROUND_Y - 1, false);
        buildings += v == DAY.near || v == DAY.far || v == DAY.window || v == DAY.lampPost;
    }
    TEST_ASSERT_TRUE(buildings > 280);
}

// Sonic waits on the sidewalk; the curb, then the road with its lane dashes above the dialogue box.
void test_sidewalk_under_sonic_then_curb_and_road() {
    TEST_ASSERT_EQUAL_HEX16(DAY.sidewalk, sunset::pixel(statusscene::CX, statusscene::FEET_Y, false));
    TEST_ASSERT_TRUE(statusscene::FEET_Y > sunset::GROUND_Y && statusscene::FEET_Y < sunset::CURB_Y);
    TEST_ASSERT_EQUAL_HEX16(DAY.curb, sunset::pixel(10, sunset::CURB_Y, false));
    TEST_ASSERT_EQUAL_HEX16(DAY.road, sunset::pixel(1, 239, false));   // under the dialogue box
    int dashes = 0;
    for (int y = sunset::ROAD_Y; y < statusscene::DLG_Y; ++y)
        for (int x = 0; x < 320; ++x) dashes += sunset::pixel(x, y, false) == DAY.dash;
    TEST_ASSERT_TRUE(dashes > 100);
    TEST_ASSERT_EQUAL_HEX16(NIGHT.road, sunset::pixel(1, 239, true));
}

// The lamps glow both by day (it is dusk) and at night.
void test_two_lamps_lit() {
    int lit = 0;
    for (int y = sunset::SKY_Y; y < sunset::GROUND_Y; ++y)
        for (int x = 0; x < 320; ++x) lit += sunset::pixel(x, y, false) == DAY.lampLit;
    TEST_ASSERT_TRUE(lit >= 2 * 12);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sky_runs_purple_to_orange_by_day_navy_by_night);
    RUN_TEST(test_band_edges_are_dithered);
    RUN_TEST(test_sun_by_day_only_with_stripes);
    RUN_TEST(test_stars_only_at_night);
    RUN_TEST(test_skyline_with_more_windows_lit_at_night);
    RUN_TEST(test_sidewalk_under_sonic_then_curb_and_road);
    RUN_TEST(test_two_lamps_lit);
    return UNITY_END();
}
