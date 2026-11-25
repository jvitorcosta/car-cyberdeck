#include <unity.h>
#include <cmath>
#include "../../src/util/car/sprint.h"

void setUp() {}
void tearDown() {}

// A steady pull: v km/h at t ms, sampled every stepMs from a standstill at t0.
static void pull(sprint::Timer& s, uint32_t t0, float kmhPerS, uint32_t stepMs, uint32_t untilMs) {
    for (uint32_t t = t0; t <= untilMs; t += stepMs) s.update(kmhPerS * (float)(t - t0) / 1000.0f, t);
}

void test_nothing_before_a_run() {
    sprint::Timer s;
    TEST_ASSERT_FALSE(s.running());
    TEST_ASSERT_EQUAL_UINT32(0, s.last());
    TEST_ASSERT_EQUAL_UINT32(0, s.best());
    TEST_ASSERT_EQUAL_UINT32(0, s.elapsed(5000));
}

void test_times_a_pull_from_a_stop_to_100() {
    sprint::Timer s;
    s.update(0.0f, 0);
    s.update(0.0f, 300);
    // 10 km/h a second from t=300: 2 km/h at 500 ms, 100 km/h at 10 300 ms
    pull(s, 300, 10.0f, 300, 12000);
    TEST_ASSERT_FALSE(s.running());
    // from crossing 2 km/h (500) to crossing 100 (10 300): interpolated between the samples
    TEST_ASSERT_UINT32_WITHIN(5, 9800, s.last());
    TEST_ASSERT_EQUAL_UINT32(s.last(), s.best());
}

void test_counts_up_while_running() {
    sprint::Timer s;
    s.update(0.0f, 0);
    s.update(20.0f, 1000);   // crossed 2 km/h at 100 ms
    TEST_ASSERT_TRUE(s.running());
    TEST_ASSERT_UINT32_WITHIN(1, 1900, s.elapsed(2000));
    TEST_ASSERT_EQUAL_UINT32(0, s.last());
}

void test_best_keeps_the_quickest() {
    sprint::Timer s;
    s.update(0.0f, 0);
    pull(s, 0, 10.0f, 250, 11000);    // ~9.8 s
    const uint32_t slow = s.last();
    s.update(0.0f, 20000);            // stopped again: armed
    pull(s, 20000, 12.5f, 250, 30000);   // ~7.84 s
    TEST_ASSERT_TRUE(s.last() < slow);
    TEST_ASSERT_EQUAL_UINT32(s.last(), s.best());
    s.update(0.0f, 40000);
    pull(s, 40000, 8.0f, 250, 54000);    // slower: best stays
    TEST_ASSERT_TRUE(s.last() > s.best());
}

void test_needs_a_stop_before_a_run() {
    sprint::Timer r;
    for (uint32_t t = 0; t < 12000; t += 250) r.update(30.0f + (float)t / 100.0f, t);   // never stopped
    TEST_ASSERT_FALSE(r.running());
    TEST_ASSERT_EQUAL_UINT32(0, r.last());
}

void test_a_stop_or_a_gap_or_too_long_cancels() {
    sprint::Timer s;
    s.update(0.0f, 0);
    s.update(30.0f, 1000);
    s.update(0.5f, 2000);   // back to a stop: cancelled, armed again
    TEST_ASSERT_FALSE(s.running());
    s.update(20.0f, 3000);
    TEST_ASSERT_TRUE(s.running());
    s.update(40.0f, 3000 + sprint::Timer::GAP_MS + 1);   // a Bluetooth stall: cancelled
    TEST_ASSERT_FALSE(s.running());
    s.update(NAN, 9000);
    TEST_ASSERT_FALSE(s.running());
    sprint::Timer slow;
    slow.update(0.0f, 0);
    for (uint32_t t = 500; t <= 40000; t += 500) slow.update(2.5f + (float)t / 1000.0f, t);   // 1 km/h a second
    TEST_ASSERT_FALSE(slow.running());   // gave up at MAX_MS
    TEST_ASSERT_EQUAL_UINT32(0, slow.last());
}

void test_a_jump_to_100_in_one_sample_is_no_run() {
    sprint::Timer s;
    s.update(0.0f, 0);
    s.update(105.0f, 300);   // a glitch or a short dropout: not a 0-100
    s.update(106.0f, 600);
    TEST_ASSERT_FALSE(s.running());
    TEST_ASSERT_EQUAL_UINT32(0, s.last());
    TEST_ASSERT_EQUAL_UINT32(0, s.best());
    s.update(0.0f, 900);   // a real stop arms it again
    s.update(10.0f, 1900);
    TEST_ASSERT_TRUE(s.running());
}

void test_unknown_speed_mid_run_cancels() {
    sprint::Timer s;
    s.update(0.0f, 0);
    s.update(20.0f, 1000);
    TEST_ASSERT_TRUE(s.running());
    s.update(NAN, 1500);
    TEST_ASSERT_FALSE(s.running());
    s.update(100.0f, 2000);
    TEST_ASSERT_EQUAL_UINT32(0, s.last());
}

void test_text_like_a_lap_time() {
    char out[12];
    sprint::text(8920, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("08 : 92", out);
    sprint::text(0, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("-- : --", out);
    sprint::text(12345, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("12 : 34", out);
    sprint::text(123456, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("99 : 99", out);
    sprint::text(8920, out, 4);
    TEST_ASSERT_EQUAL_STRING("", out);   // doesn't fit
}

// millis() wraps every 49.7 days: a pull that crosses the wrap times the same as one that doesn't.
void test_a_pull_across_the_millis_wrap_times_the_same() {
    sprint::Timer s;
    const uint32_t base = 0xFFFFFFFFu - 4000u;
    s.update(0.0f, base);
    s.update(0.0f, base + 300u);
    for (uint32_t i = 0; i <= 40; i++) {   // 10 km/h a second from base + 300, a sample every 300 ms
        const uint32_t t = base + 300u + i * 300u;
        s.update(10.0f * (float)(i * 300u) / 1000.0f, t);
    }
    TEST_ASSERT_FALSE(s.running());
    TEST_ASSERT_UINT32_WITHIN(5, 9800, s.last());
    TEST_ASSERT_EQUAL_UINT32(s.last(), s.best());
}

void test_a_gap_and_the_run_limit_hold_across_the_wrap() {
    sprint::Timer s;
    const uint32_t base = 0xFFFFFFFFu - 1000u;
    s.update(0.0f, base);
    s.update(10.0f, base + 500u);
    TEST_ASSERT_TRUE(s.running());
    TEST_ASSERT_UINT32_WITHIN(400, 3000, s.elapsed(base + 3500u));   // elapsed() itself is wrap-safe
    s.update(10.0f, base + 500u + sprint::Timer::GAP_MS + 1u);       // a gap past GAP_MS, over the wrap
    TEST_ASSERT_FALSE(s.running());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_before_a_run);
    RUN_TEST(test_times_a_pull_from_a_stop_to_100);
    RUN_TEST(test_counts_up_while_running);
    RUN_TEST(test_best_keeps_the_quickest);
    RUN_TEST(test_needs_a_stop_before_a_run);
    RUN_TEST(test_a_stop_or_a_gap_or_too_long_cancels);
    RUN_TEST(test_a_jump_to_100_in_one_sample_is_no_run);
    RUN_TEST(test_unknown_speed_mid_run_cancels);
    RUN_TEST(test_text_like_a_lap_time);
    RUN_TEST(test_a_pull_across_the_millis_wrap_times_the_same);
    RUN_TEST(test_a_gap_and_the_run_limit_hold_across_the_wrap);
    return UNITY_END();
}
