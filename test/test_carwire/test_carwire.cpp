#include <unity.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../../src/util/car/carwire.h"

void setUp() {}
void tearDown() {}

using carwire::Snapshot;

static Snapshot full() {
    Snapshot s;
    s.link = carwire::Link::Live;
    s.checkReady = true;
    s.kmh = 63.5f; s.rpm = 2150.0f; s.volts = 13.84f; s.coolantC = 91.0f; s.kmlNow = 14.3f;
    s.battery.level = carhealth::Level::Note; s.battery.known = true;
    strcpy(s.battery.why, "BATTERY WAS LOW LAST TRIP");
    s.engine.level = carhealth::Level::Problem; s.engine.known = true;
    strcpy(s.engine.why, "CHECK ENGINE LIGHT ON");
    s.trip = {12.34f, 0.987f, 12.5f, 41.2f, 1800, 120};
    s.tripCostRs = 6.41f;
    s.hasLast = true;
    s.last = {30.25f, 2.5f, 12.1f, 38.0f, 3600, 300};
    s.lastCostRs = 16.23f;
    s.rangeKm = 410.5f; s.pricePerL = 6.49f;
    s.critical = true;
    s.seq = 4000000000u;
    return s;
}

static void same(float a, float b, float tol) {
    if (std::isnan(a)) TEST_ASSERT_TRUE(std::isnan(b));
    else TEST_ASSERT_FLOAT_WITHIN(tol, a, b);
}

static void sameSummary(const tripsum::Summary& a, const tripsum::Summary& b) {
    same(a.km, b.km, 0.006f); same(a.litres, b.litres, 0.0006f); same(a.kmL, b.kmL, 0.06f);
    same(a.avgKmh, b.avgKmh, 0.06f);
    TEST_ASSERT_EQUAL_UINT32(a.seconds, b.seconds);
    TEST_ASSERT_EQUAL_UINT32(a.idleS, b.idleS);
}

static void sameSnap(const Snapshot& a, const Snapshot& b) {
    TEST_ASSERT_EQUAL(a.link, b.link);
    TEST_ASSERT_EQUAL(a.checkReady, b.checkReady);
    same(a.kmh, b.kmh, 0.06f); same(a.rpm, b.rpm, 0.06f); same(a.volts, b.volts, 0.006f);
    same(a.coolantC, b.coolantC, 0.06f); same(a.kmlNow, b.kmlNow, 0.06f);
    TEST_ASSERT_EQUAL(a.battery.level, b.battery.level);
    TEST_ASSERT_EQUAL(a.battery.known, b.battery.known);
    TEST_ASSERT_EQUAL_STRING(a.battery.why, b.battery.why);
    TEST_ASSERT_EQUAL(a.engine.level, b.engine.level);
    TEST_ASSERT_EQUAL(a.engine.known, b.engine.known);
    TEST_ASSERT_EQUAL_STRING(a.engine.why, b.engine.why);
    sameSummary(a.trip, b.trip);
    same(a.tripCostRs, b.tripCostRs, 0.006f);
    TEST_ASSERT_EQUAL(a.hasLast, b.hasLast);
    sameSummary(a.last, b.last);
    same(a.lastCostRs, b.lastCostRs, 0.006f);
    same(a.rangeKm, b.rangeKm, 0.06f); same(a.pricePerL, b.pricePerL, 0.006f);
    TEST_ASSERT_EQUAL(a.critical, b.critical);
    TEST_ASSERT_EQUAL_UINT32(a.seq, b.seq);
}

void test_full_round_trip() {
    Snapshot a = full(), b;
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    sameSnap(a, b);
}

void test_nans_round_trip_as_null() {
    Snapshot a;  // all defaults: NAN readings
    a.trip = {NAN, NAN, NAN, NAN, 0, 0};
    a.last = {NAN, NAN, NAN, NAN, 0, 0};
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"kmh\":null"));
    TEST_ASSERT_NULL(strstr(buf, "nan"));
    Snapshot b = full();
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    sameSnap(a, b);
}

void test_infinity_goes_out_as_null() {
    Snapshot a;
    a.volts = INFINITY;
    a.rpm = -INFINITY;
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"volts\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rpm\":null"));
}

void test_link_names() {
    const char* names[] = {"off", "connecting", "noengine", "live"};
    for (int i = 0; i < 4; i++) {
        Snapshot a;
        a.link = static_cast<carwire::Link>(i);
        char buf[carwire::MAX_JSON], want[32];
        carwire::encode(a, buf, sizeof(buf));
        snprintf(want, sizeof(want), "\"link\":\"%s\"", names[i]);
        TEST_ASSERT_NOT_NULL(strstr(buf, want));
        Snapshot b;
        TEST_ASSERT_TRUE(carwire::decode(buf, &b));
        TEST_ASSERT_EQUAL(a.link, b.link);
    }
}

