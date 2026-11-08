#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padfeel.h"

void setUp() {}
void tearDown() {}

void test_one_buzz_when_every_light_it_was_for_is_linked() {
    TEST_ASSERT_EQUAL_INT(1, padfeel::buzzes(true, true, true, true));
    TEST_ASSERT_EQUAL_INT(1, padfeel::buzzes(true, false, true, false));  // inside only, outside down: fine
}

void test_two_buzzes_when_one_it_was_for_is_down() {
    TEST_ASSERT_EQUAL_INT(2, padfeel::buzzes(true, true, true, false));
    TEST_ASSERT_EQUAL_INT(2, padfeel::buzzes(false, true, true, false));
    TEST_ASSERT_EQUAL_INT(2, padfeel::buzzes(true, true, false, false));
}

void test_leds_show_the_links_and_a_low_battery() {
    TEST_ASSERT_EQUAL_UINT8(1, padfeel::leds(false, false, false));  // LED 1: connected, whatever else
    TEST_ASSERT_EQUAL_UINT8(3, padfeel::leds(true, false, false));
    TEST_ASSERT_EQUAL_UINT8(7, padfeel::leds(true, true, false));
    TEST_ASSERT_EQUAL_UINT8(13, padfeel::leds(false, true, true));
}

void test_buzzes_come_apart_then_stop() {
    padfeel::Buzz b;
    TEST_ASSERT_FALSE(b.due(0));
    b.start(2, 1000);
    TEST_ASSERT_TRUE(b.due(1000));
    TEST_ASSERT_FALSE(b.due(1000 + padfeel::BUZZ_GAP_MS - 1));
    TEST_ASSERT_TRUE(b.due(1000 + padfeel::BUZZ_GAP_MS));
    TEST_ASSERT_FALSE(b.busy());
    TEST_ASSERT_FALSE(b.due(5000));
}

void test_buzz_survives_millis_wrap() {
    padfeel::Buzz b;
    b.start(2, 0xFFFFFFF0u);
    TEST_ASSERT_TRUE(b.due(0xFFFFFFF0u));
    TEST_ASSERT_FALSE(b.due(0x00000010u));  // 32 ms later, past the wrap
    TEST_ASSERT_TRUE(b.due(0xFFFFFFF0u + padfeel::BUZZ_GAP_MS));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_one_buzz_when_every_light_it_was_for_is_linked);
    RUN_TEST(test_two_buzzes_when_one_it_was_for_is_down);
    RUN_TEST(test_leds_show_the_links_and_a_low_battery);
    RUN_TEST(test_buzzes_come_apart_then_stop);
    RUN_TEST(test_buzz_survives_millis_wrap);
    return UNITY_END();
}
