#include <unity.h>
#include <cstring>
#include "../../src/util/black/linkstatus.h"

using linkstatus::Kind;

void setUp() {}
void tearDown() {}

static Kind k(bool wifiUp, bool down, int code, bool cert, bool login = false) {
    return linkstatus::classify({wifiUp, down, code, cert, login});
}

void test_classify_picks_the_screen_for_each_cause() {
    struct { bool wifi, down; int code; bool cert; Kind want; const char* why; } rows[] = {
        {true,  false, 200, false, Kind::Ok,           "polls fine"},
        {true,  false, -1,  false, Kind::Ok,           "one failed poll: still debounced"},
        {false, false, 200, false, Kind::NoWifi,       "WiFi lost before the debounce"},
        {false, true,  401, true,  Kind::NoWifi,       "no WiFi wins over everything"},
        {true,  true,  -1,  false, Kind::NoInternet,   "connect timeout (hotspot without data)"},
        {true,  true,  0,   false, Kind::NoInternet,   "no poll answered yet"},
        {true,  true,  200, false, Kind::NoInternet,   "stale: polls stopped completing"},
        {true,  true,  429, false, Kind::SpotifyError, "rate limited"},
        {true,  true,  401, false, Kind::SpotifyError, "token rejected"},
        {true,  true,  503, false, Kind::SpotifyError, "Spotify down"},
        {true,  true,  -1,  true,  Kind::CertRejected, "certificate rejected"},
    };
    for (auto& r : rows) TEST_ASSERT_EQUAL_INT_MESSAGE((int)r.want, (int)k(r.wifi, r.down, r.code, r.cert), r.why);
}

// Spotify refused the login (the refresh token): its own screen, not "Spotify error", since only
// the owner can fix it (tools/spotify_auth.py). No WiFi still comes first; an OK link shows nothing.
void test_a_refused_login_has_its_own_screen() {
    TEST_ASSERT_EQUAL_INT((int)Kind::LoginFailed, (int)k(true, true, 400, false, true));
    TEST_ASSERT_EQUAL_INT((int)Kind::NoWifi, (int)k(false, true, 400, false, true));
    TEST_ASSERT_EQUAL_INT((int)Kind::Ok, (int)k(true, false, 400, false, true));
    TEST_ASSERT_EQUAL_INT((int)Kind::SpotifyError, (int)k(true, true, 400, false, false));   // the player's 400
    char title[32], detail[64];
    linkstatus::describe(Kind::LoginFailed, 400, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_STRING("Login failed", title);
    TEST_ASSERT_NOT_NULL(strstr(detail, "spotify_auth"));
}

void test_every_screen_fits_its_box() {
    // Title: font 4 in the 280 px dialogue box; detail: font 2 on the line under it.
    const char* longName = "A very long network name that never fits on the screen at all";
    Kind kinds[] = {Kind::NoWifi, Kind::NoInternet, Kind::SpotifyError, Kind::CertRejected, Kind::LoginFailed};
    for (Kind kind : kinds) {
        char title[32], detail[64];
        linkstatus::describe(kind, 503, -1, longName, title, sizeof title, detail, sizeof detail);
        TEST_ASSERT_TRUE(strlen(title) > 0 && strlen(title) <= linkstatus::TITLE_MAX);
        TEST_ASSERT_TRUE(strlen(detail) > 0 && strlen(detail) <= linkstatus::DETAIL_MAX);
    }
}

void test_no_internet_names_the_network_and_errors_show_the_code() {
    char title[32], detail[64];
    linkstatus::describe(Kind::NoInternet, -1, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "iPhone"));
    linkstatus::describe(Kind::SpotifyError, 503, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "503"));
}

void test_a_cut_network_name_ends_in_an_ellipsis() {
    char title[32], detail[64];
    linkstatus::describe(Kind::NoWifi, 0, -1, "Home WiFi, iPhone, Office WiFi, Parents house 5G, Car",
                         title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_size_t(linkstatus::DETAIL_MAX, strlen(detail));
    TEST_ASSERT_EQUAL_STRING("...", detail + strlen(detail) - 3);
}

void test_small_buffers_are_never_overrun() {
    char title[6], detail[8];
    memset(title, 'x', sizeof title); memset(detail, 'x', sizeof detail);
    linkstatus::describe(Kind::NoInternet, -1, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_size_t(sizeof title - 1, strlen(title));
    TEST_ASSERT_EQUAL_size_t(sizeof detail - 1, strlen(detail));
}

void test_rate_limit_counts_down_to_the_next_try() {
    char title[32], detail[64];
    struct { int s; const char* want; } rows[] = {
        {23,   "Too many requests, wait 23s"},
        {125,  "Too many requests, wait 2m 05s"},
        {6537, "Too many requests, wait 1h 48m"},
        {0,    "Too many requests, retrying..."},
        {-1,   "Too many requests, retrying..."},
    };
    for (auto& r : rows) {
        linkstatus::describe(Kind::SpotifyError, 429, r.s, "iPhone", title, sizeof title, detail, sizeof detail);
        TEST_ASSERT_EQUAL_STRING("Spotify busy...", title);
        TEST_ASSERT_EQUAL_STRING(r.want, detail);
        TEST_ASSERT_TRUE(strlen(detail) <= linkstatus::DETAIL_MAX);
    }
    linkstatus::describe(Kind::SpotifyError, 429, 86400, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_STRING("Too many requests, wait 24h 00m", detail);
}

void test_retry_seconds_round_up_and_survive_millis_wrap() {
    TEST_ASSERT_EQUAL_INT(-1, linkstatus::retryInS(5000, 0));          // no poll planned yet
    TEST_ASSERT_EQUAL_INT(23, linkstatus::retryInS(1000, 23500));      // 22.5 s -> 23
    TEST_ASSERT_EQUAL_INT(1, linkstatus::retryInS(1000, 1001));
    TEST_ASSERT_EQUAL_INT(0, linkstatus::retryInS(9000, 8000));        // overdue
    TEST_ASSERT_EQUAL_INT(10, linkstatus::retryInS(0xFFFFF000u, 0x00001710u));   // across the wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_classify_picks_the_screen_for_each_cause);
    RUN_TEST(test_every_screen_fits_its_box);
    RUN_TEST(test_a_refused_login_has_its_own_screen);
    RUN_TEST(test_no_internet_names_the_network_and_errors_show_the_code);
    RUN_TEST(test_a_cut_network_name_ends_in_an_ellipsis);
    RUN_TEST(test_small_buffers_are_never_overrun);
    RUN_TEST(test_rate_limit_counts_down_to_the_next_try);
    RUN_TEST(test_retry_seconds_round_up_and_survive_millis_wrap);
    return UNITY_END();
}
