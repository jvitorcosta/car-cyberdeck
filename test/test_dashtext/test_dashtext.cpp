#include <unity.h>
#include <cstring>
#include "../../src/util/dashtext.h"

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

void test_temp() {
    char s[8];
    dashtext::temp(true, 31.6, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("32\xB0" "C", s);  // the degree sign, Latin-1 0xB0
    dashtext::temp(true, -9.5, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("-10\xB0" "C", s);
    dashtext::temp(true, -0.4, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("0\xB0" "C", s);
    dashtext::temp(false, 31.6, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("--\xB0" "C", s);
}

void test_hi_lo() {
    char s[48];
    dashtext::hiLo(true, 32.4, 25.6, "Manaus", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI 32 LO 26  MANAUS", s);
    dashtext::hiLo(true, -9.2, -12.0, "S\xC3\xA3o Paulo", s, sizeof(s));  // "São Paulo": Latin-1, accent kept
    TEST_ASSERT_EQUAL_STRING("HI -9 LO -12  S\xC3O PAULO", s);
    dashtext::hiLo(true, 30, 20, "Bras\xC3\xADlia \xE2\x80\x94 DF", s, sizeof(s));  // "Brasília — DF": the dash folded
    TEST_ASSERT_EQUAL_STRING("HI 30 LO 20  BRAS\xCDLIA - DF", s);
    dashtext::hiLo(false, 0, 0, "Manaus", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI -- LO --  MANAUS", s);
    dashtext::hiLo(true, 32.4, 25.6, "", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI 32 LO 26", s);
    dashtext::hiLo(false, 0, 0, nullptr, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI -- LO --", s);
    // the longest city the reading holds (23 characters) still comes whole
    dashtext::hiLo(true, -9, -12, "abcdefghijklmnopqrstuvw", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI -9 LO -12  ABCDEFGHIJKLMNOPQRSTUVW", s);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_weekday);
    RUN_TEST(test_date);
    RUN_TEST(test_temp);
    RUN_TEST(test_hi_lo);
    return UNITY_END();
}
