#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/gaugehud.h"

using gaugehud::Zone;
using obdplan::Link;

void setUp() {}
void tearDown() {}

void test_blocks_lit_by_rpm() {
    TEST_ASSERT_EQUAL_INT(0, gaugehud::lit(NAN));
    TEST_ASSERT_EQUAL_INT(0, gaugehud::lit(-5.0f));
    TEST_ASSERT_EQUAL_INT(0, gaugehud::lit(0.0f));
    TEST_ASSERT_EQUAL_INT(0, gaugehud::lit(291.0f));        // under one block's worth
    TEST_ASSERT_EQUAL_INT(1, gaugehud::lit(292.0f));        // 7000 / 24 = 291.67
    TEST_ASSERT_EQUAL_INT(2, gaugehud::lit(800.0f));        // idle
    TEST_ASSERT_EQUAL_INT(12, gaugehud::lit(3500.0f));      // half
    TEST_ASSERT_EQUAL_INT(24, gaugehud::lit(7000.0f));
    TEST_ASSERT_EQUAL_INT(24, gaugehud::lit(16383.75f));    // clamped
}

void test_zones_follow_the_redline() {
    // a block's zone is that of the rpm at its lower edge
    TEST_ASSERT_EQUAL_INT((int)Zone::Green, (int)gaugehud::zone(0));
    TEST_ASSERT_EQUAL_INT((int)Zone::Green, (int)gaugehud::zone(17));   // 4958 rpm
    TEST_ASSERT_EQUAL_INT((int)Zone::Amber, (int)gaugehud::zone(18));   // 5250
    TEST_ASSERT_EQUAL_INT((int)Zone::Amber, (int)gaugehud::zone(22));   // 6417
    TEST_ASSERT_EQUAL_INT((int)Zone::Red, (int)gaugehud::zone(23));     // 6708
}

void test_blocks_rise_left_to_right() {
    TEST_ASSERT_EQUAL_INT(10, gaugehud::height(0, 10, 50));
    TEST_ASSERT_EQUAL_INT(50, gaugehud::height(gaugehud::SEGS - 1, 10, 50));
    for (int i = 1; i < gaugehud::SEGS; ++i)
        TEST_ASSERT_TRUE(gaugehud::height(i, 10, 50) >= gaugehud::height(i - 1, 10, 50));
}

void test_peak_holds_then_falls() {
    gaugehud::Peak p;
    TEST_ASSERT_EQUAL_INT(0, p.update(0, 0));
    TEST_ASSERT_EQUAL_INT(20, p.update(20, 1000));   // a rev
    TEST_ASSERT_EQUAL_INT(20, p.update(5, 1100));    // back to idle: the marker holds
    TEST_ASSERT_EQUAL_INT(20, p.update(5, 2499));    // still within HOLD_MS
    TEST_ASSERT_EQUAL_INT(19, p.update(5, 2560));    // 1.5 s + one fall step
    TEST_ASSERT_EQUAL_INT(18, p.update(5, 2620));
    TEST_ASSERT_EQUAL_INT(5, p.update(5, 9000));     // never under the blocks lit
    TEST_ASSERT_EQUAL_INT(9, p.update(9, 9010));     // a higher reading lifts it at once
}

void test_peak_falls_by_elapsed_steps() {
    gaugehud::Peak p;
    p.update(24, 0);
    TEST_ASSERT_EQUAL_INT(14, p.update(0, gaugehud::Peak::HOLD_MS + 10 * gaugehud::Peak::FALL_MS));
}

void test_shift_light_blinks_from_85_percent() {
    // REDLINE 6500: 85 % is 5525
    TEST_ASSERT_FALSE(gaugehud::shiftOn(5500.0f, 0));
    TEST_ASSERT_FALSE(gaugehud::shiftOn(NAN, 0));
    TEST_ASSERT_TRUE(gaugehud::shiftOn(5525.0f, 0));
    TEST_ASSERT_TRUE(gaugehud::shiftOn(6000.0f, 124));
    TEST_ASSERT_FALSE(gaugehud::shiftOn(6000.0f, 125));   // 4 Hz blink: off half the time
    TEST_ASSERT_TRUE(gaugehud::shiftOn(6000.0f, 250));
}

