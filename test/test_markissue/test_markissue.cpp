#include <unity.h>
#include <cstring>
#include "../../src/util/car/markissue.h"

void setUp() {}
void tearDown() {}

void test_issue_json_has_the_lines_the_pc_reads() {
    char out[384];
    const int n = markissue::json(out, sizeof out, "2026-11-05 14:32:10", "2026-11-05T18:32:10Z",
                                  -3115885, -60015568, 90, 42);
    TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof out);
    TEST_ASSERT_NULL(strchr(out, '\n'));   // JSON strings can't hold raw line breaks
    TEST_ASSERT_NOT_NULL(strstr(out, "\"title\":\"Mark 2026-11-05 14:32:10\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "lat: -3.115885\\nlon: -60.015568\\nheading: 90\\nkmh: 42\\n"));
    TEST_ASSERT_NOT_NULL(strstr(out, "time: 2026-11-05T18:32:10Z\\n"));
    TEST_ASSERT_NOT_NULL(strstr(out, "https://www.google.com/maps?q=-3.115885,-60.015568\"}"));
}

void test_issue_json_without_a_clock() {
    char out[384];
    markissue::json(out, sizeof out, "", "", 1, -1, 0, 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "Mark (clock not set)"));
    TEST_ASSERT_NOT_NULL(strstr(out, "lat: 0.000001\\nlon: -0.000001"));
    TEST_ASSERT_NOT_NULL(strstr(out, "time: unknown"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_issue_json_has_the_lines_the_pc_reads);
    RUN_TEST(test_issue_json_without_a_clock);
    return UNITY_END();
}
