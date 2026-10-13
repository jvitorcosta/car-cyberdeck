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
    s.trackGen = 7;
    s.progressMs = 61000;
    s.durationMs = 200000;
    s.playing = true;
    s.tripS = 2220;
    s.lyricKind = "synced";
    s.line = "first made-up line";
    s.next = "second made-up line";
    s.lineStartMs = 59000;
    s.nextStartMs = 64000;
    return s;
}

void test_writes_the_whole_state() {
    char out[768];
    size_t n = statejson::write(sample(), out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(
        "{\"track\":\"Highway Song\",\"artist\":\"The Band\",\"status\":\"\",\"detail\":\"\","
        "\"wifi\":\"iPhone\",\"rssi\":-52,\"uptime\":754,\"clips\":[\"howl\"],"
        "\"trackGen\":7,\"progressMs\":61000,\"durationMs\":200000,\"playing\":true,\"tripS\":2220,"
        "\"lyricKind\":\"synced\",\"line\":\"first made-up line\",\"next\":\"second made-up line\","
        "\"lineStartMs\":59000,\"nextStartMs\":64000}",
        out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
}

void test_lyric_kind_names() {
    using lyricstatus::Status;
    TEST_ASSERT_EQUAL_STRING("searching", statejson::lyricKindName(Status::Searching));
    TEST_ASSERT_EQUAL_STRING("searching", statejson::lyricKindName(Status::Retrying));
    TEST_ASSERT_EQUAL_STRING("synced", statejson::lyricKindName(Status::Synced));
    TEST_ASSERT_EQUAL_STRING("plain", statejson::lyricKindName(Status::Plain));
    TEST_ASSERT_EQUAL_STRING("instrumental", statejson::lyricKindName(Status::Instrumental));
    TEST_ASSERT_EQUAL_STRING("none", statejson::lyricKindName(Status::None));
}

void test_escapes_quotes_backslashes_and_control_characters() {
    statejson::State s = sample();
    s.track = "Say \"Hi\" \\ now\n";
    s.artist = "Tab\there";
    char out[768];
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"track\":\"Say \\\"Hi\\\" \\\\ now\\n\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"artist\":\"Tab\\u0009here\""));
}

void test_utf8_passes_through() {
    statejson::State s = sample();
    s.track = "Canção";
    char out[768];
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"track\":\"Canção\""));
}

void test_null_strings_and_no_clips() {
    statejson::State s = sample();
    s.track = nullptr;
    s.clips = nullptr;
    s.clipCount = 0;
    char out[768];
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
    RUN_TEST(test_lyric_kind_names);
    RUN_TEST(test_escapes_quotes_backslashes_and_control_characters);
    RUN_TEST(test_utf8_passes_through);
    RUN_TEST(test_null_strings_and_no_clips);
    RUN_TEST(test_too_small_buffer_gives_an_empty_string_not_half_json);
    return UNITY_END();
}
