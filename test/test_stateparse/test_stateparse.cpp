#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/wire/stateparse.h"

void setUp() {}
void tearDown() {}

static const char* FULL =
    "{\"track\":\"Highway Song\",\"artist\":\"The Band\",\"status\":\"\",\"detail\":\"\","
    "\"wifi\":\"iPhone\",\"rssi\":-52,\"uptime\":754,\"clips\":[\"howl\",\"vroom\"],"
    "\"trackGen\":7,\"progressMs\":61000,\"durationMs\":200000,\"playing\":true,\"tripS\":2220,"
    "\"lyricKind\":\"synced\",\"line\":\"first made-up line\",\"next\":\"second made-up line\","
    "\"lineStartMs\":59000,\"nextStartMs\":64000}";

void test_full_state() {
    stateparse::Deck d{};
    TEST_ASSERT_TRUE(stateparse::parse(FULL, d));
    TEST_ASSERT_EQUAL_UINT32(7, d.trackGen);
    TEST_ASSERT_EQUAL_UINT32(61000, d.progressMs);
    TEST_ASSERT_EQUAL_UINT32(2220, d.tripS);
    TEST_ASSERT_TRUE(d.playing);
    TEST_ASSERT_TRUE(d.hasVroom);
    TEST_ASSERT_EQUAL_STRING("Highway Song", d.track);
    TEST_ASSERT_EQUAL_STRING("synced", d.lyricKind);
    TEST_ASSERT_EQUAL_STRING("first made-up line", d.line);
    TEST_ASSERT_EQUAL_STRING("second made-up line", d.next);
    TEST_ASSERT_EQUAL_UINT32(64000, d.nextStartMs);
}

void test_older_deck_without_the_new_fields() {
    // A deck from before Task 1: no timing or lyric keys. Parsed, with neutral defaults.
    const char* old = "{\"track\":\"T\",\"artist\":\"A\",\"status\":\"\",\"detail\":\"\",\"wifi\":\"x\","
                      "\"rssi\":-60,\"uptime\":5,\"clips\":[\"howl\"]}";
    stateparse::Deck d{};
    TEST_ASSERT_TRUE(stateparse::parse(old, d));
    TEST_ASSERT_EQUAL_UINT32(0, d.trackGen);
    TEST_ASSERT_EQUAL_STRING("none", d.lyricKind);
    TEST_ASSERT_EQUAL_STRING("", d.line);
    TEST_ASSERT_FALSE(d.hasVroom);
}

void test_truncated_body_is_rejected() {
    stateparse::Deck d{};
    d.trackGen = 99;
    std::string cut(FULL, strlen(FULL) - 20);   // lost the tail on a weak link
    TEST_ASSERT_FALSE(stateparse::parse(cut.c_str(), d));
    TEST_ASSERT_EQUAL_UINT32(99, d.trackGen);    // untouched: the screen keeps the last state
    TEST_ASSERT_FALSE(stateparse::parse("", d));
    TEST_ASSERT_FALSE(stateparse::parse("<html>captive portal</html>", d));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_state);
    RUN_TEST(test_older_deck_without_the_new_fields);
    RUN_TEST(test_truncated_body_is_rejected);
    return UNITY_END();
}
