#include <unity.h>
#include "../../src/util/car/camtone.h"

void setUp() {}
void tearDown() {}

using cameras::Stage;
using camlist::Kind;

static int count(const camtone::Note* n, int c, uint16_t hz) {
    int k = 0;
    for (int i = 0; i < c; i++) if (n[i].hz == hz) k++;
    return k;
}

void test_speed_alert_two_mid() {
    camtone::Note n[camtone::MAX_NOTES];
    int c = camtone::onChange(Stage::Off, Stage::Alert, Kind::Speed, n);
    TEST_ASSERT_EQUAL_INT(3, c);  // beep, rest, beep
    TEST_ASSERT_EQUAL_INT(2, count(n, c, 1000));
    TEST_ASSERT_EQUAL_UINT16(120, n[0].ms);
    TEST_ASSERT_EQUAL_UINT16(0, n[1].hz);
    TEST_ASSERT_EQUAL_UINT16(80, n[1].ms);
}

void test_alert_from_nofix_plays() {
    camtone::Note n[camtone::MAX_NOTES];
    TEST_ASSERT_EQUAL_INT(3, camtone::onChange(Stage::NoFix, Stage::Alert, Kind::Speed, n));
}

void test_bump_alert_two_low() {
    camtone::Note n[camtone::MAX_NOTES];
    int c = camtone::onChange(Stage::Off, Stage::Alert, Kind::Bump, n);
    TEST_ASSERT_EQUAL_INT(3, c);
    TEST_ASSERT_EQUAL_INT(2, count(n, c, 600));
}

void test_redlight_one_long_high() {
    camtone::Note n[camtone::MAX_NOTES];
    int c = camtone::onChange(Stage::Off, Stage::Alert, Kind::RedLight, n);
    TEST_ASSERT_EQUAL_INT(1, c);
    TEST_ASSERT_EQUAL_UINT16(1600, n[0].hz);
    TEST_ASSERT_EQUAL_UINT16(500, n[0].ms);
}

void test_alert_to_alarm_triple() {
    camtone::Note n[camtone::MAX_NOTES];
    int c = camtone::onChange(Stage::Alert, Stage::Alarm, Kind::Speed, n);
    TEST_ASSERT_EQUAL_INT(5, c);
    TEST_ASSERT_EQUAL_INT(3, count(n, c, 1600));
    TEST_ASSERT_EQUAL_UINT16(80, n[0].ms);
    TEST_ASSERT_EQUAL_UINT16(0, n[1].hz);
    TEST_ASSERT_EQUAL_UINT16(60, n[1].ms);
}

void test_straight_to_alarm_triple() {
    camtone::Note n[camtone::MAX_NOTES];
    TEST_ASSERT_EQUAL_INT(5, camtone::onChange(Stage::Off, Stage::Alarm, Kind::Bump, n));
}

void test_alarm_to_alert_silent() {
    camtone::Note n[camtone::MAX_NOTES];
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Alarm, Stage::Alert, Kind::Speed, n));
}

void test_alert_to_off_silent() {
    camtone::Note n[camtone::MAX_NOTES];
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Alert, Stage::Off, Kind::Speed, n));
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Alarm, Stage::NoFix, Kind::Speed, n));
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Off, Stage::NoFix, Kind::Speed, n));
}

void test_same_stage_silent() {
    camtone::Note n[camtone::MAX_NOTES];
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Alert, Stage::Alert, Kind::Speed, n));
    TEST_ASSERT_EQUAL_INT(0, camtone::onChange(Stage::Alarm, Stage::Alarm, Kind::Speed, n));
}

void test_alarm_repeat_is_the_triple() {
    camtone::Note n[camtone::MAX_NOTES];
    int c = camtone::alarmRepeat(n);
    TEST_ASSERT_EQUAL_INT(5, c);
    TEST_ASSERT_EQUAL_INT(3, count(n, c, 1600));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_speed_alert_two_mid);
    RUN_TEST(test_alert_from_nofix_plays);
    RUN_TEST(test_bump_alert_two_low);
    RUN_TEST(test_redlight_one_long_high);
    RUN_TEST(test_alert_to_alarm_triple);
    RUN_TEST(test_straight_to_alarm_triple);
    RUN_TEST(test_alarm_to_alert_silent);
    RUN_TEST(test_alert_to_off_silent);
    RUN_TEST(test_same_stage_silent);
    RUN_TEST(test_alarm_repeat_is_the_triple);
    return UNITY_END();
}
