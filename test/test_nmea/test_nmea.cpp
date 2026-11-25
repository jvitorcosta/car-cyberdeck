#include <unity.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../src/util/car/nmea.h"

void setUp() {}
void tearDown() {}

// "$" + body + "*hh" with the real XOR checksum, so the tests never hard-code a hex pair.
static std::string sentence(const char* body, const char* eol = "\r\n") {
    uint8_t x = 0;
    for (const char* p = body; *p; p++) x ^= (uint8_t)*p;
    char hh[8];
    snprintf(hh, sizeof hh, "*%02X", x);
    return std::string("$") + body + hh + eol;
}

// Feeds a whole string; returns true if any byte completed a sentence (the last one's Fix in *out).
static bool feedAll(nmea::Reader& r, const std::string& s, cameras::Fix* out, uint32_t ms = 1000) {
    bool got = false;
    for (char c : s)
        if (r.feed(c, ms, out)) got = true;
    return got;
}

static const char* GOOD = "GNRMC,123519.00,A,0257.94156,S,06001.27103,W,32.40,270.0,031126,,,A";

void test_valid_rmc() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_TRUE(feedAll(r, sentence(GOOD), &f, 4242));
    TEST_ASSERT_TRUE(f.valid);
    TEST_ASSERT_INT32_WITHIN(1, -2965693, f.latE6);
    TEST_ASSERT_INT32_WITHIN(1, -60021184, f.lonE6);
    TEST_ASSERT_EQUAL_UINT16(60, f.kmh);
    TEST_ASSERT_EQUAL_UINT16(270, f.course);
    TEST_ASSERT_EQUAL_UINT32(4242, f.ms);
}

void test_void_rmc_is_not_valid() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_TRUE(feedAll(r, sentence("GPRMC,123519.00,V,,,,,,,031126,,,N"), &f));
    TEST_ASSERT_FALSE(f.valid);
}

void test_bad_checksum_ignored() {
    nmea::Reader r;
    cameras::Fix f{};
    std::string s = sentence(GOOD);
    s[s.size() - 4] = (s[s.size() - 4] == '0') ? '1' : '0';  // flip a checksum digit
    TEST_ASSERT_FALSE(feedAll(r, s, &f));
}

void test_split_across_feeds() {
    nmea::Reader r;
    cameras::Fix f{};
    std::string s = sentence(GOOD);
    size_t half = s.size() / 2;
    TEST_ASSERT_FALSE(feedAll(r, s.substr(0, half), &f));
    TEST_ASSERT_TRUE(feedAll(r, s.substr(half), &f));
    TEST_ASSERT_EQUAL_UINT16(60, f.kmh);
}

void test_overlong_line_resets() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_FALSE(feedAll(r, "$" + std::string(300, 'A'), &f));
    TEST_ASSERT_TRUE(feedAll(r, sentence(GOOD), &f));
    TEST_ASSERT_EQUAL_UINT16(60, f.kmh);
}

void test_lf_only_and_other_talkers() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_TRUE(feedAll(r, sentence("GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W", "\n"), &f));
    TEST_ASSERT_INT32_WITHIN(1, 48117300, f.latE6);
    TEST_ASSERT_INT32_WITHIN(1, 11516667, f.lonE6);
    TEST_ASSERT_EQUAL_UINT16(41, f.kmh);  // 22.4 kn = 41.48 km/h, rounded down
    TEST_ASSERT_EQUAL_UINT16(84, f.course);
    TEST_ASSERT_TRUE(feedAll(r, sentence("GARMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,,,A", "\n"), &f));
}

void test_no_checksum_ignored() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_FALSE(feedAll(r, std::string("$") + GOOD + "\r\n", &f));
}

void test_other_sentences_ignored() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_FALSE(feedAll(r, sentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"), &f));
}

void test_empty_course_keeps_previous_and_empty_speed_is_zero() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_TRUE(feedAll(r, sentence(GOOD), &f));
    TEST_ASSERT_TRUE(feedAll(r, sentence("GNRMC,123520.00,A,0257.94156,S,06001.27103,W,,,031126,,,A"), &f));
    TEST_ASSERT_TRUE(f.valid);
    TEST_ASSERT_EQUAL_UINT16(0, f.kmh);
    TEST_ASSERT_EQUAL_UINT16(270, f.course);
}

void test_malformed_position_ignored() {
    nmea::Reader r;
    cameras::Fix f{};
    TEST_ASSERT_FALSE(feedAll(r, sentence("GNRMC,123519.00,A,,S,06001.27103,W,32.40,270.0,031126,,,A"), &f));
    TEST_ASSERT_FALSE(feedAll(r, sentence("GNRMC,123519.00,A,0257.94156,X,06001.27103,W,32.40,270.0,031126,,,A"), &f));
}

// One sentence with the lat / lon / speed / course fields swapped in; true when it came out as a Fix.
static bool rmc(const char* lat, const char* ns, const char* lon, const char* ew, const char* kn,
                const char* crs, cameras::Fix* f) {
    char body[160];
    snprintf(body, sizeof body, "GNRMC,123519.00,A,%s,%s,%s,%s,%s,%s,031126,,,A", lat, ns, lon, ew, kn, crs);
    nmea::Reader r;
    return feedAll(r, sentence(body), f);
}

