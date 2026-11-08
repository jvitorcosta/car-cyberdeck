#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padbattery.h"

void setUp() {}
void tearDown() {}

void test_levels_map_to_cells() {
    TEST_ASSERT_EQUAL_INT(4, padbattery::of(0x05).cells);
    TEST_ASSERT_EQUAL_INT(4, padbattery::of(0xEF).cells);  // charged, still on USB
    TEST_ASSERT_EQUAL_INT(3, padbattery::of(0x04).cells);
    TEST_ASSERT_EQUAL_INT(2, padbattery::of(0x03).cells);
    TEST_ASSERT_EQUAL_INT(1, padbattery::of(0x02).cells);
    TEST_ASSERT_EQUAL_INT(0, padbattery::of(0x01).cells);
}

void test_only_dying_and_shutdown_alert() {
    TEST_ASSERT_TRUE(padbattery::of(0x02).alert);
    TEST_ASSERT_TRUE(padbattery::of(0x01).alert);
    TEST_ASSERT_FALSE(padbattery::of(0x03).alert);
    TEST_ASSERT_FALSE(padbattery::of(0xEE).alert);
}

void test_unknown_shows_the_outline_alone() {
    const padbattery::Look l = padbattery::of(0x00);
    TEST_ASSERT_EQUAL_INT(-1, l.cells);
    TEST_ASSERT_FALSE(l.alert);
    TEST_ASSERT_FALSE(l.charging);
    TEST_ASSERT_EQUAL_INT(-1, padbattery::of(0x42).cells);
}

void test_an_unknown_byte_keeps_the_last_level() {
    const padbattery::Look full = padbattery::of(0x05);
    TEST_ASSERT_TRUE(padbattery::next(full, 0x0F) == full);           // the DS3's odd 0x0F
    TEST_ASSERT_EQUAL_INT(2, padbattery::next(full, 0x03).cells);     // a real level still moves it
    TEST_ASSERT_EQUAL_INT(-1, padbattery::next(padbattery::Look{}, 0x0F).cells);  // nothing known yet
}

void test_charging_fills_round() {
    const padbattery::Look l = padbattery::of(0xEE);
    TEST_ASSERT_TRUE(l.charging);
    TEST_ASSERT_EQUAL_INT(0, padbattery::shownCells(l, 0));
    TEST_ASSERT_EQUAL_INT(1, padbattery::shownCells(l, 500));
    TEST_ASSERT_EQUAL_INT(4, padbattery::shownCells(l, 2499));
    TEST_ASSERT_EQUAL_INT(0, padbattery::shownCells(l, 2500));
    TEST_ASSERT_EQUAL_INT(3, padbattery::shownCells(padbattery::of(0x04), 12345));  // a level doesn't move
}

void test_low_message_is_an_alert() {
    TEST_ASSERT_TRUE(padbattery::isAlert(padbattery::LOW_MESSAGE));
    TEST_ASSERT_FALSE(padbattery::isAlert(">ROSE APPLIED"));
    TEST_ASSERT_FALSE(padbattery::isAlert(nullptr));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_levels_map_to_cells);
    RUN_TEST(test_only_dying_and_shutdown_alert);
    RUN_TEST(test_unknown_shows_the_outline_alone);
    RUN_TEST(test_an_unknown_byte_keeps_the_last_level);
    RUN_TEST(test_charging_fills_round);
    RUN_TEST(test_low_message_is_an_alert);
    return UNITY_END();
}
