#include <unity.h>
#include <cmath>
#include "../../src/util/car/streetface.h"

using obdplan::Link;
using streetface::Mood;

void setUp() {}
void tearDown() {}

void test_mood_follows_the_link_then_the_rpm() {
    TEST_ASSERT_EQUAL_INT((int)Mood::Look, (int)streetface::mood(Link::Off, 3000.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Look, (int)streetface::mood(Link::Connecting, NAN));
    TEST_ASSERT_EQUAL_INT((int)Mood::Sleep, (int)streetface::mood(Link::NoEngine, NAN));
    TEST_ASSERT_EQUAL_INT((int)Mood::Calm, (int)streetface::mood(Link::Live, NAN));
    TEST_ASSERT_EQUAL_INT((int)Mood::Calm, (int)streetface::mood(Link::Live, 800.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Focus, (int)streetface::mood(Link::Live, 2500.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Focus, (int)streetface::mood(Link::Live, 4999.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Angry, (int)streetface::mood(Link::Live, 5000.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Angry, (int)streetface::mood(Link::Live, 9000.0f));
}

void test_blinks_briefly_and_sleeps_shut() {
    int shut = 0;
    for (uint32_t t = 0; t < streetface::BLINK_EVERY_MS; t += 10) shut += streetface::eyesShut(Mood::Calm, t);
    TEST_ASSERT_EQUAL_INT(streetface::BLINK_MS / 10, shut);   // one blink a cycle
    TEST_ASSERT_TRUE(streetface::eyesShut(Mood::Sleep, 2000));
    TEST_ASSERT_FALSE(streetface::eyesShut(Mood::Angry, 2000));
}

void test_glance_sweeps_only_while_looking() {
    int lo = 99, hi = -99;
    for (uint32_t t = 0; t < 2400; t += 20) {
        const int g = streetface::glance(Mood::Look, t);
        lo = g < lo ? g : lo;
        hi = g > hi ? g : hi;
    }
    TEST_ASSERT_EQUAL_INT(-streetface::GLANCE, lo);
    TEST_ASSERT_EQUAL_INT(streetface::GLANCE, hi);
    TEST_ASSERT_EQUAL_INT(0, streetface::glance(Mood::Calm, 600));
}

void test_stripes_run_with_the_speed() {
    streetface::Stripes s;
    TEST_ASSERT_EQUAL_INT(0, s.update(100.0f, 1000));   // the first call only starts the clock
    TEST_ASSERT_EQUAL_INT(12, s.update(100.0f, 1100));  // 120 px/s for 0.1 s
    TEST_ASSERT_EQUAL_INT(12, s.update(0.0f, 1200));    // stopped: stands
    TEST_ASSERT_EQUAL_INT(12, s.update(NAN, 1300));     // unknown: stands
    TEST_ASSERT_EQUAL_INT(0, s.update(100.0f, 1400));   // +12: wraps at 24
}

void test_stripes_dont_leap_after_a_gap() {
    streetface::Stripes s;
    s.update(100.0f, 0);
    // 10 s with the screen off: counted as 250 ms (30 px), so 6 after the wrap
    TEST_ASSERT_EQUAL_INT(6, s.update(100.0f, 10000));
}

void test_trace_fills_from_the_right() {
    streetface::Trace tr;
    for (int i = 0; i < streetface::Trace::N; ++i) TEST_ASSERT_EQUAL_INT(-1, tr.height(i, 10));
    tr.update(3750.0f, 0);
    TEST_ASSERT_EQUAL_INT(5, tr.height(streetface::Trace::N - 1, 10));   // half way from 500 to 7000
    TEST_ASSERT_EQUAL_INT(-1, tr.height(streetface::Trace::N - 2, 10));
    tr.update(7000.0f, 100);   // too soon: ignored
    TEST_ASSERT_EQUAL_INT(5, tr.height(streetface::Trace::N - 1, 10));
    tr.update(7000.0f, 375);
    TEST_ASSERT_EQUAL_INT(10, tr.height(streetface::Trace::N - 1, 10));
    TEST_ASSERT_EQUAL_INT(5, tr.height(streetface::Trace::N - 2, 10));
    tr.update(NAN, 750);
    TEST_ASSERT_EQUAL_INT(-1, tr.height(streetface::Trace::N - 1, 10));   // unknown: a dot
    TEST_ASSERT_EQUAL_INT(-1, tr.height(-1, 10));
    TEST_ASSERT_EQUAL_INT(-1, tr.height(streetface::Trace::N, 10));
}

void test_trace_keeps_the_last_n_and_clamps() {
    streetface::Trace tr;
    uint32_t t = 0;
    for (int i = 0; i < streetface::Trace::N + 5; ++i, t += streetface::Trace::SAMPLE_MS)
        tr.update((float)(i * 100), t);
    // the oldest kept is sample 5
    TEST_ASSERT_EQUAL_INT(0, tr.height(0, 70));   // 500: the idle floor
    TEST_ASSERT_EQUAL_INT((int)std::lround(1000.0f / 6500.0f * 70.0f), tr.height(10, 70));   // sample 15: 1500
    tr.update(12000.0f, t);
    TEST_ASSERT_EQUAL_INT(70, tr.height(streetface::Trace::N - 1, 70));   // past full scale: full
    tr.update(-50.0f, t + streetface::Trace::SAMPLE_MS);
    TEST_ASSERT_EQUAL_INT(0, tr.height(streetface::Trace::N - 1, 70));
}

void test_trace_restarts_its_beat_after_a_gap_and_across_the_wrap() {
    streetface::Trace tr;
    tr.update(1000.0f, 0);
    tr.update(2000.0f, 10000);   // a long gap: sampled at once, the beat restarts at 10000
    tr.update(3000.0f, 10100);   // too soon after the restart: ignored
    TEST_ASSERT_EQUAL_INT((int)std::lround(1500.0f / 6500.0f * 65.0f), tr.height(streetface::Trace::N - 1, 65));
    streetface::Trace w;
    w.update(1000.0f, 0xFFFFFF00u);
    w.update(7000.0f, 0xFFFFFF00u + streetface::Trace::SAMPLE_MS);   // past the millis() wrap
    TEST_ASSERT_EQUAL_INT(65, w.height(streetface::Trace::N - 1, 65));
    w.reset();
    TEST_ASSERT_EQUAL_INT(-1, w.height(streetface::Trace::N - 1, 65));
}

void test_mood_with_odd_rpm_and_blinks_while_looking() {
    TEST_ASSERT_EQUAL_INT((int)Mood::Calm, (int)streetface::mood(Link::Live, -100.0f));
    TEST_ASSERT_EQUAL_INT((int)Mood::Calm, (int)streetface::mood(Link::Live, INFINITY));   // not a reading
    TEST_ASSERT_TRUE(streetface::eyesShut(Mood::Look, 50));
    TEST_ASSERT_FALSE(streetface::eyesShut(Mood::Look, 1000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mood_follows_the_link_then_the_rpm);
    RUN_TEST(test_blinks_briefly_and_sleeps_shut);
    RUN_TEST(test_glance_sweeps_only_while_looking);
    RUN_TEST(test_stripes_run_with_the_speed);
    RUN_TEST(test_stripes_dont_leap_after_a_gap);
    RUN_TEST(test_trace_fills_from_the_right);
    RUN_TEST(test_trace_keeps_the_last_n_and_clamps);
    RUN_TEST(test_trace_restarts_its_beat_after_a_gap_and_across_the_wrap);
    RUN_TEST(test_mood_with_odd_rpm_and_blinks_while_looking);
    return UNITY_END();
}
