#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/drivedash.h"

using drivedash::Screen;

void setUp() {}
void tearDown() {}

void test_screens_by_name_or_number() {
    Screen s = Screen::Suki;
    TEST_ASSERT_TRUE(drivedash::parse("drive", &s));
    TEST_ASSERT_EQUAL_INT((int)Screen::Drive, (int)s);
    TEST_ASSERT_TRUE(drivedash::parse("SUKI", &s));
    TEST_ASSERT_EQUAL_INT((int)Screen::Suki, (int)s);
    TEST_ASSERT_TRUE(drivedash::parse("2", &s));
    TEST_ASSERT_EQUAL_INT((int)Screen::Drive, (int)s);
    TEST_ASSERT_TRUE(drivedash::parse("3", &s));
    TEST_ASSERT_EQUAL_INT((int)Screen::Trip, (int)s);
    TEST_ASSERT_TRUE(drivedash::parse("Trip", &s));
    TEST_ASSERT_EQUAL_INT((int)Screen::Trip, (int)s);
    TEST_ASSERT_FALSE(drivedash::parse("4", &s));
    TEST_ASSERT_FALSE(drivedash::parse("", &s));
    TEST_ASSERT_FALSE(drivedash::parse("drivedrivedrive", &s));
    TEST_ASSERT_FALSE(drivedash::parse(nullptr, &s));
    TEST_ASSERT_EQUAL_STRING("drive", drivedash::name(Screen::Drive));
}

void test_switching_is_for_a_parked_car() {
    TEST_ASSERT_TRUE(drivedash::canSwitch(0.0f));
    TEST_ASSERT_TRUE(drivedash::canSwitch(3.0f));
    TEST_ASSERT_FALSE(drivedash::canSwitch(3.5f));
    TEST_ASSERT_TRUE(drivedash::canSwitch(NAN));   // no link: parked
}

void test_seven_segments() {
    TEST_ASSERT_EQUAL_HEX8(0x7F, drivedash::segments('8'));
    TEST_ASSERT_EQUAL_HEX8(0x06, drivedash::segments('1'));   // b c
    TEST_ASSERT_EQUAL_HEX8(0x40, drivedash::segments('-'));   // g
    TEST_ASSERT_EQUAL_HEX8(0x00, drivedash::segments(' '));
    // a cell 50 x 56, strokes 9: a on top, d at the bottom, b and c on the right, nothing outside
    const drivedash::Box a = drivedash::segment(0, 66, 109, 50, 56, 9), d = drivedash::segment(3, 66, 109, 50, 56, 9);
    TEST_ASSERT_EQUAL_INT(109, a.y);
    TEST_ASSERT_EQUAL_INT(109 + 56 - 9, d.y);
    for (int k = 0; k < 7; ++k) {
        const drivedash::Box b = drivedash::segment(k, 66, 109, 50, 56, 9);
        TEST_ASSERT_TRUE(b.w > 0 && b.h > 0);
        TEST_ASSERT_TRUE(b.x >= 66 && b.x + b.w <= 66 + 50 && b.y >= 109 && b.y + b.h <= 109 + 56);
    }
    TEST_ASSERT_EQUAL_INT(66 + 50 - 9, drivedash::segment(1, 66, 109, 50, 56, 9).x);
}

void test_rpm_arc() {
    TEST_ASSERT_EQUAL_INT(0, drivedash::arcLit(NAN));
    TEST_ASSERT_EQUAL_INT(0, drivedash::arcLit(0.0f));
    TEST_ASSERT_EQUAL_INT(1, drivedash::arcLit(100.0f));   // any rpm lights the first
    TEST_ASSERT_EQUAL_INT(6, drivedash::arcLit(2100.0f));  // 6.0 of 20: exactly six
    TEST_ASSERT_EQUAL_INT(7, drivedash::arcLit(2101.0f));
    TEST_ASSERT_EQUAL_INT(drivedash::ARC_N, drivedash::arcLit(9000.0f));
    TEST_ASSERT_FALSE(drivedash::arcHot(15));
    TEST_ASSERT_TRUE(drivedash::arcHot(16));
    // red over 80 % of FULL: 5600 rpm lights 16, none red; past it the 17th (index 16) is red
    TEST_ASSERT_FALSE(drivedash::arcHot(drivedash::arcLit(5600.0f) - 1));
    TEST_ASSERT_TRUE(drivedash::arcHot(drivedash::arcLit(5601.0f) - 1));
    // the arc sits over the speed window, inside the screen, left to right
    const drivedash::Quad first = drivedash::arcQuad(0), last = drivedash::arcQuad(drivedash::ARC_N - 1);
    const drivedash::Quad ends[2] = {first, last};
    for (const drivedash::Quad& q : ends)
        for (int k = 0; k < 4; ++k) TEST_ASSERT_TRUE(q.x[k] >= 0 && q.x[k] < 320 && q.y[k] >= 20 && q.y[k] < 240);
    TEST_ASSERT_TRUE(first.x[0] < drivedash::ARC_CX && last.x[1] > drivedash::ARC_CX);
    const drivedash::Quad top = drivedash::arcQuad(drivedash::ARC_N / 2);
    TEST_ASSERT_TRUE(top.y[0] < first.y[0]);   // it rises to the middle
}

