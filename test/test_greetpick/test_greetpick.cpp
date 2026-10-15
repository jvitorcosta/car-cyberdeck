#include <unity.h>
#include "../../src/util/greetpick.h"

void setUp() {}
void tearDown() {}

// The S3 boots with one of its greetings at random (the original or McQueen's line).
void test_random_bit_picks_the_clip() {
    TEST_ASSERT_EQUAL_INT(0, greetpick::pick(0u, true));
    TEST_ASSERT_EQUAL_INT(1, greetpick::pick(1u, true));
    TEST_ASSERT_EQUAL_INT(0, greetpick::pick(0xFFFFFFFEu, true));
    TEST_ASSERT_EQUAL_INT(1, greetpick::pick(0xFFFFFFFFu, true));
}
void test_without_an_alternative_always_the_original() {
    TEST_ASSERT_EQUAL_INT(0, greetpick::pick(1u, false));
    TEST_ASSERT_EQUAL_INT(0, greetpick::pick(0xFFFFFFFFu, false));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_random_bit_picks_the_clip);
    RUN_TEST(test_without_an_alternative_always_the_original);
    return UNITY_END();
}
