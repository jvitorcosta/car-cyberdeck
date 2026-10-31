#include <unity.h>
#include <cmath>
#include "../../src/util/car/benchdrive.h"

using namespace benchdrive;
using obdplan::Link;

void setUp() {}
void tearDown() {}

static constexpr uint32_t CYCLE2 = CYCLE_MS;   // the second cycle: warmed up, no start-up

void test_starts_connecting_then_no_engine_then_live() {
    TEST_ASSERT_EQUAL(Link::Connecting, at(0).link);
    TEST_ASSERT_TRUE(std::isnan(at(0).kmh));
    TEST_ASSERT_EQUAL(Link::NoEngine, at(CONNECT_MS).link);
    TEST_ASSERT_TRUE(std::isnan(at(CONNECT_MS).volts));
    TEST_ASSERT_EQUAL(Link::Live, at(LIVE_MS).link);
    TEST_ASSERT_FALSE(std::isnan(at(LIVE_MS).kmh));
}

void test_idles_still_then_drives_then_stops_for_screen_d() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, at(CYCLE2 + 20000).kmh);
    TEST_ASSERT_EQUAL_FLOAT(800.0f, at(CYCLE2 + 20000).rpm);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, at(CYCLE2 + 100000).kmh);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, at(CYCLE2 + 185000).kmh);   // well past D's 5 s
}

void test_a_trip_passes_screen_ds_half_km_before_the_first_stop() {
    float km = 0.0f;
    for (uint32_t ms = LIVE_MS; ms < 170000; ms += 1000) km += at(ms).kmh / 3600.0f;
    TEST_ASSERT_TRUE(km >= 0.5f);
}

void test_speed_never_jumps_and_stays_in_range() {
    float last = at(CYCLE2).kmh;
    for (uint32_t ms = CYCLE2; ms < 2 * CYCLE_MS; ms += 1000) {
        const Sample s = at(ms);
        TEST_ASSERT_TRUE(s.kmh >= 0.0f && s.kmh <= 70.0f);
        TEST_ASSERT_TRUE(s.rpm >= 800.0f && s.rpm < 6000.0f);
        TEST_ASSERT_TRUE(s.mafGs > 0.0f);
        TEST_ASSERT_TRUE(fabsf(s.kmh - last) < 3.0f);
        last = s.kmh;
    }
}

void test_warms_up_to_ninety() {
    TEST_ASSERT_TRUE(at(LIVE_MS).coolantC < 50.0f);
    TEST_ASSERT_EQUAL_FLOAT(90.0f, at(CYCLE2).coolantC);
}

void test_overheat_comes_and_goes_while_moving() {
    const Sample s = at(CYCLE2 + HOT_FROM_S * 1000);
    TEST_ASSERT_EQUAL_FLOAT(HOT_C, s.coolantC);
    TEST_ASSERT_TRUE(s.kmh > 30.0f);
    TEST_ASSERT_EQUAL_FLOAT(90.0f, at(CYCLE2 + HOT_TO_S * 1000).coolantC);
}

void test_low_battery_outlasts_the_sixty_second_hold() {
    TEST_ASSERT_TRUE(LOWV_TO_S - LOWV_FROM_S > 60);
    TEST_ASSERT_EQUAL_FLOAT(LOW_V, at(CYCLE2 + LOWV_FROM_S * 1000).volts);
    TEST_ASSERT_EQUAL_FLOAT(14.1f, at(CYCLE2 + LOWV_TO_S * 1000).volts);
}

void test_code_and_mil_come_late_and_clear_with_the_cycle() {
    TEST_ASSERT_FALSE(at(CYCLE2 + (CODE_FROM_S - 1) * 1000).mil);
    const Sample s = at(CYCLE2 + CODE_FROM_S * 1000);
    TEST_ASSERT_TRUE(s.mil);
    TEST_ASSERT_EQUAL_INT(1, s.stored);
    TEST_ASSERT_FALSE(at(2 * CYCLE_MS).mil);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_connecting_then_no_engine_then_live);
    RUN_TEST(test_idles_still_then_drives_then_stops_for_screen_d);
    RUN_TEST(test_a_trip_passes_screen_ds_half_km_before_the_first_stop);
    RUN_TEST(test_speed_never_jumps_and_stays_in_range);
    RUN_TEST(test_warms_up_to_ninety);
    RUN_TEST(test_overheat_comes_and_goes_while_moving);
    RUN_TEST(test_low_battery_outlasts_the_sixty_second_hold);
    RUN_TEST(test_code_and_mil_come_late_and_clear_with_the_cycle);
    return UNITY_END();
}
