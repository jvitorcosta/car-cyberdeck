#include <unity.h>
#include <cstring>
#include "../../src/util/jsonflat.h"

void setUp() {}
void tearDown() {}

static const char* BODY =
    "{\"track\":\"A \\\"quoted\\\" title\",\"n\":-42,\"big\":64000,\"t\":28.5,\"on\":true,"
    "\"off\":false,\"clips\":[\"howl\",\"vroom\"],\"esc\":\"Can\\u00e7\\u00e3o \\\\ ok\\n\","
    "\"inner\":\"has \\\"n\\\": 5 inside\"}";

void test_reads_strings_numbers_and_bools() {
    char s[64];
    long n = 0;
    double d = 0;
    bool b = false;
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "track", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("A \"quoted\" title", s);
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "n", &n));
    TEST_ASSERT_EQUAL_INT32(-42, n);
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "big", &n));
    TEST_ASSERT_EQUAL_INT32(64000, n);
    TEST_ASSERT_TRUE(jsonflat::real(BODY, "t", &d));
    TEST_ASSERT_TRUE(d > 28.49 && d < 28.51);
    TEST_ASSERT_TRUE(jsonflat::boolean(BODY, "on", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_TRUE(jsonflat::boolean(BODY, "off", &b));
    TEST_ASSERT_FALSE(b);
}

void test_string_escapes_are_decoded() {
    char s[64];
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "esc", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("Canção \\ ok\n", s);   // ç ã -> UTF-8
}

void test_keys_inside_strings_are_not_keys() {
    long n = 0;
    TEST_ASSERT_FALSE(jsonflat::num(BODY, "n_missing", &n));
    // "n" appears inside the "inner" string value as text; the real key "n" is -42.
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "n", &n));
    TEST_ASSERT_EQUAL_INT32(-42, n);
}

void test_array_membership() {
    TEST_ASSERT_TRUE(jsonflat::arrayHas(BODY, "clips", "vroom"));
    TEST_ASSERT_TRUE(jsonflat::arrayHas(BODY, "clips", "howl"));
    TEST_ASSERT_FALSE(jsonflat::arrayHas(BODY, "clips", "honk"));
    TEST_ASSERT_FALSE(jsonflat::arrayHas(BODY, "track", "howl"));   // not an array
}

void test_wrong_types_and_small_buffers() {
    char s[6];
    long n = 0;
    TEST_ASSERT_FALSE(jsonflat::num(BODY, "track", &n));            // string, not a number
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "track", s, sizeof s));    // cut, still terminated
    TEST_ASSERT_EQUAL_size_t(5, strlen(s));
    TEST_ASSERT_FALSE(jsonflat::str(nullptr, "track", s, sizeof s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_strings_numbers_and_bools);
    RUN_TEST(test_string_escapes_are_decoded);
    RUN_TEST(test_keys_inside_strings_are_not_keys);
    RUN_TEST(test_array_membership);
    RUN_TEST(test_wrong_types_and_small_buffers);
    return UNITY_END();
}
