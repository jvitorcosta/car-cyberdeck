#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/carstatus.h"

void setUp() {}
void tearDown() {}

void test_plain_status() {
    char out[carstatus::MAX_JSON];
    TEST_ASSERT_TRUE(carstatus::write({"live", 87, "auto", "drive", "and I'm still standing"}, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING(
        "{\"car\":\"live\",\"kmh\":87,\"pin\":\"auto\",\"show\":\"drive\",\"line\":\"and I'm still standing\"}", out);
}

void test_unknown_speed_and_no_line() {
    char out[carstatus::MAX_JSON];
    TEST_ASSERT_TRUE(carstatus::write({"off", -5, "lyrics", "menu", nullptr}, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("{\"car\":\"off\",\"kmh\":-1,\"pin\":\"lyrics\",\"show\":\"menu\",\"line\":\"\"}", out);
}

void test_line_is_escaped() {
    char out[carstatus::MAX_JSON];
    carstatus::write({"live", 0, "auto", "lyrics", "say \"hi\"\\\n\x01"}, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"line\":\"say \\\"hi\\\"\\\\\\n\\u0001\"}"));
}

// The worst line: MAX_LINE bytes of quotes, each escaped to two; it still fits MAX_JSON.
void test_worst_case_fits() {
    std::string q(400, '"');
    char out[carstatus::MAX_JSON];
    TEST_ASSERT_TRUE(carstatus::write({"connecting", 999, "lyrics", "lyrics", q.c_str()}, out, sizeof out));
    TEST_ASSERT_EQUAL_CHAR('}', out[strlen(out) - 1]);
}

// A long line is cut at MAX_LINE bytes, never inside a UTF-8 character.
void test_long_line_cut_on_a_character() {
    std::string s;
    for (int i = 0; i < 100; ++i) s += "\xE3\x81\x82";   // 3-byte characters
    char out[carstatus::MAX_JSON];
    carstatus::write({"live", 1, "auto", "drive", s.c_str()}, out, sizeof out);
    const char* line = strstr(out, "\"line\":\"") + 8;
    const size_t len = strlen(line) - 2;                  // without the closing quote and brace
    TEST_ASSERT_EQUAL_UINT(159, len);                     // 53 whole characters
}

void test_too_small_buffer_says_so() {
    char out[16];
    TEST_ASSERT_FALSE(carstatus::write({"live", 87, "auto", "drive", "x"}, out, sizeof out));
    TEST_ASSERT_EQUAL_UINT(15, strlen(out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plain_status);
    RUN_TEST(test_unknown_speed_and_no_line);
    RUN_TEST(test_line_is_escaped);
    RUN_TEST(test_worst_case_fits);
    RUN_TEST(test_long_line_cut_on_a_character);
    RUN_TEST(test_too_small_buffer_says_so);
    return UNITY_END();
}
