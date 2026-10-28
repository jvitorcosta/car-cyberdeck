#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/healthview.h"

using carhealth::Level;
using carhealth::Verdict;
using healthview::Role;

void setUp() {}
void tearDown() {}

static Verdict v(Level l, const char* why = "") {
    Verdict r;
    r.level = l;
    std::strncpy(r.why, why, sizeof r.why - 1);
    return r;
}

void test_check_all_ok_lines() {
    tripsum::Summary s{18.4f, 1.52f, 12.1f, 34.0f, 1920, 360};
    auto c = healthview::check(v(Level::Ok), 12.6f, v(Level::Ok), &s, 9.4f, 310.0f);
    TEST_ASSERT_EQUAL_STRING("BATTERY  12.6V  OK", c.battery.text);
    TEST_ASSERT_EQUAL(Role::Text, c.battery.role);
    TEST_ASSERT_EQUAL_STRING("ENGINE  OK", c.engine.text);
    TEST_ASSERT_EQUAL_STRING("LAST TRIP 18.4 KM  12.1 KM/L  R$ 9,40", c.lastTrip.text);
    TEST_ASSERT_EQUAL_STRING("RANGE ~310 KM", c.range.text);
    TEST_ASSERT_EQUAL_STRING(">ALL SYSTEMS NOMINAL", c.status.text);
    TEST_ASSERT_EQUAL(Role::Text, c.status.role);
    TEST_ASSERT_FALSE(c.problem);
    auto charging = healthview::check(v(Level::Ok), 14.1f, v(Level::Ok), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("CHARGING 14.1V", charging.batteryWhy.text);
    TEST_ASSERT_EQUAL_STRING("LAST TRIP -- KM  -- KM/L  R$ --", charging.lastTrip.text);
    TEST_ASSERT_EQUAL_STRING("RANGE -- KM", charging.range.text);
}

void test_check_problem_marks_only_that_item() {
    auto c = healthview::check(v(Level::Problem, "UNDER 12V FOR A MINUTE"), 11.9f, v(Level::Ok), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("BATTERY  11.9V  LOW", c.battery.text);
    TEST_ASSERT_EQUAL(Role::Alert, c.battery.role);
    TEST_ASSERT_EQUAL_STRING("UNDER 12V FOR A MINUTE", c.batteryWhy.text);
    TEST_ASSERT_EQUAL(Role::Alert, c.batteryWhy.role);
    TEST_ASSERT_EQUAL(Role::Text, c.engine.role);
    TEST_ASSERT_EQUAL_STRING(">1 ISSUE  [TAP FOR DETAILS]", c.status.text);
    TEST_ASSERT_EQUAL(Role::Alert, c.status.role);
    TEST_ASSERT_TRUE(c.problem);
    auto two = healthview::check(v(Level::Problem, "X"), 11.0f, v(Level::Problem, "Y"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING(">2 ISSUES  [TAP FOR DETAILS]", two.status.text);
    TEST_ASSERT_EQUAL_STRING("ENGINE  CHECK", two.engine.text);
}

void test_check_note_is_not_a_problem() {
    auto c = healthview::check(v(Level::Ok), 13.9f, v(Level::Note, "FUEL TRIM HIGH"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("ENGINE  NOTE", c.engine.text);
    TEST_ASSERT_EQUAL(Role::Mid, c.engine.role);
    TEST_ASSERT_EQUAL_STRING(">1 NOTE  [TAP FOR DETAILS]", c.status.text);
    TEST_ASSERT_EQUAL(Role::Mid, c.status.role);
    TEST_ASSERT_FALSE(c.problem);
}

void test_band_dashes_for_unknown() {
    auto b = healthview::band(12.4f, 11.8f, 296.0f, 16 * 60 + 5, 6.3f, 14.1f, 90.0f);
    TEST_ASSERT_EQUAL_STRING("12.4 KM", b.trip);
    TEST_ASSERT_EQUAL_STRING("11.8 KM/L", b.kmlNow);
    TEST_ASSERT_EQUAL_STRING("296 KM LEFT", b.range);
    TEST_ASSERT_EQUAL_STRING("0:16  R$ 6,30  14.1V  90C", b.small);
    auto u = healthview::band(NAN, NAN, NAN, 3700, NAN, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("-- KM", u.trip);
    TEST_ASSERT_EQUAL_STRING("-- KM/L", u.kmlNow);
    TEST_ASSERT_EQUAL_STRING("-- KM LEFT", u.range);
    TEST_ASSERT_EQUAL_STRING("1:01  R$ --  --V  --C", u.small);
}

void test_cost_uses_comma() {
    tripsum::Summary s{18.4f, 1.52f, 12.1f, 34.0f, 1920, 360};
    auto p = healthview::parked(s, 9.4f, 290.0f, v(Level::Ok), v(Level::Ok));
    TEST_ASSERT_EQUAL_STRING("R$ 9,40", p.cost.text);
    TEST_ASSERT_EQUAL_STRING("18.4 KM", p.km.text);
    TEST_ASSERT_EQUAL_STRING("32 MIN", p.minutes.text);
    TEST_ASSERT_EQUAL_STRING("12.1 KM/L", p.kml.text);
    TEST_ASSERT_EQUAL_STRING("AVG 34 KM/H  IDLE 6 MIN  RANGE ~290 KM", p.small.text);
    TEST_ASSERT_EQUAL_STRING("BATTERY OK", p.battery.text);
    TEST_ASSERT_EQUAL_STRING(">SAFE TO SHUT DOWN", p.footer.text);
    auto q = healthview::parked(s, NAN, NAN, v(Level::Problem, "X"), v(Level::Note, "Y"));
    TEST_ASSERT_EQUAL_STRING("R$ --", q.cost.text);
    TEST_ASSERT_EQUAL_STRING("BATTERY LOW", q.battery.text);
    TEST_ASSERT_EQUAL(Role::Alert, q.battery.role);
    TEST_ASSERT_EQUAL_STRING("ENGINE NOTE", q.engine.text);
    TEST_ASSERT_EQUAL(Role::Mid, q.engine.role);
}

void test_parkgate_needs_five_seconds_and_half_km() {
    healthview::ParkGate g;
    TEST_ASSERT_FALSE(g.update(0.0f, 2.0f, 1000));
    TEST_ASSERT_FALSE(g.update(0.0f, 2.0f, 5999));
    TEST_ASSERT_TRUE(g.update(0.0f, 2.0f, 6000));
    healthview::ParkGate h;  // short trip never shows
    TEST_ASSERT_FALSE(h.update(0.0f, 0.4f, 0));
    TEST_ASSERT_FALSE(h.update(0.0f, 0.4f, 9000));
    healthview::ParkGate n;  // unknown speed breaks the 5 s
    TEST_ASSERT_FALSE(n.update(0.0f, 2.0f, 0));
    TEST_ASSERT_FALSE(n.update(NAN, 2.0f, 3000));
    TEST_ASSERT_FALSE(n.update(0.0f, 2.0f, 6000));
    TEST_ASSERT_TRUE(n.update(0.0f, 2.0f, 11000));
    healthview::ParkGate m;  // 1 km/h or more is not stopped
    TEST_ASSERT_FALSE(m.update(0.0f, 2.0f, 0));
    TEST_ASSERT_FALSE(m.update(1.5f, 2.0f, 4000));
    TEST_ASSERT_FALSE(m.update(0.0f, 2.0f, 6000));
}

void test_parkgate_hides_on_move_and_dismiss_holds_until_moving() {
    healthview::ParkGate g;
    g.update(0.0f, 2.0f, 0);
    TEST_ASSERT_TRUE(g.update(0.0f, 2.0f, 5000));
    TEST_ASSERT_TRUE(g.update(2.0f, 2.0f, 5500));  // creeping keeps it
    TEST_ASSERT_FALSE(g.update(4.0f, 2.0f, 6000));
    g.update(0.0f, 2.0f, 7000);
    TEST_ASSERT_TRUE(g.update(0.0f, 2.0f, 12000));
    g.dismiss();
    TEST_ASSERT_FALSE(g.update(0.0f, 2.0f, 12100));
    TEST_ASSERT_FALSE(g.update(0.0f, 2.0f, 60000));
    TEST_ASSERT_FALSE(g.update(2.5f, 2.0f, 61000));  // not enough movement
    TEST_ASSERT_FALSE(g.update(0.0f, 2.0f, 70000));
    TEST_ASSERT_FALSE(g.update(5.0f, 2.0f, 71000));
    g.update(0.0f, 2.0f, 72000);
    TEST_ASSERT_TRUE(g.update(0.0f, 2.0f, 77000));
}

void test_lines_fit_forty_chars() {
    Verdict w = v(Level::Problem, "012345678901234567890123456789012345678901234");
    tripsum::Summary s{99999.0f, 1e9f, 99999.0f, 9999.0f, 4000000000U, 4000000000U};
    auto c = healthview::check(w, 99999.0f, w, &s, 1e9f, 1e9f);
    const healthview::Line* ls[] = {&c.battery, &c.batteryWhy, &c.engine, &c.engineWhy, &c.lastTrip, &c.range, &c.status};
    for (auto* l : ls) TEST_ASSERT_LESS_THAN(40, std::strlen(l->text));
    auto p = healthview::parked(s, 1e9f, 1e9f, w, w);
    const healthview::Line* ps[] = {&p.km, &p.minutes, &p.kml, &p.cost, &p.small, &p.battery, &p.engine, &p.footer};
    for (auto* l : ps) TEST_ASSERT_LESS_THAN(40, std::strlen(l->text));
    auto b = healthview::band(1e9f, 1e9f, 1e9f, 4000000000U, 1e9f, 1e9f, 1e9f);
    TEST_ASSERT_LESS_THAN(12, std::strlen(b.trip));
    TEST_ASSERT_LESS_THAN(12, std::strlen(b.kmlNow));
    TEST_ASSERT_LESS_THAN(16, std::strlen(b.range));
    TEST_ASSERT_LESS_THAN(40, std::strlen(b.small));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_check_all_ok_lines);
    RUN_TEST(test_check_problem_marks_only_that_item);
    RUN_TEST(test_check_note_is_not_a_problem);
    RUN_TEST(test_band_dashes_for_unknown);
    RUN_TEST(test_cost_uses_comma);
    RUN_TEST(test_parkgate_needs_five_seconds_and_half_km);
    RUN_TEST(test_parkgate_hides_on_move_and_dismiss_holds_until_moving);
    RUN_TEST(test_lines_fit_forty_chars);
    return UNITY_END();
}
