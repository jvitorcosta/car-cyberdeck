#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/black/lyricface.h"
#include "../../src/util/car/cockpit.h"
#include "../../src/util/common/rgb565.h"
#include "../../src/util/common/termpal.h"

using namespace cockpit;

void setUp() {}
void tearDown() {}

static obdwire::Live city() {
    obdwire::Live l;
    l.link = obdplan::Link::Live;
    l.rpm = 2300;
    l.kmh = 42;
    l.coolC = 88;
    l.volts = 13.9f;
    l.throttle = 18;
    l.load = 41;
    l.kml = 11.2f;       // the instant one: not on the screen
    l.tripKmL = 12.8f;   // the trip average: the KM/L row
    l.tripKm = 14.6f;
    l.rangeKm = 318;
    return l;
}

void test_the_values_read_as_text() {
    Scene s;
    scene(city(), false, false, 0, false, &s);
    TEST_ASSERT_TRUE(s.live);
    TEST_ASSERT_EQUAL_STRING(" 42", s.speed);
    TEST_ASSERT_EQUAL_STRING("2300", s.rpm);
    TEST_ASSERT_EQUAL_STRING("COOL", s.card[COOL].label);
    TEST_ASSERT_EQUAL_STRING("88\xB0", s.card[COOL].val);
    TEST_ASSERT_EQUAL_STRING("13.9V", s.card[BATT].val);
    TEST_ASSERT_EQUAL_STRING("12.8", s.card[KMPL].val);   // the trip average, not the instant 11.2
    TEST_ASSERT_EQUAL_STRING("18%", s.card[PEDAL].val);
    TEST_ASSERT_EQUAL_STRING("41%", s.card[LOAD].val);
    TEST_ASSERT_EQUAL_STRING("14.6", s.trip);
    TEST_ASSERT_EQUAL_STRING("318", s.range);
    TEST_ASSERT_EQUAL_INT((int)Status::None, (int)s.status);
}

