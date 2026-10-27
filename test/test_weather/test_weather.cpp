#include <unity.h>
#include <cstring>
#include "../../src/util/yellow/weather.h"

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
    char u[240];
    weather::url("-23.55", "-46.63", u, sizeof u);
    TEST_ASSERT_EQUAL_STRING("http://api.open-meteo.com/v1/forecast?latitude=-23.55&longitude=-46.63"
                             "&current=temperature_2m,weather_code,is_day"
                             "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto", u);
}

// A real-shaped answer to url(): current plus today's max/min; daily_units repeats the daily keys
// with string values first.
static const char* FULL =
    "{\"latitude\":-3.125,\"longitude\":-60.0,\"generationtime_ms\":0.06,\"utc_offset_seconds\":-14400,"
    "\"timezone\":\"America/Manaus\",\"timezone_abbreviation\":\"GMT-4\",\"elevation\":40.0,"
    "\"current_units\":{\"time\":\"iso8601\",\"interval\":\"seconds\",\"temperature_2m\":\"°C\","
    "\"weather_code\":\"wmo code\",\"is_day\":\"\"},"
    "\"current\":{\"time\":\"2026-10-23T12:30\",\"interval\":900,\"temperature_2m\":31.2,"
    "\"weather_code\":2,\"is_day\":0},"
    "\"daily_units\":{\"time\":\"iso8601\",\"temperature_2m_max\":\"°C\",\"temperature_2m_min\":\"°C\"},"
    "\"daily\":{\"time\":[\"2026-10-23\"],\"temperature_2m_max\":[33.4],\"temperature_2m_min\":[-1.5]}}";

void test_reads_current_and_daily() {
    weather::Now n{};
    TEST_ASSERT_TRUE(weather::parse(FULL, &n));
    TEST_ASSERT_TRUE(n.tempC > 31.19 && n.tempC < 31.21);
    TEST_ASSERT_EQUAL_INT(2, n.code);
    TEST_ASSERT_FALSE(n.day);
    double hi = 0, lo = 0;
    TEST_ASSERT_TRUE(weather::parseDaily(FULL, &hi, &lo));
    TEST_ASSERT_TRUE(hi > 33.39 && hi < 33.41);
    TEST_ASSERT_TRUE(lo > -1.51 && lo < -1.49);
}

void test_daily_missing_or_empty() {
    double hi = 0, lo = 0;
    TEST_ASSERT_FALSE(weather::parseDaily(ANSWER, &hi, &lo));   // no daily block
    TEST_ASSERT_FALSE(weather::parseDaily("{\"daily\":{\"temperature_2m_max\":[],\"temperature_2m_min\":[20]}}", &hi, &lo));
    TEST_ASSERT_FALSE(weather::parseDaily("{\"daily\":{\"temperature_2m_max\":[null],\"temperature_2m_min\":[20]}}", &hi, &lo));
    TEST_ASSERT_FALSE(weather::parseDaily("{\"daily\":{\"temperature_2m_max\":[30]}}", &hi, &lo));
    TEST_ASSERT_FALSE(weather::parseDaily(nullptr, &hi, &lo));
}

void test_words_for_each_band() {
    const struct { int code; const char* w; } C[] = {
        {0, "CLEAR"}, {1, "PARTLY CLOUDY"}, {2, "PARTLY CLOUDY"}, {3, "OVERCAST"}, {45, "FOG"}, {48, "FOG"},
        {51, "DRIZZLE"}, {57, "DRIZZLE"}, {61, "RAIN"}, {67, "RAIN"}, {71, "SNOW"}, {77, "SNOW"},
        {80, "SHOWERS"}, {82, "SHOWERS"}, {85, "SNOW SHOWERS"}, {86, "SNOW SHOWERS"},
        {95, "THUNDERSTORM"}, {99, "THUNDERSTORM"}, {100, "UNKNOWN"}, {4, "UNKNOWN"}, {46, "UNKNOWN"},
        {-1, "UNKNOWN"},
    };
    for (const auto& c : C) TEST_ASSERT_EQUAL_STRING_MESSAGE(c.w, weather::words(c.code), "wmo band");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_current_not_units);
    RUN_TEST(test_rejects_incomplete_answers);
    RUN_TEST(test_icons_for_wmo_codes);
    RUN_TEST(test_builds_the_request_url);
    RUN_TEST(test_reads_current_and_daily);
    RUN_TEST(test_daily_missing_or_empty);
    RUN_TEST(test_words_for_each_band);
    return UNITY_END();
}