void test_number_text() {
    char t[8];
    gaugehud::text(NAN, 0, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("--", t);
    gaugehud::text(62.4f, 0, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("62", t);
    gaugehud::text(62.5f, 0, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("63", t);
    gaugehud::text(14.06f, 1, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("14.1", t);
    gaugehud::text(-40.0f, 0, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("-40", t);
    gaugehud::text(-0.2f, 0, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("0", t);         // no "-0"
    gaugehud::text(123456.0f, 0, t, 4);      // doesn't fit: dashes, never a cut number
    TEST_ASSERT_EQUAL_STRING("--", t);
}

void test_bar_cells() {
    TEST_ASSERT_EQUAL_INT(0, gaugehud::cells(NAN, 10));
    TEST_ASSERT_EQUAL_INT(0, gaugehud::cells(0.0f, 10));
    TEST_ASSERT_EQUAL_INT(0, gaugehud::cells(4.9f, 10));
    TEST_ASSERT_EQUAL_INT(1, gaugehud::cells(5.0f, 10));     // rounded
    TEST_ASSERT_EQUAL_INT(4, gaugehud::cells(41.2f, 10));
    TEST_ASSERT_EQUAL_INT(10, gaugehud::cells(100.0f, 10));
    TEST_ASSERT_EQUAL_INT(10, gaugehud::cells(130.0f, 10));  // clamped
    TEST_ASSERT_EQUAL_INT(0, gaugehud::cells(-3.0f, 10));
}

void test_warnings() {
    TEST_ASSERT_FALSE(gaugehud::coolWarn(NAN));
    TEST_ASSERT_FALSE(gaugehud::coolWarn(105.0f));
    TEST_ASSERT_TRUE(gaugehud::coolWarn(106.0f));
    TEST_ASSERT_FALSE(gaugehud::battWarn(NAN, 800.0f));
    TEST_ASSERT_FALSE(gaugehud::battWarn(12.3f, 800.0f));   // Honda smart charging at idle
    TEST_ASSERT_TRUE(gaugehud::battWarn(11.9f, 800.0f));
    TEST_ASSERT_TRUE(gaugehud::battWarn(12.1f, 0.0f));      // engine off: 12.2 V
    TEST_ASSERT_TRUE(gaugehud::battWarn(12.1f, NAN));       // rpm unknown counts as off
    TEST_ASSERT_FALSE(gaugehud::battWarn(12.4f, NAN));
}

void test_link_tags() {
    TEST_ASSERT_EQUAL_STRING("OFF", gaugehud::linkTag(Link::Off));
    TEST_ASSERT_EQUAL_STRING("SEARCH", gaugehud::linkTag(Link::Connecting));
    TEST_ASSERT_EQUAL_STRING("KEY ON", gaugehud::linkTag(Link::NoEngine));
    TEST_ASSERT_EQUAL_STRING("LIVE", gaugehud::linkTag(Link::Live));
}

void test_seven_segment_digits() {
    using namespace gaugehud;
    TEST_ASSERT_EQUAL_HEX8(0x7F, segments('8'));
    TEST_ASSERT_EQUAL_HEX8(SEG_B | SEG_C, segments('1'));
    TEST_ASSERT_EQUAL_HEX8(SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, segments('0'));
    TEST_ASSERT_EQUAL_HEX8(SEG_G, segments('-'));
    TEST_ASSERT_EQUAL_HEX8(0, segments(' '));
    TEST_ASSERT_EQUAL_HEX8(0, segments('x'));
    for (char c = '0'; c <= '9'; ++c) TEST_ASSERT_TRUE(segments(c) != 0);
}

void test_speed_cells() {
    char t[4];
    gaugehud::speedCells(NAN, t);
    TEST_ASSERT_EQUAL_STRING(" --", t);
    gaugehud::speedCells(0.0f, t);
    TEST_ASSERT_EQUAL_STRING("  0", t);
    gaugehud::speedCells(62.0f, t);
    TEST_ASSERT_EQUAL_STRING(" 62", t);
    gaugehud::speedCells(118.4f, t);
    TEST_ASSERT_EQUAL_STRING("118", t);
    gaugehud::speedCells(255.0f, t);
    TEST_ASSERT_EQUAL_STRING("255", t);
    gaugehud::speedCells(-3.0f, t);
    TEST_ASSERT_EQUAL_STRING("  0", t);
    gaugehud::speedCells(1200.0f, t);
    TEST_ASSERT_EQUAL_STRING("---", t);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_blocks_lit_by_rpm);
    RUN_TEST(test_zones_follow_the_redline);
    RUN_TEST(test_blocks_rise_left_to_right);
    RUN_TEST(test_peak_holds_then_falls);
    RUN_TEST(test_peak_falls_by_elapsed_steps);
    RUN_TEST(test_shift_light_blinks_from_85_percent);
    RUN_TEST(test_number_text);
    RUN_TEST(test_bar_cells);
    RUN_TEST(test_warnings);
    RUN_TEST(test_link_tags);
    RUN_TEST(test_seven_segment_digits);
    RUN_TEST(test_speed_cells);
    return UNITY_END();
}
