#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/obdwire.h"

using obd::Pid;
using obdplan::Link;

void setUp() {}
void tearDown() {}

static obdwire::Live sample() {
    obdwire::Live l;
    l.link = Link::Live;
    l.rpm = 2450.0f;
    l.kmh = 62.0f;
    l.coolC = 88.0f;
    l.intakeC = 34.0f;
    l.throttle = 18.4f;
    l.load = 41.2f;
    l.volts = 14.1f;
    l.kml = 12.4f;
    l.seq = 812;
    return l;
}

void test_encode_is_the_spec_line() {
    char out[obdwire::MAX_JSON];
    TEST_ASSERT_TRUE(obdwire::encode(sample(), out, sizeof out) > 0);
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"link\":3,\"rpm\":2450,\"kmh\":62,\"cool\":88,\"iat\":34,\"thr\":18.4,\"load\":41.2,"
        "\"volt\":14.1,\"kml\":12.4,\"seq\":812}",
        out);
}

void test_round_trip() {
    char out[obdwire::MAX_JSON];
    obdwire::encode(sample(), out, sizeof out);
    obdwire::Live b;
    TEST_ASSERT_TRUE(obdwire::decode(out, &b));
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)b.link);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2450.0f, b.rpm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 62.0f, b.kmh);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 88.0f, b.coolC);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 34.0f, b.intakeC);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.4f, b.throttle);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 41.2f, b.load);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.1f, b.volts);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 12.4f, b.kml);
    TEST_ASSERT_EQUAL_UINT32(812, b.seq);
}

void test_unknown_values_travel_as_null() {
    obdwire::Live l;   // link off, everything NAN
    l.volts = 12.6f;
    l.coolC = INFINITY;
    char out[obdwire::MAX_JSON];
    TEST_ASSERT_TRUE(obdwire::encode(l, out, sizeof out) > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"rpm\":null"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"cool\":null"));   // inf is not a reading
    obdwire::Live b = sample();
    TEST_ASSERT_TRUE(obdwire::decode(out, &b));
    TEST_ASSERT_EQUAL_INT((int)Link::Off, (int)b.link);
    TEST_ASSERT_TRUE(std::isnan(b.rpm));
    TEST_ASSERT_TRUE(std::isnan(b.coolC));
    TEST_ASSERT_TRUE(std::isnan(b.load));
    TEST_ASSERT_TRUE(std::isnan(b.kml));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 12.6f, b.volts);
}

void test_too_small_a_buffer_writes_nothing() {
    char out[40];
    TEST_ASSERT_EQUAL_UINT(0, obdwire::encode(sample(), out, sizeof out));
    // the widest real values still fit
    obdwire::Live w = sample();
    w.rpm = 16383.75f;
    w.kmh = 255.0f;
    w.coolC = -40.0f;
    w.intakeC = 215.0f;
    w.throttle = 100.0f;
    w.load = 100.0f;
    w.volts = 29.9f;
    w.kml = 99.9f;   // fueleco's cap
    w.seq = 4294967295u;
    char big[obdwire::MAX_JSON];
    TEST_ASSERT_TRUE(obdwire::encode(w, big, sizeof big) > 0);
}

void test_decode_refuses_junk_and_other_versions() {
    const char* bad[] = {
        "",
        "{",
        "not json",
        "{\"v\":2,\"link\":3,\"rpm\":800,\"seq\":1}",   // a newer format
        "{\"link\":3,\"rpm\":800,\"seq\":1}",            // no version
        "{\"v\":1,\"rpm\":800,\"seq\":1}",               // no link
        "{\"v\":1,\"link\":9,\"rpm\":800,\"seq\":1}",    // a link state that doesn't exist
        "{\"v\":1,\"link\":3,\"rpm\":800,\"seq\":1",     // cut off
    };
    for (const char* j : bad) {
        obdwire::Live b = sample();
        TEST_ASSERT_FALSE_MESSAGE(obdwire::decode(j, &b), j);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 2450.0f, b.rpm, j);   // untouched
    }
    TEST_ASSERT_FALSE(obdwire::decode(nullptr, nullptr));
}

void test_decode_ignores_order_and_unknown_keys() {
    obdwire::Live b;
    TEST_ASSERT_TRUE(obdwire::decode("{\"seq\":5,\"new\":\"x\",\"rpm\":900,\"link\":2,\"v\":1}", &b));
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)b.link);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 900.0f, b.rpm);
    TEST_ASSERT_TRUE(std::isnan(b.kmh));   // missing: unknown
    TEST_ASSERT_EQUAL_UINT32(5, b.seq);
}