void test_the_bars_light_and_turn_red_in_their_danger_part() {
    Scene s;
    scene(city(), false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_INT(6, s.card[COOL].lit);   // (88 - 60) / 70 = 0.40 of 14 pills, 5.6 rounds up
    TEST_ASSERT_EQUAL_INT(9, s.card[COOL].hot);   // from 64 % of the track: 105 C
    TEST_ASSERT_EQUAL_INT(12, s.card[LOAD].hot);  // from 85 %
    TEST_ASSERT_EQUAL_INT(12, s.card[PEDAL].hot);
    TEST_ASSERT_EQUAL_INT(PILLS, s.card[BATT].hot);   // never red by position
    TEST_ASSERT_EQUAL_INT(PILLS, s.card[KMPL].hot);
    TEST_ASSERT_EQUAL_INT(6, s.card[LOAD].lit);   // 41 % of 14
    TEST_ASSERT_EQUAL_INT(10, s.card[BATT].lit);  // (13.9 - 11) / 4 = 0.725 of 14
    TEST_ASSERT_EQUAL_INT(7, s.card[KMPL].lit);   // 12.8 / 25 of 14 pills, 7.2
}

void test_a_card_is_red_on_an_alert_only_with_the_link() {
    obdwire::Live l = city();
    Scene s;
    scene(l, true, true, 0, false, &s);
    TEST_ASSERT_TRUE(s.card[COOL].bad);
    TEST_ASSERT_TRUE(s.card[BATT].bad);
    TEST_ASSERT_FALSE(s.card[LOAD].bad);
    l.link = obdplan::Link::Off;
    scene(l, true, true, 0, false, &s);
    TEST_ASSERT_FALSE(s.card[COOL].bad);   // the last alert says nothing about a car we cannot see
    TEST_ASSERT_FALSE(s.card[BATT].bad);
}

void test_without_the_link_the_values_are_dashes_but_the_battery_and_trip_stay() {
    obdwire::Live l = city();
    l.link = obdplan::Link::Off;
    Scene s;
    scene(l, false, false, -1, false, &s);
    TEST_ASSERT_FALSE(s.live);
    TEST_ASSERT_EQUAL_STRING("---", s.speed);
    TEST_ASSERT_EQUAL_STRING("--", s.rpm);
    TEST_ASSERT_EQUAL_STRING("--", s.card[COOL].val);
    TEST_ASSERT_EQUAL_STRING("--", s.card[LOAD].val);
    TEST_ASSERT_EQUAL_INT(0, s.card[COOL].lit);
    TEST_ASSERT_EQUAL_INT(0, s.tachLit);
    TEST_ASSERT_EQUAL_STRING("13.9V", s.card[BATT].val);   // the adapter's own reading
    TEST_ASSERT_EQUAL_STRING("14.6", s.trip);
    TEST_ASSERT_EQUAL_INT((int)Status::NoLink, (int)s.status);
    l.link = obdplan::Link::NoEngine;
    scene(l, false, false, -1, false, &s);
    TEST_ASSERT_EQUAL_INT((int)Status::KeyOn, (int)s.status);
}

void test_a_long_trip_is_capped_and_a_dash_gets_no_unit() {
    obdwire::Live l = city();
    l.tripKm = 1234.5f;
    Scene s;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_STRING("999.9", s.trip);
    l.coolC = 1e12f;   // does not fit "88\xB0" in the card's text: a dash, and no unit after it
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_STRING("--", s.card[COOL].val);
}

void test_the_km_per_l_row_is_the_trip_average_and_stays_without_the_link() {
    obdwire::Live l = city();
    l.link = obdplan::Link::Off;
    Scene s;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_STRING("12.8", s.card[KMPL].val);   // like the trip, it does not need the link
    l.tripKmL = NAN;
    l.kml = 15.0f;   // an instant reading must not stand in for it
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_STRING("--", s.card[KMPL].val);
}

void test_unknown_numbers_are_dashes() {
    obdwire::Live l = city();
    l.tripKmL = NAN;
    l.rangeKm = NAN;
    l.tripKm = NAN;
    l.coolC = NAN;
    Scene s;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_STRING("--", s.card[KMPL].val);
    TEST_ASSERT_EQUAL_STRING("--", s.range);
    TEST_ASSERT_EQUAL_STRING("--", s.trip);
    TEST_ASSERT_EQUAL_STRING("--", s.card[COOL].val);   // no unit after a dash
    TEST_ASSERT_EQUAL_INT(0, s.card[COOL].lit);
}

void test_the_check_engine_pill() {
    Scene s;
    scene(city(), false, false, 2, true, &s);
    TEST_ASSERT_EQUAL_INT((int)Status::Chk, (int)s.status);
    TEST_ASSERT_EQUAL_INT(2, s.codes);
    scene(city(), false, false, 0, true, &s);   // the lamp without a stored code still shows
    TEST_ASSERT_EQUAL_INT((int)Status::Chk, (int)s.status);
    scene(city(), false, false, 120, false, &s);   // the count is capped to two digits
    TEST_ASSERT_EQUAL_INT(99, s.codes);
    scene(city(), false, false, -1, false, &s);   // never read: nothing to say
    TEST_ASSERT_EQUAL_INT((int)Status::None, (int)s.status);
    obdwire::Live l = city();
    l.link = obdplan::Link::Off;
    scene(l, false, false, 2, true, &s);   // no link: NO LINK wins, the lamp is unknown
    TEST_ASSERT_EQUAL_INT((int)Status::NoLink, (int)s.status);
}

void test_the_tach_and_its_zones() {
    TEST_ASSERT_EQUAL_INT(0, tachLit(NAN));
    TEST_ASSERT_EQUAL_INT(0, tachLit(0));
    TEST_ASSERT_EQUAL_INT(1, tachLit(1));        // any rpm lights the first capsule
    TEST_ASSERT_EQUAL_INT(6, tachLit(2300));     // 2300 * 16 / 7000 = 5.3
    TEST_ASSERT_EQUAL_INT(16, tachLit(9000));
    TEST_ASSERT_EQUAL_INT((int)Zone::Norm, (int)tachZone(0));
    TEST_ASSERT_EQUAL_INT((int)Zone::Norm, (int)tachZone(10));   // 4812 rpm at its edge
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)tachZone(11));   // 5250
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)tachZone(13));   // 6125
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)tachZone(14));  // 6562
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)tachZone(15));
    obdwire::Live l = city();
    Scene s;
    l.rpm = 4999;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_INT((int)Zone::Norm, (int)s.rpmZone);
    l.rpm = 5000;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)s.rpmZone);
    l.rpm = 6500;
    scene(l, false, false, 0, false, &s);
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)s.rpmZone);
}

void test_pills_lit_rounds_and_clamps() {
    TEST_ASSERT_EQUAL_INT(0, pillsLit(NAN));
    TEST_ASSERT_EQUAL_INT(0, pillsLit(-1));
    TEST_ASSERT_EQUAL_INT(PILLS, pillsLit(5));
    TEST_ASSERT_EQUAL_INT(7, pillsLit(0.5f));
}

