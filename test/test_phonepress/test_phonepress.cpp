#include <unity.h>
#include "../../src/util/phonepress.h"

using namespace menunav;
void setUp() {}
void tearDown() {}

void test_names_map_to_buttons() {
    TEST_ASSERT_EQUAL_INT(Lyrics, phonepress::parse("lyrics"));
    TEST_ASSERT_EQUAL_INT(Sounds, phonepress::parse("sounds"));
    TEST_ASSERT_EQUAL_INT(Trip, phonepress::parse("trip"));
    TEST_ASSERT_EQUAL_INT(Back, phonepress::parse("back"));
    TEST_ASSERT_EQUAL_INT(Howl, phonepress::parse("howl"));
    TEST_ASSERT_EQUAL_INT(Vroom, phonepress::parse("vroom"));
    TEST_ASSERT_EQUAL_INT(Reset, phonepress::parse("reset"));
}
void test_unknown_names_are_none() {
    TEST_ASSERT_EQUAL_INT(None, phonepress::parse(""));
    TEST_ASSERT_EQUAL_INT(None, phonepress::parse(nullptr));
    TEST_ASSERT_EQUAL_INT(None, phonepress::parse("LYRICS"));      // exact, lower case
    TEST_ASSERT_EQUAL_INT(None, phonepress::parse("lyricsx"));
    TEST_ASSERT_EQUAL_INT(None, phonepress::parse("none"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_names_map_to_buttons);
    RUN_TEST(test_unknown_names_are_none);
    return UNITY_END();
}
