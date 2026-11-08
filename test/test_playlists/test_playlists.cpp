#include <unity.h>
#include <cstring>
#include "../../src/util/black/playlists.h"

void setUp() {}
void tearDown() {}

// A /v1/me/playlists page as Spotify sends it, trimmed: a null item (a deleted playlist), a name with
// an escaped newline and an accent, nested "name"s (the owner) that must not be read as the list's.
static const char* PAGE_BODY =
    "{\"href\":\"https://api.spotify.com/v1/me/playlists?offset=8&limit=8\",\"limit\":8,"
    "\"next\":\"https://api.spotify.com/v1/me/playlists?offset=16&limit=8\",\"offset\":8,\"previous\":null,"
    "\"total\":37,\"items\":["
    "{\"collaborative\":false,\"description\":\"name: {not this}\",\"id\":\"37i9dQZF1DXcBWIGoYBM5M\","
    "\"images\":[{\"url\":\"https://i.scdn.co/a\",\"height\":640,\"width\":640}],\"name\":\"Today's Top Hits\","
    "\"owner\":{\"display_name\":\"Spotify\",\"id\":\"spotify\",\"name\":\"owner\"},\"public\":true,"
    "\"tracks\":{\"href\":\"x\",\"total\":50},\"type\":\"playlist\"},"
    "null,"
    "{\"id\":\"0vvXsWCC9xrXsKd4FyS8kM\",\"name\":\"Caf\\u00e9\\nnoite\",\"owner\":{\"id\":\"jv\"}}"
    "]}";

void test_valid_ids_are_22_base62_characters() {
    TEST_ASSERT_TRUE(playlists::validId("37i9dQZF1DXcBWIGoYBM5M"));
    TEST_ASSERT_FALSE(playlists::validId("37i9dQZF1DXcBWIGoYBM5"));     // 21
    TEST_ASSERT_FALSE(playlists::validId("37i9dQZF1DXcBWIGoYBM5MM"));   // 23
    TEST_ASSERT_FALSE(playlists::validId("37i9dQZF1DXcBWIGoY\"M5M"));   // would end the JSON string
    TEST_ASSERT_FALSE(playlists::validId("37i9dQZF1DXcBWIGoY/M5M"));
    TEST_ASSERT_FALSE(playlists::validId(""));
    TEST_ASSERT_FALSE(playlists::validId(nullptr));
}

