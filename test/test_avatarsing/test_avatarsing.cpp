#include <unity.h>
#include "../../src/util/avatarsing.h"

using avatarsing::frame;
using avatarsing::singing;
void setUp() {}
void tearDown() {}

void test_sings_the_first_three_quarters_to_the_next_line() {
    TEST_ASSERT_FALSE(singing(9999, 10000, 11000));     // line not started yet
    TEST_ASSERT_TRUE(singing(10000, 10000, 11000));
    TEST_ASSERT_TRUE(singing(10749, 10000, 11000));
    TEST_ASSERT_FALSE(singing(10750, 10000, 11000));    // the last quarter: mouth rests
}
void test_a_line_with_no_next_sings_four_seconds() {
    TEST_ASSERT_TRUE(singing(13999, 10000, 0));
    TEST_ASSERT_FALSE(singing(14000, 10000, 0));
}
void test_mouth_flaps_every_150_ms() {
    TEST_ASSERT_EQUAL_INT(1, frame(10000, 10000, 12000, 2));
    TEST_ASSERT_EQUAL_INT(1, frame(10149, 10000, 12000, 2));
    TEST_ASSERT_EQUAL_INT(0, frame(10150, 10000, 12000, 2));
    TEST_ASSERT_EQUAL_INT(1, frame(10300, 10000, 12000, 2));
}
void test_rests_when_not_singing_or_a_still_mood() {
    TEST_ASSERT_EQUAL_INT(0, frame(11600, 10000, 12000, 2));   // last quarter
    TEST_ASSERT_EQUAL_INT(0, frame(9000, 10000, 12000, 2));    // before the line
    TEST_ASSERT_EQUAL_INT(0, frame(10000, 10000, 12000, 1));   // one frame: nothing to flap
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sings_the_first_three_quarters_to_the_next_line);
    RUN_TEST(test_a_line_with_no_next_sings_four_seconds);
    RUN_TEST(test_mouth_flaps_every_150_ms);
    RUN_TEST(test_rests_when_not_singing_or_a_still_mood);
    return UNITY_END();
}
