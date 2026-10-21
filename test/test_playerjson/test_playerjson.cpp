#include <unity.h>
#include <cstring>
#include <cstdio>
#include "../../src/util/playerjson.h"

void setUp() {}
void tearDown() {}

// Trimmed from a real /v1/me/player?market=BR&additional_types=episode answer: every field the
// parser reads, in Spotify's order, with the neighbours that share key names ("name", "uri").
static const char* TRACK =
    "{\"device\":{\"id\":\"abc\",\"is_active\":true,\"name\":\"iPhone de Vitor\","
    "\"type\":\"Smartphone\",\"volume_percent\":100,\"supports_volume\":false},"
    "\"shuffle_state\":true,\"smart_shuffle\":false,\"repeat_state\":\"context\","
    "\"timestamp\":1760000000000,\"context\":{\"external_urls\":{\"spotify\":\"https://x\"},"
    "\"href\":\"https://api.spotify.com/v1/playlists/37i\",\"type\":\"playlist\","
    "\"uri\":\"spotify:playlist:37i9dQZF1DXcBWIGoYBM5M\"},\"progress_ms\":61234,"
    "\"item\":{\"album\":{\"album_type\":\"album\",\"artists\":[{\"name\":\"Album Artist\","
    "\"uri\":\"spotify:artist:1\"}],\"images\":[{\"height\":640,\"url\":\"https://i.scdn.co/640\","
    "\"width\":640},{\"height\":300,\"url\":\"https://i.scdn.co/300\",\"width\":300},"
    "{\"height\":64,\"url\":\"https://i.scdn.co/64\",\"width\":64}],\"name\":\"Cora\\u00e7\\u00e3o\","
    "\"uri\":\"spotify:album:2\"},\"artists\":[{\"name\":\"Anitta\",\"uri\":\"spotify:artist:3\"},"
    "{\"name\":\"Feat\",\"uri\":\"spotify:artist:4\"}],\"duration_ms\":200000,"
    "\"explicit\":false,\"name\":\"Song \\\"One\\\"\",\"popularity\":70,"
    "\"uri\":\"spotify:track:5\"},\"currently_playing_type\":\"track\","
    "\"actions\":{\"disallows\":{\"resuming\":true}},\"is_playing\":true}";

void test_track() {
    playerjson::Now n;
    TEST_ASSERT_TRUE(playerjson::parse(TRACK, n));
    TEST_ASSERT_EQUAL(playerjson::Kind::Track, n.kind);
    TEST_ASSERT_TRUE(n.playing);
    TEST_ASSERT_EQUAL_UINT32(61234, n.progressMs);
    TEST_ASSERT_EQUAL_UINT32(200000, n.durationMs);
    TEST_ASSERT_EQUAL_STRING("Song \"One\"", n.name);
    TEST_ASSERT_EQUAL_STRING("spotify:track:5", n.uri);
    TEST_ASSERT_EQUAL_STRING("Anitta", n.artist);                    // the song's, not the album's
    TEST_ASSERT_EQUAL_STRING("Cora\xC3\xA7\xC3\xA3o", n.album);     // UTF-8 kept
    TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/300", n.artUrl);
    TEST_ASSERT_EQUAL_STRING("spotify:playlist:37i9dQZF1DXcBWIGoYBM5M", n.context);
    TEST_ASSERT_EQUAL_STRING("iPhone de Vitor", n.deviceName);
    TEST_ASSERT_EQUAL_STRING("Smartphone", n.deviceType);
    TEST_ASSERT_EQUAL_INT(100, n.volume);
    TEST_ASSERT_TRUE(n.shuffle);
    TEST_ASSERT_EQUAL_INT(1, n.repeat);
}

void test_episode_uses_show_and_episode_images() {
    const char* body =
        "{\"device\":{\"name\":\"Car\",\"type\":\"Speaker\",\"volume_percent\":null},"
        "\"repeat_state\":\"track\",\"shuffle_state\":false,\"context\":null,\"progress_ms\":5,"
        "\"item\":{\"duration_ms\":3600000,\"images\":[{\"url\":\"e640\"},{\"url\":\"e300\"},"
        "{\"url\":\"e64\"}],\"name\":\"Ep 1\",\"show\":{\"images\":[{\"url\":\"s\"}],"
        "\"name\":\"The Show\"},\"uri\":\"spotify:episode:9\"},"
        "\"currently_playing_type\":\"episode\",\"is_playing\":false}";
    playerjson::Now n;
    TEST_ASSERT_TRUE(playerjson::parse(body, n));
    TEST_ASSERT_EQUAL(playerjson::Kind::Episode, n.kind);
    TEST_ASSERT_FALSE(n.playing);
    TEST_ASSERT_EQUAL_STRING("Ep 1", n.name);
    TEST_ASSERT_EQUAL_STRING("The Show", n.artist);
    TEST_ASSERT_EQUAL_STRING("", n.album);
    TEST_ASSERT_EQUAL_STRING("e300", n.artUrl);
    TEST_ASSERT_EQUAL_STRING("", n.context);
    TEST_ASSERT_EQUAL_INT(-1, n.volume);
    TEST_ASSERT_EQUAL_INT(2, n.repeat);
    TEST_ASSERT_EQUAL_UINT32(3600000, n.durationMs);
}