void test_offsets_are_plain_digits_within_the_cap() {
    int o = -1;
    TEST_ASSERT_TRUE(playlists::parseOffset("0", &o));
    TEST_ASSERT_EQUAL_INT(0, o);
    TEST_ASSERT_TRUE(playlists::parseOffset("16", &o));
    TEST_ASSERT_EQUAL_INT(16, o);
    TEST_ASSERT_TRUE(playlists::parseOffset("500", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset("501", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset("-1", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset("1e3", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset("99999", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset("", &o));
    TEST_ASSERT_FALSE(playlists::parseOffset(nullptr, &o));
    TEST_ASSERT_EQUAL_INT(500, o);   // untouched by the refusals
}

void test_page_lists_ids_and_names_and_the_next_offset() {
    char out[playlists::REPLY_MAX];
    TEST_ASSERT_EQUAL_INT(2, playlists::writePage(PAGE_BODY, 8, out, sizeof out));
    // next counts the null item too: it is Spotify's offset, not the lines
    TEST_ASSERT_EQUAL_STRING("playlists 37 11\n"
                             "37i9dQZF1DXcBWIGoYBM5M Today's Top Hits\n"
                             "0vvXsWCC9xrXsKd4FyS8kM Caf\xC3\xA9 noite\n", out);
}

void test_an_empty_page_is_just_the_header() {
    char out[64];
    TEST_ASSERT_EQUAL_INT(0, playlists::writePage("{\"items\":[],\"total\":0}", 0, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("playlists 0 0\n", out);
}

void test_long_names_are_cut_on_a_character() {
    // 47 'a' then "é": the accent doesn't fit whole in 48 bytes, so it goes
    char body[256];
    snprintf(body, sizeof body, "{\"total\":1,\"items\":[{\"id\":\"37i9dQZF1DXcBWIGoYBM5M\",\"name\":\"%s\\u00e9x\"}]}",
             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    char out[playlists::REPLY_MAX];
    TEST_ASSERT_EQUAL_INT(1, playlists::writePage(body, 0, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("playlists 1 1\n37i9dQZF1DXcBWIGoYBM5M aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n",
                             out);
}

void test_bad_bodies_and_short_buffers_fail_empty() {
    char out[playlists::REPLY_MAX];
    TEST_ASSERT_EQUAL_INT(-1, playlists::writePage("{\"error\":{\"status\":401}}", 0, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_EQUAL_INT(-1, playlists::writePage("{\"items\":[{\"id\":\"x\"", 0, out, sizeof out));   // cut off
    TEST_ASSERT_EQUAL_INT(-1, playlists::writePage(nullptr, 0, out, sizeof out));
    char tiny[40];
    TEST_ASSERT_EQUAL_INT(-1, playlists::writePage(PAGE_BODY, 8, tiny, sizeof tiny));
    TEST_ASSERT_EQUAL_STRING("", tiny);
}

void test_a_full_page_fits_the_reply_buffer() {
    // PAGE playlists with the longest names: REPLY_MAX must hold them all
    char body[2048] = "{\"total\":99,\"items\":[";
    for (int i = 0; i < playlists::PAGE; ++i) {
        char item[160];
        snprintf(item, sizeof item, "%s{\"id\":\"37i9dQZF1DXcBWIGoYBM5%c\",\"name\":\"%s\"}", i ? "," : "", 'A' + i,
                 "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn");
        strcat(body, item);
    }
    strcat(body, "]}");
    char out[playlists::REPLY_MAX];
    TEST_ASSERT_EQUAL_INT(playlists::PAGE, playlists::writePage(body, 492, out, sizeof out));
    TEST_ASSERT_NOT_NULL(strstr(out, "playlists 99 500\n"));
}

void test_play_body_names_the_playlist() {
    char out[80];
    TEST_ASSERT_TRUE(playlists::playBody("37i9dQZF1DXcBWIGoYBM5M", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("{\"context_uri\":\"spotify:playlist:37i9dQZF1DXcBWIGoYBM5M\"}", out);
    TEST_ASSERT_FALSE(playlists::playBody("bad\"id", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("", out);
    char tiny[20];
    TEST_ASSERT_FALSE(playlists::playBody("37i9dQZF1DXcBWIGoYBM5M", tiny, sizeof tiny));
    TEST_ASSERT_EQUAL_STRING("", tiny);
}

void test_spotify_errors_read_as_what_to_do() {
    const char* noDevice =
        "{\"error\":{\"status\":404,\"message\":\"Player command failed: No active device found\","
        "\"reason\":\"NO_ACTIVE_DEVICE\"}}";
    const char* premium =
        "{\"error\":{\"status\":403,\"message\":\"Player command failed: Premium required\","
        "\"reason\":\"PREMIUM_REQUIRED\"}}";
    TEST_ASSERT_EQUAL_STRING("Open Spotify on phone", playlists::why(404, noDevice, true));
    TEST_ASSERT_EQUAL_STRING("Open Spotify on phone", playlists::why(404, "", true));
    TEST_ASSERT_EQUAL_STRING("Not found", playlists::why(404, "", false));
    TEST_ASSERT_EQUAL_STRING("Needs Spotify Premium", playlists::why(403, premium, true));
    TEST_ASSERT_EQUAL_STRING("Re-authorise Spotify",
                             playlists::why(403, "{\"error\":{\"status\":403,\"message\":\"Insufficient client scope\"}}", true));
    TEST_ASSERT_EQUAL_STRING("Spotify refused", playlists::why(403, "", true));
    TEST_ASSERT_EQUAL_STRING("Spotify login failed", playlists::why(401, nullptr, false));
    TEST_ASSERT_EQUAL_STRING("Spotify busy: wait", playlists::why(429, "", false));
    TEST_ASSERT_EQUAL_STRING("Spotify down: retry", playlists::why(503, "", true));
    TEST_ASSERT_EQUAL_STRING("Spotify not answering", playlists::why(-1, "", true));
    TEST_ASSERT_EQUAL_STRING("Spotify error", playlists::why(418, "", true));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_ids_are_22_base62_characters);
    RUN_TEST(test_offsets_are_plain_digits_within_the_cap);
    RUN_TEST(test_page_lists_ids_and_names_and_the_next_offset);
    RUN_TEST(test_an_empty_page_is_just_the_header);
    RUN_TEST(test_long_names_are_cut_on_a_character);
    RUN_TEST(test_bad_bodies_and_short_buffers_fail_empty);
    RUN_TEST(test_a_full_page_fits_the_reply_buffer);
    RUN_TEST(test_play_body_names_the_playlist);
    RUN_TEST(test_spotify_errors_read_as_what_to_do);
    return UNITY_END();
}
