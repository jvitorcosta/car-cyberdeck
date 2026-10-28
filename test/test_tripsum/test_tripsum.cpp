#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/tripsum.h"

void setUp() {}
void tearDown() {}

static tripsum::Sample smp(uint32_t ms, float kmh, float rpm, float volts = NAN, bool fresh = true,
                           float litres = NAN) {
    tripsum::Sample s{};
    s.nowMs = ms;
    s.kmh = kmh;
    s.rpm = rpm;
    s.litres = litres;
    s.volts = volts;
    s.coolantC = NAN;
    s.trimS = NAN;
    s.trimL = NAN;
    s.fresh = fresh;
    return s;
}

// The first sample only sets the clock, so n seconds of data need n+1 samples.
static void run(tripsum::Trip& t, uint32_t fromMs, int seconds, float kmh, float rpm, float volts = NAN,
                bool fresh = true, float litres = NAN) {
    for (int i = 0; i <= seconds; i++) t.add(smp(fromMs + 1000u * i, kmh, rpm, volts, fresh, litres));
}

void test_sixty_seconds_at_sixty_kmh_is_one_km_row() {
    tripsum::Trip t;
    run(t, 0, 60, 60.0f, 2000.0f);
    TEST_ASSERT_TRUE(t.rowReady());
    const tripsum::Row r = t.takeRow();
    TEST_ASSERT_FALSE(t.rowReady());
    TEST_ASSERT_EQUAL_UINT16(0, r.minute);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.km);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, r.avgKmh);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, r.maxKmh);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, r.avgRpm);
    TEST_ASSERT_TRUE(std::isnan(r.vAvg));
}

void test_stale_reading_adds_no_distance() {
    tripsum::Trip t;
    run(t, 0, 60, 60.0f, 2000.0f, NAN, false, 0.01f);
    const tripsum::Row r = t.takeRow();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.km);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.litres);
    TEST_ASSERT_TRUE(std::isnan(r.avgKmh));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, t.summary().km);
    TEST_ASSERT_EQUAL_UINT32(60, t.summary().seconds);  // time still passes
}

void test_stalled_loop_step_counts_three_seconds() {
    tripsum::Trip t;
    t.add(smp(0, 36.0f, 2000.0f));
    t.add(smp(10000, 36.0f, 2000.0f));  // 10 s gap -> 3 s: 36 km/h * 3 s = 30 m
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.030f, t.summary().km);
    TEST_ASSERT_EQUAL_UINT32(3, t.summary().seconds);
}

void test_second_minute_is_numbered_and_latest_row_wins() {
    tripsum::Trip t;
    run(t, 0, 120, 30.0f, 1500.0f);
    TEST_ASSERT_TRUE(t.rowReady());
    TEST_ASSERT_EQUAL_UINT16(1, t.takeRow().minute);  // two closed, none taken: the latest
    TEST_ASSERT_FALSE(t.rowReady());
}

void test_idle_counts_engine_on_and_stopped() {
    tripsum::Trip t;
    run(t, 0, 30, 0.0f, 800.0f);
    run(t, 31000, 29, 40.0f, 2000.0f);
    const tripsum::Row r = t.takeRow();
    TEST_ASSERT_EQUAL_UINT16(30, r.idleS);
    TEST_ASSERT_EQUAL_UINT32(30, t.summary().idleS);
}

void test_engine_off_is_not_idle() {
    tripsum::Trip t;
    run(t, 0, 60, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_UINT16(0, t.takeRow().idleS);
}

void test_row_voltage_and_flags() {
    tripsum::Trip t;
    for (int i = 0; i <= 60; i++) {
        t.add(smp(1000u * i, 50.0f, 2000.0f, i < 30 ? 13.0f : 14.0f));
        if (i == 5) t.orFlags(1);
    }
    const tripsum::Row r = t.takeRow();
    TEST_ASSERT_EQUAL_FLOAT(13.0f, r.vMin);
    TEST_ASSERT_EQUAL_FLOAT(14.0f, r.vMax);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 13.5f, r.vAvg);
    TEST_ASSERT_EQUAL_UINT8(1, r.flags);
}

void test_summary_kml_nan_without_fuel() {
    tripsum::Trip t;
    run(t, 0, 30, 60.0f, 2000.0f);
    const tripsum::Summary s = t.summary();
    TEST_ASSERT_TRUE(std::isnan(s.kmL));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, s.km);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, s.avgKmh);
}

void test_summary_kml_with_fuel() {
    tripsum::Trip t;
    run(t, 0, 60, 60.0f, 2000.0f, NAN, true, 0.001f);  // the clock sample's litres is not counted
    const tripsum::Summary s = t.summary();
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.060f, s.litres);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f / 0.060f, s.kmL);
}

