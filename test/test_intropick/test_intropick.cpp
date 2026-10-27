#include <unity.h>
#include "../../src/util/black/intropick.h"

using namespace intropick;

void setUp() {}
void tearDown() {}

void test_never_repeats_last_and_reaches_all() {
    bool seen[COUNT] = {};
    for (int last = 0; last < COUNT; ++last)
        for (uint32_t r = 0; r < 64; ++r) {
            const Opening o = pick(r * 2654435761u, last, true, true, true);
            TEST_ASSERT_NOT_EQUAL(last, (int)o);
            seen[o] = true;
        }
    for (bool s : seen) TEST_ASSERT_TRUE(s);
}

void test_skips_missing_files() {
    for (uint32_t r = 0; r < 64; ++r) {
        TEST_ASSERT_NOT_EQUAL(KISS, pick(r, MCQUEEN, false, true, true));
        TEST_ASSERT_NOT_EQUAL(MCQUEEN, pick(r, KISS, true, false, true));
        TEST_ASSERT_NOT_EQUAL(MAX, pick(r, KISS, true, true, false));
    }
}

// Review focus 1: a fresh clone has neither file.
void test_only_honda_without_files() {
    for (int last = -1; last < COUNT; ++last)
        for (uint32_t r = 0; r < 16; ++r) TEST_ASSERT_EQUAL(HONDA, pick(r, last, false, false, false));
}

// Review focus 2: NVS may hold anything.
void test_bad_last_is_ignored() {
    for (uint32_t r = 0; r < 32; ++r) {
        const Opening o = pick(r, 5, true, true, true);
        TEST_ASSERT_TRUE(o < COUNT);
        const int k = pick(r, -7, true, false, false);
        TEST_ASSERT_TRUE(k == KISS || k == HONDA);
    }
}

void test_greeting_per_opening() {
    TEST_ASSERT_EQUAL(NONE, greetingFor(KISS));
    TEST_ASSERT_EQUAL(MAIN, greetingFor(HONDA));
    TEST_ASSERT_EQUAL(ALT, greetingFor(MCQUEEN));
    TEST_ASSERT_EQUAL(NONE, greetingFor(MAX));
}

void test_videos() {
    TEST_ASSERT_TRUE(isVideo(KISS));
    TEST_ASSERT_TRUE(isVideo(MAX));
    TEST_ASSERT_FALSE(isVideo(HONDA));
    TEST_ASSERT_FALSE(isVideo(MCQUEEN));
}

// Max alone with the Honda: they alternate.
void test_max_and_honda_alternate() {
    for (uint32_t r = 0; r < 16; ++r) {
        TEST_ASSERT_EQUAL(MAX, pick(r, HONDA, false, false, true));
        TEST_ASSERT_EQUAL(HONDA, pick(r, MAX, false, false, true));
    }
}

void test_names_round_trip() {
    for (int o = 0; o < COUNT; ++o) TEST_ASSERT_EQUAL_INT(o, parse(name((Opening)o)));
    TEST_ASSERT_EQUAL_INT(-1, parse("sonic"));
    TEST_ASSERT_EQUAL_INT(-1, parse(""));
    TEST_ASSERT_EQUAL_INT(-1, parse(nullptr));
}

// Review focus 3: forcing an opening the build lacks falls back (-1).
void test_forced_needs_its_files() {
    TEST_ASSERT_EQUAL_INT(KISS, forced(KISS, true, false, false));
    TEST_ASSERT_EQUAL_INT(-1, forced(KISS, false, true, true));
    TEST_ASSERT_EQUAL_INT(-1, forced(MCQUEEN, true, false, true));
    TEST_ASSERT_EQUAL_INT(MAX, forced(MAX, false, false, true));
    TEST_ASSERT_EQUAL_INT(-1, forced(MAX, true, true, false));
    TEST_ASSERT_EQUAL_INT(HONDA, forced(HONDA, false, false, false));
    TEST_ASSERT_EQUAL_INT(-1, forced(-1, true, true, true));
    TEST_ASSERT_EQUAL_INT(-1, forced(9, true, true, true));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_repeats_last_and_reaches_all);
    RUN_TEST(test_skips_missing_files);
    RUN_TEST(test_only_honda_without_files);
    RUN_TEST(test_bad_last_is_ignored);
    RUN_TEST(test_greeting_per_opening);
    RUN_TEST(test_videos);
    RUN_TEST(test_max_and_honda_alternate);
    RUN_TEST(test_names_round_trip);
    RUN_TEST(test_forced_needs_its_files);
    return UNITY_END();
}
