#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/util/tripmeter.h"

void setUp() {}
void tearDown() {}

static void lineOf(uint32_t sec, uint32_t m, bool has, char* b, size_t n) {
    tripmeter::Trip t;
    t.seconds = sec;
    t.meters = m;
    t.hasDistance = has;
    tripmeter::line(t, b, n);
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

void test_line_times() {
    char b[40];
    lineOf(0, 0, false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP -- KM  0:00", b);
    lineOf(59 * 60, 0, false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP -- KM  0:59", b);
    lineOf(125 * 60, 0, false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP -- KM  2:05", b);
    lineOf(12 * 3600 + 5 * 60, 1000, true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP 1.0 KM  12:05", b);
}

void test_line_distance_rounds_down() {
    char b[40];
    lineOf(32 * 60, 12345, true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP 12.3 KM  0:32", b);
    lineOf(0, 50, true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("TRIP 0.0 KM  0:00", b);
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
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP -- KM 0:47", b);
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
    RUN_TEST(test_has_distance_only_after_a_speed);
    RUN_TEST(test_seconds_are_kept);
    RUN_TEST(test_line_times);
    RUN_TEST(test_line_distance_rounds_down);
    RUN_TEST(test_last_fits_the_message_buffer);
    RUN_TEST(test_encode_decode_round_trip);
    RUN_TEST(test_decode_rejects_bad_input);
    return UNITY_END();
}
