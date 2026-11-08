#include <unity.h>
#include <cstring>
#include "../../src/util/yellow/dashtext.h"

void setUp() {}
void tearDown() {}

void test_weekday() {
    TEST_ASSERT_EQUAL_INT(5, dashtext::weekday(2026, 10, 23));  // Friday
    TEST_ASSERT_EQUAL_INT(6, dashtext::weekday(2000, 1, 1));    // Saturday
    TEST_ASSERT_EQUAL_INT(4, dashtext::weekday(2024, 2, 29));   // Thursday, a leap day
    TEST_ASSERT_EQUAL_INT(4, dashtext::weekday(1970, 1, 1));
    TEST_ASSERT_EQUAL_INT(4, dashtext::weekday(2026, 12, 31));
    TEST_ASSERT_EQUAL_INT(1, dashtext::weekday(2100, 3, 1));    // 2100 is no leap year
    TEST_ASSERT_EQUAL_INT(0, dashtext::weekday(2026, 10, 25));  // Sunday
}

void test_date() {
    char s[16];
    dashtext::date(true, 2026, 10, 23, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("FRI 23 OCT", s);
    dashtext::date(true, 2026, 1, 4, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("SUN 4 JAN", s);
    dashtext::date(true, 2026, 12, 31, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("THU 31 DEC", s);
    // before the clock synced, or a date that can't be
    dashtext::date(false, 2026, 10, 23, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--- -- ---", s);
    dashtext::date(true, 2026, 13, 1, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--- -- ---", s);
    dashtext::date(true, 2026, 0, 1, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--- -- ---", s);
    dashtext::date(true, 2026, 2, 0, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--- -- ---", s);
    dashtext::date(true, 2026, 2, 32, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--- -- ---", s);
    char small[6];
    dashtext::date(true, 2026, 10, 23, small, sizeof(small));
    TEST_ASSERT_EQUAL_STRING("FRI 2", small);  // cut, still terminated
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_weekday);
    RUN_TEST(test_date);
    return UNITY_END();
}
