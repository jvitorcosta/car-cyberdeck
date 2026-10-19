#include <unity.h>
#include <cmath>
#include "../../src/util/obdlearn.h"

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
    d.feed(12.5f, false);
    d.feed(12.4f, false);
    d.feed(10.1f, false);
    d.feed(11.6f, false);
    TEST_ASSERT_TRUE(std::isnan(d.value()));                   // not started yet
    d.feed(14.0f, true);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
    d.feed(9.0f, false);                                       // a stall and restart: first start kept
    d.feed(14.1f, true);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
}

void test_dip_missed_when_already_running() {
    obdlearn::StartDip d;
    d.feed(14.1f, true);
    TEST_ASSERT_TRUE(std::isnan(d.value()));
    d.feed(13.9f, true);
    TEST_ASSERT_TRUE(std::isnan(d.value()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_range_learns_and_clamps);
    RUN_TEST(test_dip_caught_when_waiting_at_key_on);
    RUN_TEST(test_dip_missed_when_already_running);
    return UNITY_END();
}
