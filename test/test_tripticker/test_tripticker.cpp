#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/tripticker.h"

using tripticker::Cell;

void setUp() {}
void tearDown() {}

static obdwire::Live live() {
    obdwire::Live l;
    l.link = obdplan::Link::Live;
    l.kmh = 62.0f;
    l.kml = 14.26f;
    l.tripKm = 23.44f;
    l.tripKmL = 13.06f;
    l.tripS = 1860;
    l.rangeKm = 412.4f;
    l.priceL = 6.49f;
    l.tripRs = 11.62f;
    return l;
}

static const char* cell(Cell c, const obdwire::Live& l) {
    static char out[tripticker::TEXT];
    memset(out, 'x', sizeof out);
    tripticker::text(c, l, out);
    return out;
}

void test_a_live_trip() {
    const obdwire::Live l = live();
    TEST_ASSERT_EQUAL_STRING("14.3", cell(Cell::KmlNow, l));
    TEST_ASSERT_EQUAL_STRING("13.1", cell(Cell::TripKml, l));
    TEST_ASSERT_EQUAL_STRING("23.4", cell(Cell::TripKm, l));
    TEST_ASSERT_EQUAL_STRING("412", cell(Cell::Range, l));
    TEST_ASSERT_EQUAL_STRING("0:31", cell(Cell::TripTime, l));
    TEST_ASSERT_EQUAL_STRING("11.6", cell(Cell::TripRs, l));
}

void test_unknown_is_dashes() {
    obdwire::Live l;   // link off, nothing known, no fill-up
    for (int c = 0; c < (int)Cell::COUNT; ++c) {
        if ((Cell)c == Cell::TripTime) continue;
        TEST_ASSERT_EQUAL_STRING_MESSAGE("--", cell((Cell)c, l), tripticker::label((Cell)c));
    }
    TEST_ASSERT_EQUAL_STRING("0:00", cell(Cell::TripTime, l));
    l = live();
    l.link = obdplan::Link::NoEngine;   // km/l now needs the live link; the trip's figures stay
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::KmlNow, l));
    TEST_ASSERT_EQUAL_STRING("13.1", cell(Cell::TripKml, l));
    l = live();
    l.rangeKm = l.tripRs = l.priceL = NAN;   // no full tank with a price yet
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::Range, l));
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::TripRs, l));
}

void test_coasting() {
    obdwire::Live l = live();
    l.kml = 99.9f;
    TEST_ASSERT_EQUAL_STRING("COAST", cell(Cell::KmlNow, l));
    l.kml = tripticker::COAST_KML;
    TEST_ASSERT_EQUAL_STRING("50.0", cell(Cell::KmlNow, l));
    l.kml = NAN;   // stopped (under 3 km/h): no km/l now
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::KmlNow, l));
}

void test_values_fit_five_characters() {
    obdwire::Live l = live();
    l.tripKm = 99.94f;
    TEST_ASSERT_EQUAL_STRING("99.9", cell(Cell::TripKm, l));
    l.tripKm = 99.96f;   // rounds up past 100: whole km from there
    TEST_ASSERT_EQUAL_STRING("100", cell(Cell::TripKm, l));
    l.tripKm = 123456.0f;
    TEST_ASSERT_EQUAL_STRING("99999", cell(Cell::TripKm, l));
    l.tripRs = 99.97f;
    TEST_ASSERT_EQUAL_STRING("100", cell(Cell::TripRs, l));
    l.tripRs = 2e6f;
    TEST_ASSERT_EQUAL_STRING("99999", cell(Cell::TripRs, l));
    l.rangeKm = 0.0f;
    TEST_ASSERT_EQUAL_STRING("0", cell(Cell::Range, l));
    l.rangeKm = 1e6f;
    TEST_ASSERT_EQUAL_STRING("9999", cell(Cell::Range, l));
    l.tripS = 9 * 3600 + 59 * 60 + 59;
    TEST_ASSERT_EQUAL_STRING("9:59", cell(Cell::TripTime, l));
    l.tripS = 12 * 3600 + 5 * 60;
    TEST_ASSERT_EQUAL_STRING("12:05", cell(Cell::TripTime, l));
    l.tripS = 4294967295u;   // capped, never wider
    TEST_ASSERT_EQUAL_STRING("99:59", cell(Cell::TripTime, l));
    l.tripKmL = -3.0f;       // nothing real is negative
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::TripKml, l));
    l.tripKmL = -0.0f;       // a computed -0 is 0, never "-0.0"
    TEST_ASSERT_EQUAL_STRING("0.0", cell(Cell::TripKml, l));
    l.tripRs = -0.0f;
    TEST_ASSERT_EQUAL_STRING("0.0", cell(Cell::TripRs, l));
}

void test_low_range_is_red() {
    obdwire::Live l = live();
    TEST_ASSERT_FALSE(tripticker::alert(Cell::Range, l));
    l.rangeKm = 59.0f;
    TEST_ASSERT_TRUE(tripticker::alert(Cell::Range, l));
    TEST_ASSERT_FALSE(tripticker::alert(Cell::TripKm, l));
    l.rangeKm = NAN;   // no fill-up noted: no alarm
    TEST_ASSERT_FALSE(tripticker::alert(Cell::Range, l));
    l.rangeKm = -5.0f;   // shown as "--": no red either
    TEST_ASSERT_FALSE(tripticker::alert(Cell::Range, l));
    TEST_ASSERT_EQUAL_STRING("--", cell(Cell::Range, l));
}

void test_header_and_status() {
    char h[32];
    obdwire::Live l = live();
    tripticker::header(l, h, sizeof h);
    TEST_ASSERT_EQUAL_STRING("> TRIP \xB7 31 MIN", h);
    l.tripS = 59;
    tripticker::header(l, h, sizeof h);
    TEST_ASSERT_EQUAL_STRING("> TRIP \xB7 00 MIN", h);
    l.tripS = 4294967295u;
    tripticker::header(l, h, sizeof h);
    TEST_ASSERT_EQUAL_STRING("> TRIP \xB7 999 MIN", h);
    TEST_ASSERT_EQUAL_STRING("OK", tripticker::status(obdplan::Link::Live));
    TEST_ASSERT_EQUAL_STRING("KEY ON", tripticker::status(obdplan::Link::NoEngine));
    TEST_ASSERT_EQUAL_STRING("TRIP KM/L", tripticker::label(Cell::TripKml));
    TEST_ASSERT_EQUAL_STRING("", tripticker::label(Cell::COUNT));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_live_trip);
    RUN_TEST(test_unknown_is_dashes);
    RUN_TEST(test_coasting);
    RUN_TEST(test_values_fit_five_characters);
    RUN_TEST(test_low_range_is_red);
    RUN_TEST(test_header_and_status);
    return UNITY_END();
}