void test_ad_and_empty_item() {
    playerjson::Now n;
    TEST_ASSERT_TRUE(playerjson::parse(
        "{\"item\":null,\"currently_playing_type\":\"ad\",\"is_playing\":true}", n));
    TEST_ASSERT_EQUAL(playerjson::Kind::Other, n.kind);
    TEST_ASSERT_EQUAL_STRING("", n.name);
    TEST_ASSERT_TRUE(playerjson::parse(
        "{\"item\":null,\"currently_playing_type\":\"unknown\",\"is_playing\":false}", n));
    TEST_ASSERT_EQUAL(playerjson::Kind::None, n.kind);
    TEST_ASSERT_TRUE(playerjson::parse(
        "{\"item\":{\"name\":\"x\"},\"currently_playing_type\":\"unknown\",\"is_playing\":true}", n));
    TEST_ASSERT_EQUAL(playerjson::Kind::Other, n.kind);
    TEST_ASSERT_EQUAL_STRING("", n.name);
}

void test_one_image_and_no_images() {
    playerjson::Now n;
    TEST_ASSERT_TRUE(playerjson::parse(
        "{\"item\":{\"album\":{\"images\":[{\"url\":\"only\"}]}},\"currently_playing_type\":\"track\","
        "\"is_playing\":true}", n));
    TEST_ASSERT_EQUAL_STRING("only", n.artUrl);
    TEST_ASSERT_TRUE(playerjson::parse(
        "{\"item\":{\"album\":{\"images\":[]}},\"currently_playing_type\":\"track\",\"is_playing\":true}", n));
    TEST_ASSERT_EQUAL_STRING("", n.artUrl);
}

void test_not_a_player_answer() {
    playerjson::Now n;
    n.volume = 7;
    TEST_ASSERT_FALSE(playerjson::parse("", n));
    TEST_ASSERT_EQUAL_INT(-1, n.volume);   // zeroed
    TEST_ASSERT_FALSE(playerjson::parse("<html>captive portal</html>", n));
    TEST_ASSERT_FALSE(playerjson::parse("{\"error\":{\"status\":401}}", n));
}

void test_cut_off_answers_never_overrun() {
    // A body cut anywhere (a dropped connection) parses or fails, never reads past its end (ASan).
    const size_t len = strlen(TRACK);
    static char buf[4096];
    TEST_ASSERT_TRUE(len < sizeof buf);
    for (size_t i = 0; i < len; ++i) {
        memcpy(buf, TRACK, i);
        buf[i] = '\0';
        playerjson::Now n;
        playerjson::parse(buf, n);
    }
}

void test_long_values_are_cut_to_fit() {
    char body[1024];
    char longName[400];
    memset(longName, 'a', sizeof longName - 1);
    longName[sizeof longName - 1] = '\0';
    snprintf(body, sizeof body,
             "{\"item\":{\"name\":\"%s\",\"uri\":\"%s\"},\"currently_playing_type\":\"track\","
             "\"is_playing\":true}", longName, longName);
    playerjson::Now n;
    TEST_ASSERT_TRUE(playerjson::parse(body, n));
    TEST_ASSERT_EQUAL_size_t(sizeof n.name - 1, strlen(n.name));
    TEST_ASSERT_EQUAL_size_t(sizeof n.uri - 1, strlen(n.uri));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_track);
    RUN_TEST(test_episode_uses_show_and_episode_images);
    RUN_TEST(test_ad_and_empty_item);
    RUN_TEST(test_one_image_and_no_images);
    RUN_TEST(test_not_a_player_answer);
    RUN_TEST(test_cut_off_answers_never_overrun);
    RUN_TEST(test_long_values_are_cut_to_fit);
    return UNITY_END();
}
