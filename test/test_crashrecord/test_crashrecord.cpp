#include <unity.h>
#include <cstring>
#include "../../src/util/common/crashrecord.h"

void setUp() {}
void tearDown() {}

void test_push_puts_the_newest_first_and_drops_the_oldest() {
    crashrecord::Record ring[crashrecord::HISTORY] = {};
    for (uint32_t i = 1; i <= crashrecord::HISTORY + 2; i++) {
        crashrecord::Record r{};
        r.reason = 4;
        r.upS = i;
        crashrecord::push(ring, r);
    }
    TEST_ASSERT_EQUAL_UINT32(crashrecord::HISTORY + 2, ring[0].upS);
    TEST_ASSERT_EQUAL_UINT32(3, ring[crashrecord::HISTORY - 1].upS);
}

void test_mark_cuts_and_terminates() {
    char at[crashrecord::AT_LEN];
    crashrecord::mark(at, "a very long phase name indeed");
    TEST_ASSERT_EQUAL_size_t(crashrecord::AT_LEN - 1, strlen(at));
    crashrecord::mark(at, nullptr);
    TEST_ASSERT_EQUAL_STRING("", at);
}

void test_only_panics_and_watchdogs_are_crashes() {
    TEST_ASSERT_TRUE(crashrecord::crashed(4));
    TEST_ASSERT_TRUE(crashrecord::crashed(6));
    TEST_ASSERT_FALSE(crashrecord::crashed(1));  // power-on
    TEST_ASSERT_FALSE(crashrecord::crashed(9));  // brownout: the power, not the firmware
    TEST_ASSERT_EQUAL_STRING("brownout", crashrecord::reasonName(9));
}

void test_format_names_the_reason_and_the_phase() {
    crashrecord::Record r{};
    r.reason = 6;
    r.upS = 42;
    r.heap = 9000;
    r.fails = 3;
    crashrecord::mark(r.at, "lyrics get");
    char line[160];
    crashrecord::format(r, line, sizeof(line));
    TEST_ASSERT_NOT_NULL(strstr(line, "task watchdog after 42s"));
    TEST_ASSERT_NOT_NULL(strstr(line, "3 failed allocs"));
    TEST_ASSERT_NOT_NULL(strstr(line, "at lyrics get"));
    char tiny[8];
    crashrecord::format(r, tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_size_t(7, strlen(tiny));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_push_puts_the_newest_first_and_drops_the_oldest);
    RUN_TEST(test_mark_cuts_and_terminates);
    RUN_TEST(test_only_panics_and_watchdogs_are_crashes);
    RUN_TEST(test_format_names_the_reason_and_the_phase);
    return UNITY_END();
}