void test_a_key_changes_only_with_what_its_region_draws() {
    Scene a, b;
    scene(city(), false, false, 0, false, &a);
    obdwire::Live l = city();
    l.rpm = 2400;
    scene(l, false, false, 0, false, &b);
    TEST_ASSERT_NOT_EQUAL(key(a, R_RPM), key(b, R_RPM));
    TEST_ASSERT_EQUAL_UINT32(key(a, R_SPEED), key(b, R_SPEED));
    TEST_ASSERT_EQUAL_UINT32(key(a, R_ROW0), key(b, R_ROW0));
    TEST_ASSERT_EQUAL_UINT32(key(a, R_TRIP), key(b, R_TRIP));
    scene(city(), true, false, 0, false, &b);   // the coolant alert: only its card
    TEST_ASSERT_NOT_EQUAL(key(a, (Region)(R_ROW0 + COOL)), key(b, (Region)(R_ROW0 + COOL)));
    TEST_ASSERT_EQUAL_UINT32(key(a, (Region)(R_ROW0 + BATT)), key(b, (Region)(R_ROW0 + BATT)));
    scene(city(), false, false, 2, true, &b);   // the pill is in the speed region
    TEST_ASSERT_NOT_EQUAL(key(a, R_SPEED), key(b, R_SPEED));
    TEST_ASSERT_EQUAL_UINT32(key(a, R_TACH), key(b, R_TACH));
    Scene c;
    scene(city(), false, false, 0, false, &c);
    for (int r = 0; r < REGIONS; ++r) TEST_ASSERT_EQUAL_UINT32(key(a, (Region)r), key(c, (Region)r));   // repeatable
}

void test_the_regions_fit_the_screen_and_do_not_overlap() {
    for (int i = 0; i < REGIONS; ++i) {
        const Rect a = region((Region)i);
        TEST_ASSERT_TRUE(a.w > 0 && a.h > 0);
        TEST_ASSERT_TRUE(a.x >= 0 && a.y >= 0 && a.x + a.w <= W && a.y + a.h <= H);
        for (int j = i + 1; j < REGIONS; ++j) {
            const Rect b = region((Region)j);
            const bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
            TEST_ASSERT_TRUE_MESSAGE(apart, "two regions overlap");
        }
    }
}

void test_the_cards_keep_the_eight_pixel_margin() {
    TEST_ASSERT_EQUAL_INT(8, rowY(0));
    TEST_ASSERT_EQUAL_INT(54, rowY(1));
    TEST_ASSERT_EQUAL_INT(99, rowY(2));
    TEST_ASSERT_EQUAL_INT(145, rowY(3));
    TEST_ASSERT_EQUAL_INT(190, rowY(4));
    TEST_ASSERT_EQUAL_INT(H - MARGIN, rowY(ROWS - 1) + ROW_H);   // the last card ends on the margin
    TEST_ASSERT_EQUAL_INT(W - MARGIN, ROW_X + ROW_W);
    TEST_ASSERT_TRUE(ALERT_FILL_H > 31 && ALERT_FILL_H < TICK_DY);   // the block holds the value (baseline 31), over the ticks
    TEST_ASSERT_TRUE(TICK_DY + 3 <= ROW_H);   // the 3 px ticks stay in the row
    const Rect sp = region(R_SPEED);
    TEST_ASSERT_EQUAL_INT(172, sp.w);
    TEST_ASSERT_TRUE(sp.x + sp.w < ROW_X);   // the speed clears the cards
}

void test_the_palette_is_the_yellow_decks() {
    static Palette p;
    // the yellow deck: termpal::rgb(role, theme, true) on termpal's BG, in every theme and at any hour
    for (int theme = -1; theme < 9; ++theme) {
        makePalette(theme, &p);
        TEST_ASSERT_EQUAL_UINT16(0, p.bg);   // pure black
        TEST_ASSERT_EQUAL_UINT16(rgb565::hex(termpal::rgb(termpal::TEXT, theme, true)), p.tx);
        TEST_ASSERT_EQUAL_UINT16(rgb565::hex(termpal::rgb(termpal::MID, theme, true)), p.md);
        TEST_ASSERT_EQUAL_UINT16(rgb565::hex(termpal::rgb(termpal::DIM, theme, true)), p.dm);
        TEST_ASSERT_EQUAL_UINT16(rgb565::hex(termpal::rgb(termpal::ALERT, theme, true)), p.al);
        TEST_ASSERT_NOT_EQUAL(p.tx, p.ghost);   // unlit parts are visible but quiet
        TEST_ASSERT_NOT_EQUAL(p.bg, p.ghost);
        TEST_ASSERT_NOT_EQUAL(p.bg, p.border);  // the divider shows against the black
        TEST_ASSERT_NOT_EQUAL(p.al, p.tx);
    }
    makePalette(2, &p);   // pink: the yellow deck's pink, not Ride's pinker one
    TEST_ASSERT_EQUAL_UINT16(rgb565::hex(0xFF0040), p.tx);
    makePalette(2, nullptr);   // no crash
}

