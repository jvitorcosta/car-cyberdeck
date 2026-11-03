#include <unity.h>
#include <cstring>
#include "../../src/util/car/camcue.h"

void setUp() {}
void tearDown() {}

using cameras::Stage;
using camlist::Kind;
using camtone::Note;

static Note n[camtone::MAX_NOTES];

void test_alert_chimes_once() {
    camcue::Cue q;
    TEST_ASSERT_EQUAL_INT(3, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", false, 1000, n));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(1000));   // nothing to repeat
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alert, false, 5000, n));
}

void test_same_point_flicker_within_3s_is_quiet() {
    camcue::Cue q;
    TEST_ASSERT_GREATER_THAN(0, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", false, 1000, n));
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alert, Stage::Off, Kind::Speed, "", false, 2000, n));
    // Back over 10 km/h a second later, same camera: no second chime.
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", false, 3000, n));
    // ...and the guard runs from the last chime, not from the flicker.
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alert, Stage::Off, Kind::Speed, "", false, 3500, n));
    TEST_ASSERT_GREATER_THAN(0, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", false,
                                           1000 + camcue::CHIME_GUARD_MS, n));
}

void test_another_point_chimes_at_once() {
    camcue::Cue q;
    TEST_ASSERT_GREATER_THAN(0, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", false, 1000, n));
    q.onChange(Stage::Alert, Stage::Off, Kind::Speed, "", false, 1500, n);
    TEST_ASSERT_GREATER_THAN(0, q.onChange(Stage::Off, Stage::Alert, Kind::Bump, "mao-2", false, 2000, n));
}

void test_alarm_plays_then_repeats_every_1500ms() {
    camcue::Cue q;
    TEST_ASSERT_EQUAL_INT(5, q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n));
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(1000));
    TEST_ASSERT_EQUAL_UINT32(100, q.waitMs(2400));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alarm, false, 2400, n));    // not yet
    TEST_ASSERT_EQUAL_INT(5, q.tick(Stage::Alarm, false, 2500, n));
    TEST_ASSERT_EQUAL_UINT16(1600, n[0].hz);
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(2500));
    TEST_ASSERT_EQUAL_INT(5, q.tick(Stage::Alarm, false, 4100, n));    // a late tick: next from now
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(4100));
}

void test_alarm_stops_when_stage_leaves() {
    camcue::Cue q;
    q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n);
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alarm, Stage::Alert, Kind::Speed, "mao-1", false, 1800, n));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(1800));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alert, false, 2500, n));
}

void test_tick_trusts_the_real_stage() {
    // A lost "-> off" (full queue): the repeat checks the live stage and stops.
    camcue::Cue q;
    q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n);
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Off, false, 2500, n));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(2500));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alarm, false, 4000, n));    // it took a change to start
}

void test_alarm_flicker_keeps_the_cadence() {
    camcue::Cue q;
    q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n);
    q.onChange(Stage::Alarm, Stage::Alert, Kind::Speed, "mao-1", false, 1400, n);
    // Back over the limit 600 ms after the triple beep: it waits for its 1.5 s slot.
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1600, n));
    TEST_ASSERT_EQUAL_UINT32(900, q.waitMs(1600));
    TEST_ASSERT_EQUAL_INT(5, q.tick(Stage::Alarm, false, 2500, n));
}

void test_mute_silences_but_band_logic_continues() {
    camcue::Cue q;
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", true, 1000, n));
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", true, 2000, n));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alarm, true, 3500, n));
    // Unmuted mid-alarm: the next slot beeps.
    TEST_ASSERT_EQUAL_INT(5, q.tick(Stage::Alarm, false, 5000, n));
}

void test_mute_mid_alarm_stops_repeat() {
    camcue::Cue q;
    TEST_ASSERT_EQUAL_INT(5, q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alarm, true, 2500, n));
    TEST_ASSERT_EQUAL_INT(0, q.tick(Stage::Alarm, true, 4000, n));
}

void test_alarm_easing_to_alert_no_chime() {
    camcue::Cue q;
    q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "mao-1", false, 1000, n);
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Alarm, Stage::Alert, Kind::Speed, "mao-1", false, 5000, n));
}

void test_millis_wrap() {
    camcue::Cue q;
    const uint32_t t = 0xFFFFFF00u;
    TEST_ASSERT_EQUAL_INT(5, q.onChange(Stage::Alert, Stage::Alarm, Kind::Speed, "x", false, t, n));
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(t));
    TEST_ASSERT_EQUAL_INT(5, q.tick(Stage::Alarm, false, t + 1500, n));
}

void test_null_id_counts_as_empty() {
    camcue::Cue q;
    TEST_ASSERT_GREATER_THAN(0, q.onChange(Stage::Off, Stage::Alert, Kind::RedLight, nullptr, false, 1000, n));
    TEST_ASSERT_EQUAL_INT(0, q.onChange(Stage::Off, Stage::Alert, Kind::RedLight, "", false, 2000, n));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_alert_chimes_once);
    RUN_TEST(test_same_point_flicker_within_3s_is_quiet);
    RUN_TEST(test_another_point_chimes_at_once);
    RUN_TEST(test_alarm_plays_then_repeats_every_1500ms);
    RUN_TEST(test_alarm_stops_when_stage_leaves);
    RUN_TEST(test_tick_trusts_the_real_stage);
    RUN_TEST(test_alarm_flicker_keeps_the_cadence);
    RUN_TEST(test_mute_silences_but_band_logic_continues);
    RUN_TEST(test_mute_mid_alarm_stops_repeat);
    RUN_TEST(test_alarm_easing_to_alert_no_chime);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_null_id_counts_as_empty);
    return UNITY_END();
}