void test_ladders_and_warmup() {
    TEST_ASSERT_EQUAL_INT(0, drivedash::ladderLit(NAN, 100.0f, 18));
    TEST_ASSERT_EQUAL_INT(9, drivedash::ladderLit(50.0f, 100.0f, 18));
    TEST_ASSERT_EQUAL_INT(18, drivedash::ladderLit(140.0f, 100.0f, 18));
    TEST_ASSERT_EQUAL_INT(0, drivedash::ladderLit(-3.0f, 100.0f, 18));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, drivedash::warmup(NAN));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, drivedash::warmup(20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, drivedash::warmup(62.5f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, drivedash::warmup(95.0f));
}

void test_alerts_hold_until_clear() {
    drivedash::Alerts a;
    a.update(104.0f, 14.0f, 2000.0f, 0);
    TEST_ASSERT_FALSE(a.coolant());
    a.update(106.0f, 14.0f, 2000.0f, 1000);
    TEST_ASSERT_TRUE(a.coolant());
    a.update(102.0f, 14.0f, 2000.0f, 2000);   // between: still red
    TEST_ASSERT_TRUE(a.coolant());
    a.update(NAN, 14.0f, 2000.0f, 3000);      // unknown: no change
    TEST_ASSERT_TRUE(a.coolant());
    a.update(99.0f, 14.0f, 2000.0f, 4000);
    TEST_ASSERT_FALSE(a.coolant());
    // volts: 60 s low while running
    a.update(90.0f, 11.8f, 2000.0f, 10000);
    a.update(90.0f, 11.8f, 2000.0f, 69999);
    TEST_ASSERT_FALSE(a.volts());
    a.update(90.0f, 11.8f, 2000.0f, 70000);
    TEST_ASSERT_TRUE(a.volts());
    a.update(90.0f, 12.1f, 2000.0f, 71000);   // not back yet
    TEST_ASSERT_TRUE(a.volts());
    a.update(90.0f, 12.3f, 2000.0f, 72000);
    TEST_ASSERT_FALSE(a.volts());
    // a dip that recovers starts the minute over; an engine that's off doesn't count
    a.update(90.0f, 11.8f, 2000.0f, 80000);
    a.update(90.0f, 12.1f, 2000.0f, 100000);
    a.update(90.0f, 11.8f, 2000.0f, 120000);
    a.update(90.0f, 11.8f, 2000.0f, 150000);
    TEST_ASSERT_FALSE(a.volts());
    drivedash::Alerts off;
    off.update(30.0f, 11.5f, 0.0f, 0);
    off.update(30.0f, 11.5f, 0.0f, 120000);
    TEST_ASSERT_FALSE(off.volts());
}

void test_rpm_text() {
    char t[8];
    drivedash::rpmText(2450.0f, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("2.5", t);
    drivedash::rpmText(800.0f, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("0.8", t);
    drivedash::rpmText(NAN, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("--", t);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_screens_by_name_or_number);
    RUN_TEST(test_switching_is_for_a_parked_car);
    RUN_TEST(test_seven_segments);
    RUN_TEST(test_rpm_arc);
    RUN_TEST(test_ladders_and_warmup);
    RUN_TEST(test_alerts_hold_until_clear);
    RUN_TEST(test_rpm_text);
    return UNITY_END();
}