static int wid(lyricface::Face f, const char* t) {
    int w = 0;
    for (; *t; ++t) w += lyricface::advance(f, (uint8_t)*t);
    return w;
}

// The view places VT323 text by its advances; these are the widest strings each slot can hold.
void test_the_text_fits_its_slots() {
    using lyricface::Face;
    const char* const labels[ROWS] = {"COOL", "BATT", "KM/L", "PEDAL", "LOAD"};
    const char* const widest[ROWS] = {"130\xB0", "16.0V", "99.9", "100%", "100%"};
    for (int i = 0; i < ROWS; ++i) {   // label left, value right, a gap between, inside the card's 7 / 6 px padding
        const int used = wid(Face::LINE, labels[i]) + wid(Face::SUNG, widest[i]) + 6;
        TEST_ASSERT_TRUE_MESSAGE(used <= ROW_W - 13, labels[i]);
    }
    TEST_ASSERT_TRUE(RPM_RIGHT - wid(Face::SUNG, "9999") >= region(R_RPM).x);   // the number stays inside its region
    TEST_ASSERT_TRUE(RPM_LABEL_X + wid(Face::LINE, "RPM") + 2 <= region(R_RPM).x);   // and clear of its label
    TEST_ASSERT_TRUE(KMH_X + wid(Face::LINE, "KM/H") + 4 <= RPM_LABEL_X);
    // trip: value, 4 px, "km" must end before the RANGE block; range the same before the cards
    TEST_ASSERT_TRUE(TRIP_X0 + wid(Face::SUNG, "999.9") + 4 + wid(Face::LINE, "km") <= TRIP_X1);
    TEST_ASSERT_TRUE(TRIP_X1 + wid(Face::SUNG, "9999") + 4 + wid(Face::LINE, "km") <= region(R_TRIP).x + region(R_TRIP).w);
    // the CHK pill (text + 12) stays left of the first cell's right-hand segments, which are lit from 100 km/h
    TEST_ASSERT_TRUE(PILL_X + wid(Face::LINE, "CHK") + 12 <= SPD_X + SPD_CW - SPD_T);
    // NO LINK / KEY ON (text + 16) cover only cells that show "---": their middle bar is below the pill
    TEST_ASSERT_TRUE(PILL_X + wid(Face::LINE, "NO LINK") + 16 <= SPD_X + 3 * SPD_CW + 2 * SPD_GAP);
    TEST_ASSERT_TRUE(PILL_Y + PILL_H < SPD_Y + SPD_CH / 2 - SPD_T / 2);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_values_read_as_text);
    RUN_TEST(test_the_bars_light_and_turn_red_in_their_danger_part);
    RUN_TEST(test_a_card_is_red_on_an_alert_only_with_the_link);
    RUN_TEST(test_without_the_link_the_values_are_dashes_but_the_battery_and_trip_stay);
    RUN_TEST(test_a_long_trip_is_capped_and_a_dash_gets_no_unit);
    RUN_TEST(test_the_km_per_l_row_is_the_trip_average_and_stays_without_the_link);
    RUN_TEST(test_unknown_numbers_are_dashes);
    RUN_TEST(test_the_check_engine_pill);
    RUN_TEST(test_the_tach_and_its_zones);
    RUN_TEST(test_pills_lit_rounds_and_clamps);
    RUN_TEST(test_a_key_changes_only_with_what_its_region_draws);
    RUN_TEST(test_the_regions_fit_the_screen_and_do_not_overlap);
    RUN_TEST(test_the_cards_keep_the_eight_pixel_margin);
    RUN_TEST(test_the_palette_is_the_yellow_decks);
    RUN_TEST(test_the_text_fits_its_slots);
    return UNITY_END();
}