void test_summary_avg_nan_for_no_time() {
    tripsum::Trip t;
    TEST_ASSERT_TRUE(std::isnan(t.summary().avgKmh));
    TEST_ASSERT_TRUE(std::isnan(t.recharge().peakV));
}

void test_recharge_profile_first_five_minutes() {
    tripsum::Trip t;
    run(t, 0, 90, 30.0f, 2000.0f, 14.4f);
    TEST_ASSERT_FALSE(t.recharge().done);
    run(t, 91000, 400, 30.0f, 2000.0f, 13.4f);
    const tripsum::Recharge r = t.recharge();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.4f, r.peakV);
    TEST_ASSERT_EQUAL_UINT16(90, r.secondsOver13_8);
    TEST_ASSERT_TRUE(r.done);
}

void test_recharge_waits_for_the_engine() {
    tripsum::Trip t;
    run(t, 0, 100, 0.0f, 0.0f, 12.6f);  // key on, engine off: the window is not open
    TEST_ASSERT_FALSE(t.recharge().done);
    TEST_ASSERT_TRUE(std::isnan(t.recharge().peakV));
}

static tripsum::Row exampleRow() {
    tripsum::Row r{};
    r.minute = 3;
    r.km = 0.62f;
    r.avgKmh = 37.1f;
    r.maxKmh = 52.0f;
    r.avgRpm = 1840.0f;
    r.litres = 0.051f;
    r.idleS = 4;
    r.vMin = 13.9f;
    r.vAvg = 14.1f;
    r.vMax = 14.4f;
    r.coolMax = 91.0f;
    r.trimS = 1.6f;
    r.trimL = 2.3f;
    r.flags = 0;
    return r;
}

void test_json_row_is_one_line_with_fixed_keys() {
    char buf[200];
    const size_t len = tripsum::jsonRow(exampleRow(), "t1", buf, sizeof(buf));
    const char* want =
        "{\"t\":\"t1\",\"m\":3,\"km\":0.62,\"kmh\":37.1,\"kmhMax\":52,\"rpm\":1840,\"l\":0.051,\"idle\":4,"
        "\"v\":[13.9,14.1,14.4],\"cool\":91,\"trim\":[1.6,2.3],\"f\":0}\n";
    TEST_ASSERT_EQUAL_STRING(want, buf);
    TEST_ASSERT_EQUAL_UINT32(strlen(want), len);
}

void test_json_row_unknowns_are_null() {
    tripsum::Row r = exampleRow();
    r.vMin = r.vAvg = r.vMax = NAN;
    r.coolMax = NAN;
    char buf[200];
    TEST_ASSERT_TRUE(tripsum::jsonRow(r, "t1", buf, sizeof(buf)) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"v\":[null,null,null],\"cool\":null"));
}

// JSON has no inf: a division gone wrong upstream must still give a parseable row.
void test_json_row_infinite_is_null() {
    tripsum::Row r = exampleRow();
    r.km = INFINITY;
    r.litres = -INFINITY;
    char buf[200];
    TEST_ASSERT_TRUE(tripsum::jsonRow(r, "t1", buf, sizeof(buf)) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"km\":null,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"l\":null,"));
    TEST_ASSERT_NULL(strstr(buf, "inf"));
}

void test_json_row_reports_no_fit() {
    char buf[20];
    TEST_ASSERT_EQUAL_UINT32(0, tripsum::jsonRow(exampleRow(), "t1", buf, 20));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sixty_seconds_at_sixty_kmh_is_one_km_row);
    RUN_TEST(test_stale_reading_adds_no_distance);
    RUN_TEST(test_stalled_loop_step_counts_three_seconds);
    RUN_TEST(test_second_minute_is_numbered_and_latest_row_wins);
    RUN_TEST(test_idle_counts_engine_on_and_stopped);
    RUN_TEST(test_engine_off_is_not_idle);
    RUN_TEST(test_row_voltage_and_flags);
    RUN_TEST(test_summary_kml_nan_without_fuel);
    RUN_TEST(test_summary_kml_with_fuel);
    RUN_TEST(test_summary_avg_nan_for_no_time);
    RUN_TEST(test_recharge_profile_first_five_minutes);
    RUN_TEST(test_recharge_waits_for_the_engine);
    RUN_TEST(test_json_row_is_one_line_with_fixed_keys);
    RUN_TEST(test_json_row_unknowns_are_null);
    RUN_TEST(test_json_row_infinite_is_null);
    RUN_TEST(test_json_row_reports_no_fit);
    return UNITY_END();
}