void test_why_with_quotes_and_backslashes_survives() {
    Snapshot a = full();
    strcpy(a.engine.why, "CODE \"P0301\" C:\\X \\\" \"x\":1,");
    a.battery.why[0] = '\t';  // a control character is escaped too
    a.battery.why[1] = 'A';
    a.battery.why[2] = 0;
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    TEST_ASSERT_NULL(strchr(buf, '\t'));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\\u0009"));
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    TEST_ASSERT_EQUAL_STRING(a.engine.why, b.engine.why);
    TEST_ASSERT_EQUAL_STRING(a.battery.why, b.battery.why);
    sameSnap(a, b);
}

void test_encode_reports_no_fit() {
    Snapshot a = full();
    char big[carwire::MAX_JSON];
    size_t len = carwire::encode(a, big, sizeof(big));
    TEST_ASSERT_GREATER_THAN(0, len);
    char small[carwire::MAX_JSON];
    TEST_ASSERT_EQUAL(0, carwire::encode(a, small, len));  // one byte short (no room for the NUL)
    TEST_ASSERT_EQUAL(len, carwire::encode(a, small, len + 1));
    TEST_ASSERT_EQUAL(0, carwire::encode(a, small, 10));
    TEST_ASSERT_EQUAL(0, carwire::encode(a, small, 0));
    TEST_ASSERT_EQUAL(0, carwire::encode(a, nullptr, 100));
}

void test_decode_rejects_junk() {
    Snapshot b = full(), keep = full();
    TEST_ASSERT_FALSE(carwire::decode("", &b));
    TEST_ASSERT_FALSE(carwire::decode("hello", &b));
    TEST_ASSERT_FALSE(carwire::decode("[1,2,3]", &b));
    TEST_ASSERT_FALSE(carwire::decode("{}", &b));
    TEST_ASSERT_FALSE(carwire::decode(nullptr, &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":1}", nullptr));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":1,\"link\":\"warp\"}", &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":1,\"kmh\":\"fast\"}", &b));
    sameSnap(keep, b);  // untouched on failure
}

void test_decode_rejects_wrong_or_missing_version() {
    Snapshot b;
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":2,\"seq\":1}", &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":0,\"seq\":1}", &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"seq\":1,\"link\":\"live\"}", &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":null,\"seq\":1}", &b));
    TEST_ASSERT_FALSE(carwire::decode("{\"v\":\"1\",\"seq\":1}", &b));
}

void test_decode_rejects_every_truncation() {
    Snapshot a = full();
    char buf[carwire::MAX_JSON];
    size_t len = carwire::encode(a, buf, sizeof(buf));
    for (size_t cut = 0; cut < len; cut++) {
        char part[carwire::MAX_JSON];
        memcpy(part, buf, cut);
        part[cut] = 0;
        Snapshot b;
        TEST_ASSERT_FALSE_MESSAGE(carwire::decode(part, &b), part);
    }
}

void test_unknown_keys_ignored_and_order_free() {
    const char* j = "{\"zzz\":[1,2],\"seq\":7,\"extra\":\"a\\\"b\",\"link\":\"live\",\"v\":1,\"kmh\":50.5,\"nope\":null}";
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(j, &b));
    TEST_ASSERT_EQUAL_UINT32(7, b.seq);
    TEST_ASSERT_EQUAL(carwire::Link::Live, b.link);
    TEST_ASSERT_EQUAL_FLOAT(50.5f, b.kmh);
}

void test_missing_optional_keys_keep_defaults() {
    Snapshot b = full();  // old content must not leak in
    TEST_ASSERT_TRUE(carwire::decode("{\"v\":1}", &b));
    Snapshot d;
    sameSnap(d, b);
    TEST_ASSERT_FALSE(b.hasLast);
    TEST_ASSERT_TRUE(std::isnan(b.volts));
}

void test_key_in_a_why_string_is_not_a_key() {
    Snapshot a;
    strcpy(a.engine.why, "\"kmh\":99 \"v\":2");
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    TEST_ASSERT_TRUE(std::isnan(b.kmh));
    TEST_ASSERT_EQUAL_STRING(a.engine.why, b.engine.why);
}

void test_overlong_why_is_cut_safely() {
    const char* j = "{\"v\":1,\"bw\":\"0123456789012345678901234567890123456789012345678901234567890\",\"seq\":3}";
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(j, &b));
    TEST_ASSERT_EQUAL(47, strlen(b.battery.why));
    TEST_ASSERT_EQUAL_UINT32(3, b.seq);
}

