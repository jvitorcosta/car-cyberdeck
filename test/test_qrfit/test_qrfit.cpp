#include <unity.h>
#include <cstring>
#include "../../src/util/qrfit.h"

void setUp() {}
void tearDown() {}

void test_smallest_version_for_the_link() {
    TEST_ASSERT_EQUAL_INT(1, qrfit::version(17));
    TEST_ASSERT_EQUAL_INT(2, qrfit::version(18));
    TEST_ASSERT_EQUAL_INT(2, qrfit::version(32));
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(33));
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(53));
    // The real link with a 24-character key: http://172.20.10.10/?k=<24> = 47 bytes.
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(strlen("http://172.20.10.10/?k=0123456789abcdefghijklmn")));
}

void test_too_long_for_the_dialogue_box_is_zero() {
    TEST_ASSERT_EQUAL_INT(0, qrfit::version(54));
    TEST_ASSERT_EQUAL_INT(0, qrfit::version(0));
}

void test_drawn_size_with_the_standard_quiet_zone() {
    TEST_ASSERT_EQUAL_INT(21, qrfit::modules(1));
    TEST_ASSERT_EQUAL_INT(29, qrfit::modules(3));
    TEST_ASSERT_EQUAL_INT((29 + 8) * 2, qrfit::pixels(3));   // 74 px: fits the sky left of Pikachu
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_smallest_version_for_the_link);
    RUN_TEST(test_too_long_for_the_dialogue_box_is_zero);
    RUN_TEST(test_drawn_size_with_the_standard_quiet_zone);
    return UNITY_END();
}
