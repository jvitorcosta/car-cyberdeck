#include <unity.h>
#include <cstring>
#include "../../src/util/wire/statejson.h"

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

// The deck's own buffer (statejson::MAX_JSON) holds the largest state it can produce: every
// string at the size the deck stores it (track and artist 96, status 24, detail 48, ssid 33,
// lyric lines 160), all of them quotes (escaped to 2 bytes each), every number at its widest.
void test_the_largest_state_fits_max_json() {
    static char track[96], artist[96], title[24], detail[48], ssid[33], line[160], next[160];
    char* fields[] = {track, artist, title, detail, ssid, line, next};
    const size_t sizes[] = {sizeof track, sizeof artist, sizeof title, sizeof detail, sizeof ssid,
                            sizeof line, sizeof next};
    for (int i = 0; i < 7; ++i) { memset(fields[i], '"', sizes[i] - 1); fields[i][sizes[i] - 1] = 0; }
    statejson::State s = sample();
    s.track = track; s.artist = artist; s.title = title; s.detail = detail; s.ssid = ssid;
    s.line = line; s.next = next;
    s.rssi = -2147483647; s.uptimeS = s.trackGen = s.progressMs = s.durationMs = s.tripS = 4294967295u;
    s.lineStartMs = s.nextStartMs = 4294967295u;
    s.lyricKind = "instrumental";
    s.fw = "100.100.100+99999.gabcdef0123.dirty";
    static char out[statejson::MAX_JSON];
    TEST_ASSERT_NOT_EQUAL(0, statejson::write(s, out, sizeof out));
}

// The firmware's version goes last as "fw" when the deck sets it; without one there is no key.
void test_fw_goes_last_when_set() {
    statejson::State s = sample();
    s.fw = "1.1.0+226.g41b2b7a";
    char out[768];
    const size_t n = statejson::write(s, out, sizeof out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
    const char* tail = ",\"nextStartMs\":64000,\"fw\":\"1.1.0+226.g41b2b7a\"}";
    TEST_ASSERT_EQUAL_STRING(tail, out + n - strlen(tail));
    s.fw = "";
    statejson::write(s, out, sizeof out);
    TEST_ASSERT_NULL(strstr(out, "\"fw\""));
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
    RUN_TEST(test_the_largest_state_fits_max_json);
    RUN_TEST(test_fw_goes_last_when_set);
    RUN_TEST(test_lyric_kind_names);
    RUN_TEST(test_escapes_quotes_backslashes_and_control_characters);
    RUN_TEST(test_utf8_passes_through);
    RUN_TEST(test_null_strings_and_no_clips);
    RUN_TEST(test_too_small_buffer_gives_an_empty_string_not_half_json);
    return UNITY_END();
}
