#include <unity.h>
#include <cmath>
#include "../../src/util/obdlearn.h"

using E = obdlearn::Engine;

void setUp() {}
void tearDown() {}

void test_range_learns_and_clamps() {
    obdlearn::Range r(16, 80);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.scale(16));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, r.scale(80));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, r.scale(48));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.scale(14));        // learns a lower released position
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, r.scale(86));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, r.scale(50));         // (50-14)/(86-14)
}

void test_dip_caught_when_waiting_at_key_on() {
    obdlearn::StartDip d;
    d.feed(12.5f, E::Off);
    d.feed(12.4f, E::Off);
    d.feed(10.1f, E::Off);
    d.feed(11.6f, E::Off);
    TEST_ASSERT_TRUE(std::isnan(d.value()));                   // not started yet
    d.feed(14.0f, E::Running);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
    d.feed(9.0f, E::Off);                                       // a stall and restart: first start kept
    d.feed(14.1f, E::Running);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
}

void test_dip_missed_when_already_running() {
    obdlearn::StartDip d;
    d.feed(14.1f, E::Running);
    TEST_ASSERT_TRUE(std::isnan(d.value()));
    d.feed(13.9f, E::Running);
    TEST_ASSERT_TRUE(std::isnan(d.value()));
}

// Final review: the link came up after the crank, so the first battery readings arrive before
// any RPM reply. They must not count as "engine off": the dip was missed, not 14.1 V.
void test_readings_before_the_engine_state_is_known_dont_count() {
    obdlearn::StartDip d;
    d.feed(14.1f, E::Unknown);
    d.feed(14.0f, E::Unknown);
    d.feed(14.1f, E::Running);
    TEST_ASSERT_TRUE(std::isnan(d.value()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_range_learns_and_clamps);
    RUN_TEST(test_dip_caught_when_waiting_at_key_on);
    RUN_TEST(test_dip_missed_when_already_running);
    RUN_TEST(test_readings_before_the_engine_state_is_known_dont_count);
    return UNITY_END();
}
