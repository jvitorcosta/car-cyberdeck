#include <unity.h>
#include <cstring>
#include "../../src/util/linkstatus.h"

using linkstatus::Kind;

void setUp() {}
void tearDown() {}

static Kind k(bool wifiUp, bool down, int code, bool cert) {
    return linkstatus::classify({wifiUp, down, code, cert});
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

void test_every_screen_fits_its_box() {
    // Title: font 4 in the 280 px dialogue box; detail: font 2 on the line under it.
    const char* longName = "A very long network name that never fits on the screen at all";
    Kind kinds[] = {Kind::NoWifi, Kind::NoInternet, Kind::SpotifyError, Kind::CertRejected};
    for (Kind kind : kinds) {
        char title[32], detail[64];
        linkstatus::describe(kind, 503, longName, title, sizeof title, detail, sizeof detail);
        TEST_ASSERT_TRUE(strlen(title) > 0 && strlen(title) <= linkstatus::TITLE_MAX);
        TEST_ASSERT_TRUE(strlen(detail) > 0 && strlen(detail) <= linkstatus::DETAIL_MAX);
    }
}

void test_no_internet_names_the_network_and_errors_show_the_code() {
    char title[32], detail[64];
    linkstatus::describe(Kind::NoInternet, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "iPhone"));
    linkstatus::describe(Kind::SpotifyError, 429, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "429"));
}

void test_a_cut_network_name_ends_in_an_ellipsis() {
    char title[32], detail[64];
    linkstatus::describe(Kind::NoWifi, 0, "Cecilia, iPhone, Office WiFi, Parents house 5G, Car",
                         title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_size_t(linkstatus::DETAIL_MAX, strlen(detail));
    TEST_ASSERT_EQUAL_STRING("...", detail + strlen(detail) - 3);
}

void test_small_buffers_are_never_overrun() {
    char title[6], detail[8];
    memset(title, 'x', sizeof title); memset(detail, 'x', sizeof detail);
    linkstatus::describe(Kind::NoInternet, -1, "iPhone", title, sizeof title, detail, sizeof detail);
    TEST_ASSERT_EQUAL_size_t(sizeof title - 1, strlen(title));
    TEST_ASSERT_EQUAL_size_t(sizeof detail - 1, strlen(detail));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_classify_picks_the_screen_for_each_cause);
    RUN_TEST(test_every_screen_fits_its_box);
    RUN_TEST(test_no_internet_names_the_network_and_errors_show_the_code);
    RUN_TEST(test_a_cut_network_name_ends_in_an_ellipsis);
    RUN_TEST(test_small_buffers_are_never_overrun);
    return UNITY_END();
}