void test_worst_case_fits_max_json() {
    Snapshot a = full();
    // longest whys (47 chars) made of characters that escape to two bytes
    for (int i = 0; i < 47; i++) {
        a.battery.why[i] = (i % 2) ? '"' : '\\';
        a.engine.why[i] = (i % 2) ? '\\' : '"';
    }
    a.battery.why[47] = a.engine.why[47] = 0;
    a.kmh = -199.99f; a.rpm = 16383.5f; a.volts = -25.55f; a.coolantC = -140.0f; a.kmlNow = 999.9f;
    a.trip = {9999.99f, 999.999f, 999.9f, 999.9f, 4294967295u, 4294967295u};
    a.last = a.trip;
    a.tripCostRs = a.lastCostRs = -9999.99f;
    a.rangeKm = 9999.9f; a.pricePerL = 99.99f;
    a.seq = 4294967295u;
    a.link = carwire::Link::Connecting;
    char buf[carwire::MAX_JSON];
    size_t len = carwire::encode(a, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_OR_EQUAL(carwire::MAX_JSON - 1, len);
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    TEST_ASSERT_EQUAL_STRING(a.battery.why, b.battery.why);
    TEST_ASSERT_EQUAL_STRING(a.engine.why, b.engine.why);
}

// A finite figure too wide for its field (a sensor glitch, a fuel book gone wild) is null; the rest
// of the reply still goes out. Before, one such value failed the whole reply.
void test_too_wide_finite_value_is_null_not_a_failed_reply() {
    Snapshot a = full();
    a.kmh = 1e30f;
    a.rangeKm = -1000000000.0f;  // "-1000000000.0": 13 chars, one too many
    a.tripCostRs = 3.4e38f;
    char buf[carwire::MAX_JSON];
    TEST_ASSERT_GREATER_THAN(0, carwire::encode(a, buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"kmh\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"range\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"tcost\":null"));
    Snapshot b;
    TEST_ASSERT_TRUE(carwire::decode(buf, &b));
    TEST_ASSERT_TRUE(std::isnan(b.kmh));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.84f, b.volts);
}

// Every real at its widest (REAL_CHARS) still fits MAX_JSON, so no figure can fail the reply.
void test_sim_flag_round_trips_and_defaults_off() {
    carwire::Snapshot a{};
    a.sim = true;
    char json[carwire::MAX_JSON];
    TEST_ASSERT_TRUE(carwire::encode(a, json, sizeof json) > 0);
    carwire::Snapshot b{};
    TEST_ASSERT_TRUE(carwire::decode(json, &b));
    TEST_ASSERT_TRUE(b.sim);
    carwire::Snapshot old{};
    old.sim = true;
    TEST_ASSERT_TRUE(carwire::decode("{\"v\":1,\"seq\":3}", &old));   // a deck without the key
    TEST_ASSERT_FALSE(old.sim);
}

void test_widest_reals_fit_max_json() {
    Snapshot a = full();
    for (int i = 0; i < 47; i++) {
        a.battery.why[i] = (i % 2) ? '"' : '\\';
        a.engine.why[i] = (i % 2) ? '\\' : '"';
    }
    a.battery.why[47] = a.engine.why[47] = 0;
    const float d1 = -100000000.0f, d2 = -10000000.0f, d3 = -1000000.0f;  // 12 chars at 1, 2, 3 decimals
    a.kmh = a.rpm = a.coolantC = a.kmlNow = a.rangeKm = d1;
    a.volts = a.tripCostRs = a.lastCostRs = a.pricePerL = d2;
    a.trip = {d2, d3, d1, d1, 4294967295u, 4294967295u};
    a.last = a.trip;
    a.seq = 4294967295u;
    a.link = carwire::Link::Connecting;
    char buf[carwire::MAX_JSON];
    const size_t len = carwire::encode(a, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"kmh\":-100000000.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"tl\":-1000000.000"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_round_trip);
    RUN_TEST(test_nans_round_trip_as_null);
    RUN_TEST(test_infinity_goes_out_as_null);
    RUN_TEST(test_too_wide_finite_value_is_null_not_a_failed_reply);
    RUN_TEST(test_widest_reals_fit_max_json);
    RUN_TEST(test_sim_flag_round_trips_and_defaults_off);
    RUN_TEST(test_link_names);
    RUN_TEST(test_why_with_quotes_and_backslashes_survives);
    RUN_TEST(test_encode_reports_no_fit);
    RUN_TEST(test_decode_rejects_junk);
    RUN_TEST(test_decode_rejects_wrong_or_missing_version);
    RUN_TEST(test_decode_rejects_every_truncation);
    RUN_TEST(test_unknown_keys_ignored_and_order_free);
    RUN_TEST(test_missing_optional_keys_keep_defaults);
    RUN_TEST(test_key_in_a_why_string_is_not_a_key);
    RUN_TEST(test_overlong_why_is_cut_safely);
    RUN_TEST(test_worst_case_fits_max_json);
    return UNITY_END();
}
