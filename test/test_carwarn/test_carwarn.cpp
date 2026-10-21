#include <unity.h>
#include "../../src/util/carwarn.h"

using carwarn::Warn;
void setUp() {}
void tearDown() {}

// She says a warning once it has held for HOLD_MS, then not again while it lasts.
void test_says_once_after_the_hold() {
    carwarn::Watch w;
    TEST_ASSERT_TRUE(w.update(true, false, 0) == Warn::None);
    TEST_ASSERT_TRUE(w.update(true, false, carwarn::HOLD_MS - 1) == Warn::None);
    TEST_ASSERT_TRUE(w.update(true, false, carwarn::HOLD_MS) == Warn::CoolantHot);
    TEST_ASSERT_TRUE(w.update(true, false, carwarn::HOLD_MS + 60000) == Warn::None);
}

// The crank dip: the battery under its limit for a few seconds is never said.
void test_crank_dip_is_not_said() {
    carwarn::Watch w;
    for (uint32_t t = 0; t < 4000; t += 100) TEST_ASSERT_TRUE(w.update(false, true, t) == Warn::None);
    for (uint32_t t = 4000; t < 60000; t += 100) TEST_ASSERT_TRUE(w.update(false, false, t) == Warn::None);
}

// Said again only after it has been clear for REARM_MS; a short clear doesn't rearm it.
void test_rearms_after_a_long_clear() {
    carwarn::Watch w;
    w.update(false, true, 0);
    TEST_ASSERT_TRUE(w.update(false, true, carwarn::HOLD_MS) == Warn::BatteryLow);
    w.update(false, false, 20000);
    w.update(false, true, 30000);                                   // back before the rearm
    TEST_ASSERT_TRUE(w.update(false, true, 30000 + carwarn::HOLD_MS) == Warn::None);
    w.update(false, false, 50000);
    w.update(false, false, 50000 + carwarn::REARM_MS);              // clear long enough
    w.update(false, true, 200000);
    TEST_ASSERT_TRUE(w.update(false, true, 200000 + carwarn::HOLD_MS) == Warn::BatteryLow);
}

// Both at once: coolant first, the battery on the next call.
void test_one_warning_per_call() {
    carwarn::Watch w;
    w.update(true, true, 0);
    TEST_ASSERT_TRUE(w.update(true, true, carwarn::HOLD_MS) == Warn::CoolantHot);
    TEST_ASSERT_TRUE(w.update(true, true, carwarn::HOLD_MS + 20) == Warn::BatteryLow);
    TEST_ASSERT_TRUE(w.update(true, true, carwarn::HOLD_MS + 40) == Warn::None);
}

void test_hold_across_millis_wrap() {
    carwarn::Watch w;
    w.update(true, false, 0xFFFFF000u);
    TEST_ASSERT_TRUE(w.update(true, false, 0xFFFFF000u + carwarn::HOLD_MS) == Warn::CoolantHot);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_says_once_after_the_hold);
    RUN_TEST(test_crank_dip_is_not_said);
    RUN_TEST(test_rearms_after_a_long_clear);
    RUN_TEST(test_one_warning_per_call);
    RUN_TEST(test_hold_across_millis_wrap);
    return UNITY_END();
}