// CONTRIBUTING: a parser of outside data gets an absurd number. The reader drops the whole sentence for
// a number past its cap (never a wrapped position), and keeps 90 / 180 degrees and 59.999 minutes as limits.
void test_absurd_numbers_drop_the_sentence() {
    cameras::Fix f{};
    TEST_ASSERT_TRUE(rmc("0257.94156", "S", "06001.27103", "W", "32.40", "270.0", &f));   // control
    TEST_ASSERT_FALSE(rmc("99999999999999.0", "N", "06001.27103", "W", "32.40", "270.0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "99999999999999.0", "W", "32.40", "270.0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "06001.27103", "W", "99999999999999.0", "270.0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "06001.27103", "W", "32.40", "99999999999999.0", &f));
    // 2^32-ish values that would wrap a 32-bit multiply if the cap were missing
    TEST_ASSERT_FALSE(rmc("4294967296.0", "N", "06001.27103", "W", "32.40", "270.0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "06001.27103", "W", "4294967296", "270.0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "06001.27103", "W", "32.40", "4294967296", &f));
}

void test_speed_over_the_uint16_range_clamps() {
    cameras::Fix f{};
    TEST_ASSERT_TRUE(rmc("0257.94156", "S", "06001.27103", "W", "99999999.0", "270.0", &f));
    TEST_ASSERT_EQUAL_UINT16(65535, f.kmh);
}

void test_course_wraps_to_a_compass_bearing() {
    cameras::Fix f{};
    TEST_ASSERT_TRUE(rmc("0257.94156", "S", "06001.27103", "W", "32.40", "360.0", &f));
    TEST_ASSERT_EQUAL_UINT16(0, f.course);
    TEST_ASSERT_TRUE(rmc("0257.94156", "S", "06001.27103", "W", "32.40", "725.4", &f));
    TEST_ASSERT_EQUAL_UINT16(5, f.course);
}

void test_latitude_and_longitude_limits() {
    cameras::Fix f{};
    TEST_ASSERT_TRUE(rmc("9000.0000", "N", "18000.0000", "E", "0", "0", &f));    // the edges are valid
    TEST_ASSERT_EQUAL_INT32(90000000, f.latE6);
    TEST_ASSERT_EQUAL_INT32(180000000, f.lonE6);
    TEST_ASSERT_TRUE(rmc("9000.0000", "S", "18000.0000", "W", "0", "0", &f));
    TEST_ASSERT_EQUAL_INT32(-90000000, f.latE6);
    TEST_ASSERT_FALSE(rmc("9100.0000", "N", "06001.27103", "W", "0", "0", &f));    // 91 degrees
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "18100.0000", "E", "0", "0", &f));    // 181 degrees
    TEST_ASSERT_FALSE(rmc("0260.0000", "N", "06001.27103", "W", "0", "0", &f));    // 60 minutes
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "06060.0000", "W", "0", "0", &f));
    TEST_ASSERT_FALSE(rmc("-257.94156", "S", "06001.27103", "W", "0", "0", &f));   // a sign is not NMEA
}

// Finding (F-21 follow-up): parseCoord checks `deg > maxDeg`, not the finished value, so 90 degrees
// and a little over (9000.0001 N, 18000.0001 E) come out past the pole / the date line. Harmless to
// the matcher but outside the range the rule promises. Flip to true once parseCoord checks the sum.
static const bool COORD_EDGE_FIXED = true;

void test_a_position_just_past_the_pole_or_date_line_is_refused() {
    cameras::Fix f{};
    if (!COORD_EDGE_FIXED) TEST_IGNORE_MESSAGE("finding: 9000.0001 N / 18000.0001 E are accepted past the limit");
    TEST_ASSERT_FALSE(rmc("9000.0001", "N", "06001.27103", "W", "0", "0", &f));
    TEST_ASSERT_FALSE(rmc("0257.94156", "S", "18000.0001", "E", "0", "0", &f));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_rmc);
    RUN_TEST(test_void_rmc_is_not_valid);
    RUN_TEST(test_bad_checksum_ignored);
    RUN_TEST(test_split_across_feeds);
    RUN_TEST(test_overlong_line_resets);
    RUN_TEST(test_lf_only_and_other_talkers);
    RUN_TEST(test_no_checksum_ignored);
    RUN_TEST(test_other_sentences_ignored);
    RUN_TEST(test_empty_course_keeps_previous_and_empty_speed_is_zero);
    RUN_TEST(test_malformed_position_ignored);
    RUN_TEST(test_absurd_numbers_drop_the_sentence);
    RUN_TEST(test_speed_over_the_uint16_range_clamps);
    RUN_TEST(test_course_wraps_to_a_compass_bearing);
    RUN_TEST(test_latitude_and_longitude_limits);
    RUN_TEST(test_a_position_just_past_the_pole_or_date_line_is_refused);
    return UNITY_END();
}
