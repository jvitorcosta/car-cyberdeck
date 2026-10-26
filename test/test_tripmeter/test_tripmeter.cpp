#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/util/tripmeter.h"

void setUp() {}
void tearDown() {}

static tripmeter::Trip tripOf(uint32_t sec, uint32_t m, bool has) {
    tripmeter::Trip t;
    t.seconds = sec;
    t.meters = m;
    t.hasDistance = has;
    return t;
}

void test_sixty_kmh_for_a_minute_is_a_kilometre() {
    tripmeter::Meter m;
    m.addSpeed(60, 60000);
    TEST_ASSERT_EQUAL_UINT32(1000, m.trip().meters);
}

void test_tiny_steps_do_not_drift() {
    tripmeter::Meter m;
    for (uint32_t i = 0; i < 3600000u; i++) m.addSpeed(1, 1);
    TEST_ASSERT_EQUAL_UINT32(1000, m.trip().meters);
}

void test_large_step_does_not_overflow() {
    tripmeter::Meter m;
    m.addSpeed(65535, 200000000u);  // 65535 * 2e8 overflows 32 bits; = 3,640,833,333 m
    TEST_ASSERT_EQUAL_UINT32(3640833333u, m.trip().meters);
}

void test_remainder_carries_across_calls() {
    tripmeter::Meter m;
    m.addSpeed(7, 1000);  // 1.944 m
    TEST_ASSERT_EQUAL_UINT32(1, m.trip().meters);
    m.addSpeed(7, 1000);  // 3.888 m
    TEST_ASSERT_EQUAL_UINT32(3, m.trip().meters);
    m.addSpeed(7, 1000);  // 5.833 m
    m.addSpeed(7, 1000);  // 7.777 m
    TEST_ASSERT_EQUAL_UINT32(7, m.trip().meters);
}

void test_has_distance_only_after_a_speed() {
    tripmeter::Meter m;
    TEST_ASSERT_FALSE(m.trip().hasDistance);
    m.addSpeed(0, 1000);
    TEST_ASSERT_TRUE(m.trip().hasDistance);
    TEST_ASSERT_EQUAL_UINT32(0, m.trip().meters);
}

void test_seconds_are_kept() {
    tripmeter::Meter m;
    m.setSeconds(754);
    TEST_ASSERT_EQUAL_UINT32(754, m.trip().seconds);
}

// The dashboard's trip band: the time (h:mm) and, apart, the distance.
void test_elapsed() {
    char b[16];
    tripmeter::elapsed(tripOf(0, 0, false), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0:00", b);
    tripmeter::elapsed(tripOf(59 * 60 + 59, 0, false), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0:59", b);
    tripmeter::elapsed(tripOf(125 * 60, 0, false), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("2:05", b);
    tripmeter::elapsed(tripOf(12 * 3600 + 5 * 60, 1000, true), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("12:05", b);
    tripmeter::elapsed(tripOf(100 * 3600, 0, false), b, sizeof(b));  // hours aren't capped
    TEST_ASSERT_EQUAL_STRING("100:00", b);
}

void test_distance_rounds_down() {
    char b[16];
    tripmeter::distance(tripOf(32 * 60, 12345, true), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("12.3 KM", b);
    tripmeter::distance(tripOf(0, 50, true), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0.0 KM", b);
    tripmeter::distance(tripOf(0, 999900, true), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("999.9 KM", b);
}

// No speed source has fed the meter: no made-up 0.0, just the unit (the band draws it in DIM).
void test_distance_without_a_speed() {
    char b[16];
    tripmeter::distance(tripOf(32 * 60, 0, false), b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("-- KM", b);
}

void test_last_fits_the_message_buffer() {
    tripmeter::Trip t;
    t.seconds = 47 * 60;
    t.meters = 12400;
    t.hasDistance = true;
    char b[32];
    tripmeter::last(t, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 12.4 KM 0:47", b);
    t.hasDistance = false;
    tripmeter::last(t, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 0:47", b);  // no km part when there was none
    t.seconds = 99 * 3600 + 59 * 60;  // a long one
    t.meters = 999900;
    t.hasDistance = true;
    tripmeter::last(t, b, sizeof(b));
    TEST_ASSERT_TRUE(strlen(b) <= 31);
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 999.9 KM 99:59", b);
}

void test_encode_decode_round_trip() {
    tripmeter::Trip t;
    t.seconds = 0x01020304u;
    t.meters = 0x0A0B0C0Du;
    t.hasDistance = true;
    uint8_t buf[9];
    tripmeter::encode(t, buf);
    TEST_ASSERT_EQUAL_UINT8(0x04, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x0D, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(1, buf[8]);
    tripmeter::Trip u;
    TEST_ASSERT_TRUE(tripmeter::decode(buf, 9, &u));
    TEST_ASSERT_EQUAL_UINT32(t.seconds, u.seconds);
    TEST_ASSERT_EQUAL_UINT32(t.meters, u.meters);
    TEST_ASSERT_TRUE(u.hasDistance);
}

void test_decode_rejects_bad_input() {
    uint8_t buf[9] = {1, 0, 0, 0, 2, 0, 0, 0, 1};
    tripmeter::Trip u;
    u.seconds = 99;
    TEST_ASSERT_FALSE(tripmeter::decode(buf, 8, &u));
    TEST_ASSERT_EQUAL_UINT32(0, u.seconds);
    buf[8] = 2;
    u.seconds = 99;
    TEST_ASSERT_FALSE(tripmeter::decode(buf, 9, &u));
    TEST_ASSERT_EQUAL_UINT32(0, u.seconds);
    TEST_ASSERT_FALSE(tripmeter::decode(buf, 10, &u));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sixty_kmh_for_a_minute_is_a_kilometre);
    RUN_TEST(test_tiny_steps_do_not_drift);
    RUN_TEST(test_large_step_does_not_overflow);
    RUN_TEST(test_remainder_carries_across_calls);
    RUN_TEST(test_has_distance_only_after_a_speed);
    RUN_TEST(test_seconds_are_kept);
    RUN_TEST(test_elapsed);
    RUN_TEST(test_distance_rounds_down);
    RUN_TEST(test_distance_without_a_speed);
    RUN_TEST(test_last_fits_the_message_buffer);
    RUN_TEST(test_encode_decode_round_trip);
    RUN_TEST(test_decode_rejects_bad_input);
    return UNITY_END();
}
