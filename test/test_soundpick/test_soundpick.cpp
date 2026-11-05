#include <unity.h>
#include "../../src/util/black/soundpick.h"

using namespace soundpick;

void setUp() {}
void tearDown() {}

void test_never_repeats_last_and_reaches_all() {
    bool seen[5] = {};
    for (int last = -1; last < 5; ++last)
        for (uint32_t r = 0; r < 64; ++r) {
            const int i = pick(r * 2654435761u, last, 5);
            TEST_ASSERT_TRUE(i >= 0 && i < 5);
            TEST_ASSERT_NOT_EQUAL(last, i);
            seen[i] = true;
        }
    for (bool s : seen) TEST_ASSERT_TRUE(s);
}

void test_single_clip_repeats() {
    for (uint32_t r = 0; r < 8; ++r) TEST_ASSERT_EQUAL_INT(0, pick(r, 0, 1));
}

// NVS may hold an index from an older, longer roster.
void test_bad_last_is_ignored() {
    for (uint32_t r = 0; r < 32; ++r) {
        const int a = pick(r, 99, 3), b = pick(r, -7, 3);
        TEST_ASSERT_TRUE(a >= 0 && a < 3);
        TEST_ASSERT_TRUE(b >= 0 && b < 3);
        const int c = pick(r * 977u, -1, 500);                  // clamped to MAX
        TEST_ASSERT_TRUE(c >= 0 && c < MAX);
    }
    TEST_ASSERT_EQUAL_INT(-1, pick(5, -1, 0));
    TEST_ASSERT_EQUAL_INT(-1, pick(5, -1, -3));
}

void test_ok_filter() {
    const bool ok[4] = {false, true, false, true};
    for (uint32_t r = 0; r < 32; ++r) {
        const int i = pick(r, -1, 4, ok);
        TEST_ASSERT_TRUE(i == 1 || i == 3);
        TEST_ASSERT_EQUAL_INT(3, pick(r, 1, 4, ok));
    }
}

void test_none_fit_gives_minus_one() {
    const bool ok[3] = {false, false, false};
    for (uint32_t r = 0; r < 8; ++r) TEST_ASSERT_EQUAL_INT(-1, pick(r, -1, 3, ok));
}

void test_forced() {
    TEST_ASSERT_EQUAL_INT(0, forced("drive:3,opener:0", "opener", 5));
    TEST_ASSERT_EQUAL_INT(3, forced("drive:3,opener:0", "drive", 5));
    TEST_ASSERT_EQUAL_INT(-1, forced("drive:3", "finale", 5));
    TEST_ASSERT_EQUAL_INT(-1, forced("drive:9", "drive", 5));
    TEST_ASSERT_EQUAL_INT(-1, forced(nullptr, "drive", 5));
    TEST_ASSERT_EQUAL_INT(-1, forced("drive:", "drive", 5));
    TEST_ASSERT_EQUAL_INT(11, forced("finale:11", "finale", 12));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_repeats_last_and_reaches_all);
    RUN_TEST(test_single_clip_repeats);
    RUN_TEST(test_bad_last_is_ignored);
    RUN_TEST(test_ok_filter);
    RUN_TEST(test_none_fit_gives_minus_one);
    RUN_TEST(test_forced);
    return UNITY_END();
}
