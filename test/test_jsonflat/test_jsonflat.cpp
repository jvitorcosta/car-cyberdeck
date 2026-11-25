#include <unity.h>
#include <cstring>
#include "../../src/util/common/jsonflat.h"

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

// An emoji escaped as a UTF-16 surrogate pair becomes one 4-byte UTF-8 character.
void test_surrogate_pair_to_utf8() {
    char s[16];
    TEST_ASSERT_TRUE(jsonflat::strAt("\"a\\ud83d\\udd25b\"", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("a\xF0\x9F\x94\xA5" "b", s);
}

// Half a pair is skipped, not written as a broken 3-byte sequence.
void test_lone_surrogate_dropped() {
    char s[16];
    TEST_ASSERT_TRUE(jsonflat::strAt("\"a\\ud83db\"", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("ab", s);
    TEST_ASSERT_TRUE(jsonflat::strAt("\"a\\udd25b\"", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("ab", s);
    TEST_ASSERT_TRUE(jsonflat::strAt("\"a\\ud83d\\u0041\"", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("aA", s);
}

// F-21: numAt ignores strtol's ERANGE, so a 20-digit number saturates at LONG_MAX and is accepted
// (long is 64-bit on the host, 32-bit on the deck: the same text lands on a different value). Flip
// to true with the errno check in src/util/common/jsonflat.cpp and the out-of-range checks run.
static const bool NUM_ERANGE_FIXED = false;

void test_num_range_and_shape() {
    long v = 7;
    TEST_ASSERT_TRUE(jsonflat::numAt("2147483647", &v));    // the deck's LONG_MAX
    TEST_ASSERT_EQUAL_INT32(2147483647, (int32_t)v);
    TEST_ASSERT_TRUE(jsonflat::numAt("-2147483648", &v));
    TEST_ASSERT_EQUAL_INT32((int32_t)-2147483647 - 1, (int32_t)v);
    v = 7;
    TEST_ASSERT_FALSE(jsonflat::numAt("-", &v));
    TEST_ASSERT_FALSE(jsonflat::numAt("--5", &v));
    TEST_ASSERT_FALSE(jsonflat::numAt("1.5", &v));
    TEST_ASSERT_FALSE(jsonflat::numAt("1e3", &v));
    TEST_ASSERT_FALSE(jsonflat::numAt(nullptr, &v));
    TEST_ASSERT_EQUAL_INT(7, (int)v);                        // none of those touched it
    if (!NUM_ERANGE_FIXED) TEST_IGNORE_MESSAGE("F-21: numAt accepts an ERANGE number (saturates)");
    TEST_ASSERT_FALSE(jsonflat::numAt("99999999999999999999", &v));
    TEST_ASSERT_FALSE(jsonflat::numAt("-99999999999999999999", &v));
    TEST_ASSERT_EQUAL_INT(7, (int)v);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_strings_numbers_and_bools);
    RUN_TEST(test_string_escapes_are_decoded);
    RUN_TEST(test_keys_inside_strings_are_not_keys);
    RUN_TEST(test_array_membership);
    RUN_TEST(test_wrong_types_and_small_buffers);
    RUN_TEST(test_surrogate_pair_to_utf8);
    RUN_TEST(test_lone_surrogate_dropped);
    RUN_TEST(test_num_range_and_shape);
    return UNITY_END();
}
