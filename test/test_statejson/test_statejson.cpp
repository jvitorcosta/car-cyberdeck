#include <unity.h>
#include <cstring>
#include "../../src/util/statejson.h"

void setUp() {}
void tearDown() {}

static const char* const CLIPS[] = {"howl"};

static statejson::State sample() {
    statejson::State s{};
    s.track = "Highway Song";
    s.artist = "The Band";
    s.title = "";
    s.detail = "";
    s.ssid = "iPhone";
    s.rssi = -52;
    s.uptimeS = 754;
    s.clips = CLIPS;
    s.clipCount = 1;
    return s;
}

void test_writes_the_whole_state() {
    char out[512];
    size_t n = statejson::write(sample(), out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(
        "{\"track\":\"Highway Song\",\"artist\":\"The Band\",\"status\":\"\",\"detail\":\"\","
        "\"wifi\":\"iPhone\",\"rssi\":-52,\"uptime\":754,\"clips\":[\"howl\"]}",
        out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
}

void test_escapes_quotes_backslashes_and_control_characters() {
    statejson::State s = sample();
    s.track = "Say \"Hi\" \\ now\n";
    s.artist = "Tab\there";
    char out[512];
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"track\":\"Say \\\"Hi\\\" \\\\ now\\n\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"artist\":\"Tab\\u0009here\""));
}

void test_utf8_passes_through() {
    statejson::State s = sample();
    s.track = "Canção";
    char out[512];
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"track\":\"Canção\""));
}

void test_null_strings_and_no_clips() {
    statejson::State s = sample();
    s.track = nullptr;
    s.clips = nullptr;
    s.clipCount = 0;
    char out[512];
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"track\":\"\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"clips\":[]"));
}

void test_too_small_buffer_gives_an_empty_string_not_half_json() {
    char out[40];
    memset(out, 'x', sizeof out);
    TEST_ASSERT_EQUAL_size_t(0, statejson::write(sample(), out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_writes_the_whole_state);
    RUN_TEST(test_escapes_quotes_backslashes_and_control_characters);
    RUN_TEST(test_utf8_passes_through);
    RUN_TEST(test_null_strings_and_no_clips);
    RUN_TEST(test_too_small_buffer_gives_an_empty_string_not_half_json);
    return UNITY_END();
}