void test_slot_folds_the_extended_temperatures() {
    TEST_ASSERT_EQUAL_INT((int)Pid::Coolant, (int)obdwire::slot(Pid::CoolantX));
    TEST_ASSERT_EQUAL_INT((int)Pid::Intake, (int)obdwire::slot(Pid::IntakeX));
    TEST_ASSERT_EQUAL_INT((int)Pid::Rpm, (int)obdwire::slot(Pid::Rpm));
    TEST_ASSERT_EQUAL_INT((int)Pid::Battery, (int)obdwire::slot(Pid::Battery));
}

void test_from_readings_takes_fresh_values_only() {
    constexpr int N = (int)Pid::COUNT;
    float value[N] = {};
    bool have[N] = {};
    uint32_t atMs[N] = {};
    const uint32_t now = 100000;
    auto put = [&](Pid p, float v, uint32_t age) {
        value[(int)p] = v;
        have[(int)p] = true;
        atMs[(int)p] = now - age;
    };
    put(Pid::Rpm, 2000.0f, 500);
    put(Pid::Speed, 50.0f, 4000);      // fast and 4 s old: stale
    put(Pid::Coolant, 90.0f, 20000);   // slow and 20 s old: fine
    put(Pid::Intake, 30.0f, 40000);    // slow and 40 s old: stale
    put(Pid::Throttle, 12.5f, 100);
    put(Pid::Battery, 14.2f, 1000);
    // Load never read
    const obdwire::Live l = obdwire::fromReadings(Link::Live, value, have, atMs, now);
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)l.link);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, l.rpm);
    TEST_ASSERT_TRUE(std::isnan(l.kmh));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, l.coolC);
    TEST_ASSERT_TRUE(std::isnan(l.intakeC));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, l.throttle);
    TEST_ASSERT_TRUE(std::isnan(l.load));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f, l.volts);
}

void test_from_readings_across_millis_wrap() {
    constexpr int N = (int)Pid::COUNT;
    float value[N] = {};
    bool have[N] = {};
    uint32_t atMs[N] = {};
    value[(int)Pid::Rpm] = 800.0f;
    have[(int)Pid::Rpm] = true;
    atMs[(int)Pid::Rpm] = 0xFFFFFF00u;   // 256 ms before the wrap, now 100 ms after it
    const obdwire::Live l = obdwire::fromReadings(Link::Live, value, have, atMs, 100);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 800.0f, l.rpm);
}

void test_seq_moves_only_with_a_new_reading() {
    constexpr int N = (int)Pid::COUNT;
    bool have[N] = {};
    uint32_t atMs[N] = {};
    obdwire::Seq seq;
    TEST_ASSERT_EQUAL_UINT32(0, seq.update(have, atMs));   // nothing read yet
    have[(int)Pid::Rpm] = true;
    atMs[(int)Pid::Rpm] = 1000;
    TEST_ASSERT_EQUAL_UINT32(1, seq.update(have, atMs));
    TEST_ASSERT_EQUAL_UINT32(1, seq.update(have, atMs));   // the same readings: no bump
    have[(int)Pid::Speed] = true;
    atMs[(int)Pid::Speed] = 900;                           // older than the newest: no bump
    TEST_ASSERT_EQUAL_UINT32(1, seq.update(have, atMs));
    atMs[(int)Pid::Speed] = 1100;
    TEST_ASSERT_EQUAL_UINT32(2, seq.update(have, atMs));
    atMs[(int)Pid::Rpm] = 0xFFFFFFF0u;                     // before the wrap...
    atMs[(int)Pid::Speed] = 0xFFFFFFF0u;
    TEST_ASSERT_EQUAL_UINT32(3, seq.update(have, atMs));
    atMs[(int)Pid::Rpm] = 20;                              // ...and after it: still newer
    TEST_ASSERT_EQUAL_UINT32(4, seq.update(have, atMs));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_encode_is_the_spec_line);
    RUN_TEST(test_round_trip);
    RUN_TEST(test_unknown_values_travel_as_null);
    RUN_TEST(test_too_small_a_buffer_writes_nothing);
    RUN_TEST(test_decode_refuses_junk_and_other_versions);
    RUN_TEST(test_decode_ignores_order_and_unknown_keys);
    RUN_TEST(test_slot_folds_the_extended_temperatures);
    RUN_TEST(test_from_readings_takes_fresh_values_only);
    RUN_TEST(test_from_readings_across_millis_wrap);
    RUN_TEST(test_seq_moves_only_with_a_new_reading);
    return UNITY_END();
}
