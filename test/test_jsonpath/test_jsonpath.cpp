#include <unity.h>
#include <cstring>
#include "../../src/util/jsonpath.h"

void setUp() {}
void tearDown() {}

// The same key ("name") at several depths, a string holding braces and quotes, nested arrays.
static const char* BODY =
    "{ \"device\": {\"name\": \"iPhone\", \"volume_percent\": 64},\n"
    "  \"note\": \"a {tricky} \\\"name\\\": [1,2]\",\n"
    "  \"item\": {\"name\": \"Song\", \"artists\": [{\"name\": \"First\"}, {\"name\": \"Second\"}],\n"
    "            \"album\": {\"name\": \"Album\", \"images\": [[9], {\"url\": \"u0\"}, {\"url\": \"u1\"}]}},\n"
    "  \"is_playing\": true, \"context\": null, \"progress_ms\": 12345 }";

void test_reads_by_path_at_any_depth() {
    char s[32];
    long n = 0;
    bool b = false;
    TEST_ASSERT_TRUE(jsonpath::str(BODY, "device.name", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("iPhone", s);
    TEST_ASSERT_TRUE(jsonpath::str(BODY, "item.name", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("Song", s);
    TEST_ASSERT_TRUE(jsonpath::str(BODY, "item.artists.1.name", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("Second", s);
    TEST_ASSERT_TRUE(jsonpath::str(BODY, "item.album.images.2.url", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("u1", s);
    TEST_ASSERT_TRUE(jsonpath::num(BODY, "device.volume_percent", &n));
    TEST_ASSERT_EQUAL_INT32(64, n);
    TEST_ASSERT_TRUE(jsonpath::num(BODY, "progress_ms", &n));
    TEST_ASSERT_EQUAL_INT32(12345, n);
    TEST_ASSERT_TRUE(jsonpath::boolean(BODY, "is_playing", &b));
    TEST_ASSERT_TRUE(b);
}

void test_missing_steps_and_wrong_kinds_are_null() {
    char s[8] = "x";
    const char* missing[] = {"nope", "item.nope", "item.artists.2.name", "item.artists.name",
                             "device.name.x", "context.name", "item..name", "item.artists.-1",
                             "item.album.images.0.url", ""};
    for (const char* path : missing) {
        if (!*path) continue;   // "" is the whole body
        TEST_ASSERT_NULL_MESSAGE(jsonpath::find(BODY, path), path);
    }
    TEST_ASSERT_FALSE(jsonpath::str(BODY, "nope", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("", s);
    TEST_ASSERT_FALSE(jsonpath::str(BODY, "device.volume_percent", s, sizeof s));   // a number
    TEST_ASSERT_NULL(jsonpath::find(nullptr, "a"));
    TEST_ASSERT_NULL(jsonpath::find(BODY, nullptr));
}

void test_whole_body_for_an_empty_path() {
    TEST_ASSERT_EQUAL_PTR(BODY, jsonpath::find(BODY, ""));
}

void test_a_key_inside_a_string_never_matches() {
    // "note" holds the text  "name": [1,2]  ; only real keys count.
    TEST_ASSERT_NULL(jsonpath::find("{\"note\":\"\\\"name\\\": 1\"}", "name"));
}

void test_cut_off_bodies_fail_cleanly() {
    // Every prefix of the body: never a crash or an out-of-bounds read (ASan), and a value that
    // is cut off is not reported.
    const size_t len = strlen(BODY);
    char buf[512];
    TEST_ASSERT_TRUE(len < sizeof buf);
    for (size_t i = 0; i < len; ++i) {
        memcpy(buf, BODY, i);
        buf[i] = '\0';
        char s[32];
        if (jsonpath::str(buf, "item.album.images.2.url", s, sizeof s)) TEST_ASSERT_EQUAL_STRING("u1", s);
        long n;
        jsonpath::num(buf, "progress_ms", &n);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reads_by_path_at_any_depth);
    RUN_TEST(test_missing_steps_and_wrong_kinds_are_null);
    RUN_TEST(test_whole_body_for_an_empty_path);
    RUN_TEST(test_a_key_inside_a_string_never_matches);
    RUN_TEST(test_cut_off_bodies_fail_cleanly);
    return UNITY_END();
}
