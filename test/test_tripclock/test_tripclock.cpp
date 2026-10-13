#include <unity.h>
#include <cstdint>
#include "../../src/util/tripclock.h"

void setUp() {}
void tearDown() {}

void test_counts_seconds_since_start_and_reset() {
    tripclock::Clock c(1000);
    TEST_ASSERT_EQUAL_UINT32(0, c.seconds(1999));
    TEST_ASSERT_EQUAL_UINT32(37 * 60, c.seconds(1000 + 37 * 60 * 1000));
    c.reset(500000);
    TEST_ASSERT_EQUAL_UINT32(2, c.seconds(502500));
}

void test_survives_millis_wrap() {
    tripclock::Clock c(0xFFFFF000u);                       // 4.096 s before the wrap
    TEST_ASSERT_EQUAL_UINT32(10, c.seconds(0x00001710u));  // 10 s later, past the wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_counts_seconds_since_start_and_reset);
    RUN_TEST(test_survives_millis_wrap);
    return UNITY_END();
}
