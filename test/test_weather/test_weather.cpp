#include <unity.h>
#include <cstring>
#include "../../src/util/weather.h"

void setUp() {}
void tearDown() {}

// Shape of an Open-Meteo answer for ?current=temperature_2m,weather_code,is_day
static const char* ANSWER =
    "{\"latitude\":-23.5,\"longitude\":-46.625,\"current_units\":{\"time\":\"iso8601\","
    "\"interval\":\"seconds\",\"temperature_2m\":\"°C\",\"weather_code\":\"wmo code\",\"is_day\":\"\"},"
    "\"current\":{\"time\":\"2026-10-13T13:45\",\"interval\":900,\"temperature_2m\":28.3,"
    "\"weather_code\":3,\"is_day\":1}}";

void test_reads_current_not_units() {
    weather::Now n{};
    TEST_ASSERT_TRUE(weather::parse(ANSWER, &n));
    TEST_ASSERT_TRUE(n.tempC > 28.29 && n.tempC < 28.31);
    TEST_ASSERT_EQUAL_INT(3, n.code);
    TEST_ASSERT_TRUE(n.day);
}

void test_rejects_incomplete_answers() {
    weather::Now n{};
    TEST_ASSERT_FALSE(weather::parse("{\"error\":true,\"reason\":\"bad lat\"}", &n));
    TEST_ASSERT_FALSE(weather::parse("", &n));
    TEST_ASSERT_FALSE(weather::parse(nullptr, &n));
}

void test_icons_for_wmo_codes() {
    using weather::Icon;
    TEST_ASSERT_EQUAL_INT((int)Icon::Sun, (int)weather::iconFor(0, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Moon, (int)weather::iconFor(1, false));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(3, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(45, true));    // fog
    TEST_ASSERT_EQUAL_INT((int)Icon::Rain, (int)weather::iconFor(61, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Rain, (int)weather::iconFor(81, false));
    TEST_ASSERT_EQUAL_INT((int)Icon::Storm, (int)weather::iconFor(95, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(73, true));    // snow: rare here
}

void test_builds_the_request_url() {
    char u[200];
    weather::url("-23.55", "-46.63", u, sizeof u);
    TEST_ASSERT_EQUAL_STRING("https://api.open-meteo.com/v1/forecast?latitude=-23.55&longitude=-46.63"
                             "&current=temperature_2m,weather_code,is_day", u);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_current_not_units);
    RUN_TEST(test_rejects_incomplete_answers);
    RUN_TEST(test_icons_for_wmo_codes);
    RUN_TEST(test_builds_the_request_url);
    return UNITY_END();
}
