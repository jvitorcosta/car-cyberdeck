#include <unity.h>
#include <cstring>
#include "../../src/util/geoip.h"

void setUp() {}
void tearDown() {}

void test_url_asks_for_the_few_fields() {
    TEST_ASSERT_EQUAL_STRING("http://ip-api.com/json/?fields=status,lat,lon,city", geoip::URL);
}

void test_reads_a_success_answer() {
    geoip::Place p{};
    TEST_ASSERT_TRUE(geoip::parse("{\"status\":\"success\",\"city\":\"Manaus\",\"lat\":-3.1019,\"lon\":-60.025}", &p));
    TEST_ASSERT_EQUAL_STRING("-3.1019", p.lat);
    TEST_ASSERT_EQUAL_STRING("-60.025", p.lon);
    TEST_ASSERT_EQUAL_STRING("Manaus", p.city);
}

void test_cuts_long_numbers_and_cities() {
    geoip::Place p{};
    TEST_ASSERT_TRUE(geoip::parse("{\"status\":\"success\",\"city\":\"S\u00e3o Jos\u00e9 dos Campos do Norte\","
                                  "\"lat\":-23.5557123456,\"lon\":-46.6395999}", &p));
    TEST_ASSERT_EQUAL_STRING("-23.5557", p.lat);
    TEST_ASSERT_EQUAL_STRING("-46.6395", p.lon);
    TEST_ASSERT_TRUE(strlen(p.city) <= 23);
    TEST_ASSERT_EQUAL_STRING("S\xc3\xa3o Jos\xc3\xa9 dos Campos d", p.city);   // whole characters, 23 bytes
    // An integer and no city are fine.
    TEST_ASSERT_TRUE(geoip::parse("{\"status\":\"success\",\"lat\":10,\"lon\":0}", &p));
    TEST_ASSERT_EQUAL_STRING("10", p.lat);
    TEST_ASSERT_EQUAL_STRING("0", p.lon);
    TEST_ASSERT_EQUAL_STRING("", p.city);
}

void test_rejects_failures() {
    geoip::Place p{};
    TEST_ASSERT_FALSE(geoip::parse("{\"status\":\"fail\",\"message\":\"private range\"}", &p));
    TEST_ASSERT_FALSE(geoip::parse("{\"status\":\"fail\",\"lat\":1.5,\"lon\":2.5}", &p));
    TEST_ASSERT_FALSE(geoip::parse("{\"status\":\"success\",\"city\":\"Manaus\",\"lat\":-3.1019}", &p));   // no lon
    TEST_ASSERT_FALSE(geoip::parse("{\"status\":\"success\",\"lat\":\"x\",\"lon\":2}", &p));
    TEST_ASSERT_FALSE(geoip::parse("{\"lat\":1,\"lon\":2}", &p));
    TEST_ASSERT_FALSE(geoip::parse("", &p));
    TEST_ASSERT_FALSE(geoip::parse(nullptr, &p));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_url_asks_for_the_few_fields);
    RUN_TEST(test_reads_a_success_answer);
    RUN_TEST(test_cuts_long_numbers_and_cities);
    RUN_TEST(test_rejects_failures);
    return UNITY_END();
}
