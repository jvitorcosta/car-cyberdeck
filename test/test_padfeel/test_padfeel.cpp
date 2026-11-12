#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padfeel.h"

using padfeel::Buzz;
using padfeel::Feel;
using padfeel::Pulse;

void setUp() {}
void tearDown() {}

void test_leds_show_the_links_and_a_low_battery() {
    TEST_ASSERT_EQUAL_UINT8(1, padfeel::leds(false, false, false));  // LED 1: connected, whatever else
    TEST_ASSERT_EQUAL_UINT8(3, padfeel::leds(true, false, false));
    TEST_ASSERT_EQUAL_UINT8(7, padfeel::leds(true, true, false));
    TEST_ASSERT_EQUAL_UINT8(13, padfeel::leds(false, true, true));
}

void test_tick_is_one_small_pulse() {
    Buzz b;
    Pulse p{};
    TEST_ASSERT_FALSE(b.due(0, &p));
    b.start(Feel::Tick, true, false, 1000);
    TEST_ASSERT_TRUE(b.due(1000, &p));
    TEST_ASSERT_EQUAL_UINT16(40, p.smallMs);
    TEST_ASSERT_EQUAL_UINT16(0, p.bigMs);
    TEST_ASSERT_FALSE(b.busy());
    TEST_ASSERT_FALSE(b.due(5000, &p));
}

void test_double_is_two_small_pulses_120_apart() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Double, true, false, 1000);
    TEST_ASSERT_TRUE(b.due(1000, &p));
    TEST_ASSERT_EQUAL_UINT16(40, p.smallMs);
    TEST_ASSERT_TRUE(b.busy());
    TEST_ASSERT_FALSE(b.due(1000 + 40 + 120 - 1, &p));
    TEST_ASSERT_TRUE(b.due(1000 + 40 + 120, &p));
    TEST_ASSERT_EQUAL_UINT16(40, p.smallMs);
    TEST_ASSERT_EQUAL_UINT16(0, p.bigMs);
    TEST_ASSERT_FALSE(b.busy());
}

void test_long_is_one_big_pulse() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Long, true, false, 0);
    TEST_ASSERT_TRUE(b.due(0, &p));
    TEST_ASSERT_EQUAL_UINT16(0, p.smallMs);
    TEST_ASSERT_EQUAL_UINT16(400, p.bigMs);
    TEST_ASSERT_EQUAL_UINT8(160, p.bigPower);
    TEST_ASSERT_FALSE(b.busy());
}

void test_firm_is_one_strong_big_pulse() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Firm, true, false, 0);
    TEST_ASSERT_TRUE(b.due(0, &p));
    TEST_ASSERT_EQUAL_UINT16(150, p.bigMs);
    TEST_ASSERT_EQUAL_UINT8(255, p.bigPower);
}

void test_off_moves_small_to_big_and_softens() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Tick, false, false, 0);
    TEST_ASSERT_TRUE(b.due(0, &p));
    TEST_ASSERT_EQUAL_UINT16(0, p.smallMs);
    TEST_ASSERT_EQUAL_UINT16(40, p.bigMs);
    TEST_ASSERT_EQUAL_UINT8(90, p.bigPower);

    b.start(Feel::Firm, false, false, 100);
    TEST_ASSERT_TRUE(b.due(100, &p));
    TEST_ASSERT_EQUAL_UINT16(150, p.bigMs);
    TEST_ASSERT_EQUAL_UINT8(127, p.bigPower);
}

void test_partial_link_adds_a_weak_big_pulse() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Firm, true, true, 1000);
    TEST_ASSERT_TRUE(b.due(1000, &p));
    TEST_ASSERT_EQUAL_UINT8(255, p.bigPower);
    TEST_ASSERT_FALSE(b.due(1000 + 150 + 160 - 1, &p));
    TEST_ASSERT_TRUE(b.due(1000 + 150 + 160, &p));
    TEST_ASSERT_EQUAL_UINT16(0, p.smallMs);
    TEST_ASSERT_EQUAL_UINT16(200, p.bigMs);
    TEST_ASSERT_EQUAL_UINT8(90, p.bigPower);
    TEST_ASSERT_FALSE(b.busy());
}

void test_a_new_start_replaces_a_running_one() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Double, true, false, 1000);
    TEST_ASSERT_TRUE(b.due(1000, &p));
    b.start(Feel::Long, true, false, 1050);
    TEST_ASSERT_TRUE(b.due(1050, &p));
    TEST_ASSERT_EQUAL_UINT16(400, p.bigMs);
    TEST_ASSERT_FALSE(b.busy());
}

void test_buzz_survives_millis_wrap() {
    Buzz b;
    Pulse p{};
    b.start(Feel::Double, true, false, 0xFFFFFFF0u);
    TEST_ASSERT_TRUE(b.due(0xFFFFFFF0u, &p));
    TEST_ASSERT_FALSE(b.due(0x00000010u, &p));  // 32 ms later, past the wrap
    TEST_ASSERT_TRUE(b.due(0xFFFFFFF0u + 160, &p));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_leds_show_the_links_and_a_low_battery);
    RUN_TEST(test_tick_is_one_small_pulse);
    RUN_TEST(test_double_is_two_small_pulses_120_apart);
    RUN_TEST(test_long_is_one_big_pulse);
    RUN_TEST(test_firm_is_one_strong_big_pulse);
    RUN_TEST(test_off_moves_small_to_big_and_softens);
    RUN_TEST(test_partial_link_adds_a_weak_big_pulse);
    RUN_TEST(test_a_new_start_replaces_a_running_one);
    RUN_TEST(test_buzz_survives_millis_wrap);
    return UNITY_END();
}
