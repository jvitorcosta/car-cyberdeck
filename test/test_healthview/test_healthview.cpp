#include <unity.h>
#include <cmath>
#include <initializer_list>
#include <cstdio>
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
    r.known = true;
    std::strncpy(r.why, why, sizeof r.why - 1);
    return r;
}

static Verdict unknown() { return Verdict{}; }  // nothing read yet

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
    TEST_ASSERT_EQUAL_STRING(">1 ISSUE  [TAP TO CLOSE]", c.status.text);
    TEST_ASSERT_EQUAL(Role::Alert, c.status.role);
    TEST_ASSERT_TRUE(c.problem);
    auto two = healthview::check(v(Level::Problem, "X"), 11.0f, v(Level::Problem, "Y"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING(">2 ISSUES  [TAP TO CLOSE]", two.status.text);
    TEST_ASSERT_EQUAL_STRING("ENGINE  CHECK", two.engine.text);
}

void test_check_note_is_not_a_problem() {
    auto c = healthview::check(v(Level::Ok), 13.9f, v(Level::Note, "FUEL TRIM HIGH"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("ENGINE  NOTE", c.engine.text);
    TEST_ASSERT_EQUAL(Role::Mid, c.engine.role);
    TEST_ASSERT_EQUAL_STRING(">1 NOTE", c.status.text);
    TEST_ASSERT_EQUAL(Role::Mid, c.status.role);
    TEST_ASSERT_FALSE(c.problem);
    // Notes leave by themselves after 5 s: no tap hint.
    auto two = healthview::check(v(Level::Note, "WAS LOW"), 12.4f, v(Level::Note, "FUEL TRIM HIGH"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING(">2 NOTES", two.status.text);
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
    TEST_ASSERT_EQUAL_STRING("AVG 34 KM/H  IDLE 6 MIN  290 KM LEFT", p.small.text);
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

// Nothing read yet is not "OK": the screen says it is still checking, and never claims all is well.
void test_check_unknown_items_say_checking() {
    auto c = healthview::check(unknown(), NAN, unknown(), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("BATTERY  CHECKING", c.battery.text);
    TEST_ASSERT_EQUAL(Role::Dim, c.battery.role);
    TEST_ASSERT_EQUAL_STRING("", c.batteryWhy.text);
    TEST_ASSERT_EQUAL_STRING("ENGINE  CHECKING", c.engine.text);
    TEST_ASSERT_EQUAL(Role::Dim, c.engine.role);
    TEST_ASSERT_EQUAL_STRING("", c.engineWhy.text);
    TEST_ASSERT_EQUAL_STRING(">CHECKING...", c.status.text);
    TEST_ASSERT_EQUAL(Role::Dim, c.status.role);
    TEST_ASSERT_FALSE(c.problem);
}

void test_check_one_known_ok_is_still_checking() {
    auto c = healthview::check(v(Level::Ok), 13.9f, unknown(), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING("BATTERY  13.9V  OK", c.battery.text);
    TEST_ASSERT_EQUAL_STRING("CHARGING 13.9V", c.batteryWhy.text);
    TEST_ASSERT_EQUAL_STRING("ENGINE  CHECKING", c.engine.text);
    TEST_ASSERT_EQUAL_STRING(">CHECKING...", c.status.text);
    TEST_ASSERT_EQUAL(Role::Dim, c.status.role);
    TEST_ASSERT_FALSE(c.problem);
    auto n = healthview::check(unknown(), NAN, v(Level::Note, "1 PENDING CODE, NOT CONFIRMED"), nullptr, NAN, NAN);
    TEST_ASSERT_EQUAL_STRING(">1 NOTE", n.status.text);
    TEST_ASSERT_EQUAL_STRING("BATTERY  CHECKING", n.battery.text);
}

void test_parked_unknown_items_are_dashes() {
    tripsum::Summary s{18.4f, 1.52f, 12.1f, 34.0f, 1920, 360};
    auto p = healthview::parked(s, 9.4f, 290.0f, unknown(), unknown());
    TEST_ASSERT_EQUAL_STRING("BATTERY --", p.battery.text);
    TEST_ASSERT_EQUAL(Role::Dim, p.battery.role);
    TEST_ASSERT_EQUAL_STRING("ENGINE --", p.engine.text);
    TEST_ASSERT_EQUAL(Role::Dim, p.engine.role);
}

// Worst realistic inputs; exact strings, so a cut-off tail fails (setLine would silently truncate).
void test_lines_fit_forty_chars() {
    Verdict w = v(Level::Problem, "99 PENDING CODES, NOT CONFIRMED");
    tripsum::Summary s{999.9f, 80.0f, 99.9f, 199.0f, 999U * 60U, 999U * 60U};
    auto c = healthview::check(w, 15.9f, w, &s, 999.99f, 999.0f);
    TEST_ASSERT_EQUAL_STRING("BATTERY  15.9V  LOW", c.battery.text);
    TEST_ASSERT_EQUAL_STRING("99 PENDING CODES, NOT CONFIRMED", c.batteryWhy.text);
    TEST_ASSERT_EQUAL_STRING("ENGINE  CHECK", c.engine.text);
    TEST_ASSERT_EQUAL_STRING(">2 ISSUES  [TAP TO CLOSE]", c.status.text);
    const Verdict note = v(Level::Note, "BATTERY WAS LOW LAST TRIP");   // A's rows: no advice over them
    auto n = healthview::check(note, 15.9f, note, &s, 999.99f, 999.0f);
    TEST_ASSERT_EQUAL_STRING("LAST TRIP 1000 KM  99.9 KM/L  R$ 999,99", n.lastTrip.text);
    TEST_ASSERT_EQUAL_STRING("RANGE ~999 KM", n.range.text);
    auto p = healthview::parked(s, 999.99f, 999.0f, w, w);
    TEST_ASSERT_EQUAL_STRING("1000 KM", p.km.text);
    TEST_ASSERT_EQUAL_STRING("999 MIN", p.minutes.text);
    TEST_ASSERT_EQUAL_STRING("99.9 KM/L", p.kml.text);
    TEST_ASSERT_EQUAL_STRING("R$ 999,99", p.cost.text);
    TEST_ASSERT_EQUAL_STRING("AVG 199 KM/H  IDLE 999 MIN  999 KM LEFT", p.small.text);
    TEST_ASSERT_EQUAL_UINT(39, std::strlen(p.small.text));
    auto b = healthview::band(999.9f, 99.9f, 999.0f, 99U * 3600U + 59U * 60U, 999.99f, 15.9f, 130.0f);
    TEST_ASSERT_EQUAL_STRING("1000 KM", b.trip);
    TEST_ASSERT_EQUAL_STRING("99.9 KM/L", b.kmlNow);
    TEST_ASSERT_EQUAL_STRING("999 KM LEFT", b.range);
    TEST_ASSERT_EQUAL_STRING("99:59  R$ 999,99  15.9V  130C", b.small);
    auto q = healthview::parked(s, NAN, NAN, w, w);
    TEST_ASSERT_EQUAL_STRING("AVG 199 KM/H  IDLE 999 MIN  -- KM LEFT", q.small.text);
}

// Screen C's live warning: critical (engine hot, MIL) while moving; any problem once stopped; notes never.
static const char* warn(healthview::Warnings& w, const Verdict& b, const Verdict& e, bool critical, float kmh, uint32_t now) {
    static char out[40];
    out[0] = 'x';
    out[1] = 0;
    return w.next(b, e, critical, kmh, now, out, sizeof out) ? out : nullptr;
}

void test_warning_critical_shows_while_moving() {
    healthview::Warnings w;
    const char* m = warn(w, v(Level::Ok), v(Level::Problem, "ENGINE HOT 108C"), true, 60.0f, 1000);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING(">ENGINE HOT 108C", m);
    TEST_ASSERT_NOT_NULL(warn(w, v(Level::Ok), v(Level::Problem, "CHECK ENGINE LIGHT ON"), true, NAN, 1000));  // speed unknown
}

void test_warning_problem_waits_for_a_stop() {
    healthview::Warnings w;
    const Verdict low = v(Level::Problem, "BATTERY LOW WHILE RUNNING");
    TEST_ASSERT_NULL(warn(w, low, v(Level::Ok), false, 40.0f, 1000));
    TEST_ASSERT_NULL(warn(w, low, v(Level::Ok), false, 1.0f, 2000));  // 1 km/h is still moving
    TEST_ASSERT_NULL(warn(w, low, v(Level::Ok), false, NAN, 3000));   // an unknown speed isn't a stop
    const char* m = warn(w, low, v(Level::Ok), false, 0.5f, 4000);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING(">BATTERY LOW WHILE RUNNING", m);
}

void test_warning_notes_never_show() {
    healthview::Warnings w;
    TEST_ASSERT_NULL(warn(w, v(Level::Note, "BATTERY WAS LOW LAST TRIP"), v(Level::Note, "1 PENDING CODE, NOT CONFIRMED"),
                          false, 0.0f, 1000));
    TEST_ASSERT_NULL(warn(w, v(Level::Ok), v(Level::Ok), false, 0.0f, 1000));
}

void test_warning_each_at_most_every_two_minutes() {
    healthview::Warnings w;
    const Verdict low = v(Level::Problem, "BATTERY LOW WHILE RUNNING"), hot = v(Level::Problem, "ENGINE HOT 108C");
    TEST_ASSERT_EQUAL_STRING(">ENGINE HOT 108C", warn(w, low, hot, true, 0.0f, 1000));  // the engine first
    TEST_ASSERT_EQUAL_STRING(">BATTERY LOW WHILE RUNNING", warn(w, low, hot, true, 0.0f, 3000));
    TEST_ASSERT_NULL(warn(w, low, hot, true, 0.0f, 5000));
    TEST_ASSERT_NULL(warn(w, low, hot, true, 0.0f, 1000 + healthview::WARN_EVERY_MS - 1));
    TEST_ASSERT_EQUAL_STRING(">ENGINE HOT 108C", warn(w, low, hot, true, 0.0f, 1000 + healthview::WARN_EVERY_MS));
    TEST_ASSERT_EQUAL_STRING(">BATTERY LOW WHILE RUNNING", warn(w, low, hot, true, 0.0f, 3000 + healthview::WARN_EVERY_MS));
}

// The kind counts, not the figures in it: a climbing temperature is one warning (its current text when
// it is due again); another kind shows at once.
void test_warning_kind_ignores_the_figures() {
    healthview::Warnings w;
    const Verdict ok = v(Level::Ok);
    TEST_ASSERT_EQUAL_STRING(">ENGINE HOT 108C", warn(w, ok, v(Level::Problem, "ENGINE HOT 108C"), true, 60.0f, 1000));
    for (int c = 109; c <= 115; c++) {  // more values than the memory has slots
        char why[24];
        std::snprintf(why, sizeof why, "ENGINE HOT %dC", c);
        TEST_ASSERT_NULL_MESSAGE(warn(w, ok, v(Level::Problem, why), true, 60.0f, 1000 + 10000 * (c - 108)), why);
    }
    TEST_ASSERT_EQUAL_STRING(">ENGINE HOT 110C",
                             warn(w, ok, v(Level::Problem, "ENGINE HOT 110C"), true, 60.0f, 1000 + healthview::WARN_EVERY_MS));
    // another kind, at once: the MIL while the hot engine is still inside its 2 minutes
    healthview::Warnings m;
    TEST_ASSERT_NOT_NULL(warn(m, ok, v(Level::Problem, "ENGINE HOT 108C"), true, 60.0f, 1000));
    TEST_ASSERT_EQUAL_STRING(">CHECK ENGINE LIGHT ON", warn(m, ok, v(Level::Problem, "CHECK ENGINE LIGHT ON"), true, 60.0f, 2000));
    TEST_ASSERT_EQUAL_STRING(">2 STORED CODES", warn(m, ok, v(Level::Problem, "2 STORED CODES"), false, 0.0f, 3000));
}

// A yellow reboot mid-drive: the check would pop up while moving. It shows only below 3 km/h, and
// an unknown speed is no stop.
void test_check_shows_only_when_still() {
    TEST_ASSERT_TRUE(healthview::checkMayShow(0.0f));
    TEST_ASSERT_TRUE(healthview::checkMayShow(2.9f));
    TEST_ASSERT_FALSE(healthview::checkMayShow(3.0f));
    TEST_ASSERT_FALSE(healthview::checkMayShow(40.0f));
    TEST_ASSERT_FALSE(healthview::checkMayShow(NAN));
}

// Each real problem from the Watch gets its own advice, inside the screen's 39 characters.
static void adviceFor(const Verdict& batt, const Verdict& eng, const char* first) {
    const healthview::Check c = healthview::check(batt, 11.9f, eng, nullptr, NAN, 310.0f);
    TEST_ASSERT_TRUE(c.problem);
    TEST_ASSERT_EQUAL_STRING(first, c.advice[0].text);
    TEST_ASSERT_EQUAL_STRING("", c.lastTrip.text);
    TEST_ASSERT_EQUAL_STRING("", c.range.text);
}

void test_advice_follows_the_most_urgent_problem() {
    const Verdict ok = v(Level::Ok), low = v(Level::Problem, "BATTERY LOW WHILE RUNNING");
    adviceFor(low, ok, "THE CAR ISN'T CHARGING AS IT SHOULD.");
    adviceFor(ok, v(Level::Problem, "ENGINE HOT 108C"), "STOP WHEN SAFE AND TURN THE ENGINE OFF.");
    adviceFor(ok, v(Level::Problem, "CHECK ENGINE LIGHT ON"), "HAVE THE FAULT CODES READ SOON.");
    adviceFor(ok, v(Level::Problem, "2 STORED CODES"), "THE ENGINE LOGGED A FAULT.");
    adviceFor(ok, v(Level::Problem, "FUEL TRIM +22% THIS TRIP"), "THE FUEL MIXTURE IS FAR FROM NORMAL.");
    adviceFor(ok, v(Level::Problem, "SOMETHING NEW"), "HAVE IT CHECKED SOON.");
    adviceFor(low, v(Level::Problem, "ENGINE HOT 108C"), "STOP WHEN SAFE AND TURN THE ENGINE OFF.");   // the worst wins
    adviceFor(low, v(Level::Problem, "1 STORED CODE"), "THE CAR ISN'T CHARGING AS IT SHOULD.");
}

void test_no_advice_on_a_note_or_all_ok() {
    Verdict ok = v(Level::Ok), note = v(Level::Note, "BATTERY WAS LOW LAST TRIP");
    ok.known = note.known = true;
    for (const healthview::Check& c : {healthview::check(ok, 14.1f, ok, nullptr, NAN, 310.0f), healthview::check(note, 14.1f, ok, nullptr, NAN, 310.0f)}) {
        for (const healthview::Line& l : c.advice) TEST_ASSERT_EQUAL_STRING("", l.text);
        TEST_ASSERT_EQUAL_STRING("RANGE ~310 KM", c.range.text);
    }
}

void test_ok_engine_shows_the_last_trips_trim() {
    Verdict ok = v(Level::Ok);
    ok.known = true;
    TEST_ASSERT_EQUAL_STRING("NO FAULTS  FUEL TRIM +2%", healthview::check(ok, 14.1f, ok, nullptr, NAN, 310.0f, 2.4f).engineWhy.text);
    TEST_ASSERT_EQUAL_STRING("NO FAULTS  FUEL TRIM -3%", healthview::check(ok, 14.1f, ok, nullptr, NAN, 310.0f, -3.4f).engineWhy.text);
    TEST_ASSERT_EQUAL_STRING("NO FAULTS  FUEL TRIM +0%", healthview::check(ok, 14.1f, ok, nullptr, NAN, 310.0f, -0.2f).engineWhy.text);
    TEST_ASSERT_EQUAL_STRING("NO FAULTS SEEN", healthview::check(ok, 14.1f, ok, nullptr, NAN, 310.0f).engineWhy.text);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_advice_follows_the_most_urgent_problem);
    RUN_TEST(test_no_advice_on_a_note_or_all_ok);
    RUN_TEST(test_ok_engine_shows_the_last_trips_trim);
    RUN_TEST(test_check_all_ok_lines);
    RUN_TEST(test_check_problem_marks_only_that_item);
    RUN_TEST(test_check_note_is_not_a_problem);
    RUN_TEST(test_band_dashes_for_unknown);
    RUN_TEST(test_cost_uses_comma);
    RUN_TEST(test_parkgate_needs_five_seconds_and_half_km);
    RUN_TEST(test_parkgate_hides_on_move_and_dismiss_holds_until_moving);
    RUN_TEST(test_lines_fit_forty_chars);
    RUN_TEST(test_check_unknown_items_say_checking);
    RUN_TEST(test_check_one_known_ok_is_still_checking);
    RUN_TEST(test_parked_unknown_items_are_dashes);
    RUN_TEST(test_warning_critical_shows_while_moving);
    RUN_TEST(test_warning_problem_waits_for_a_stop);
    RUN_TEST(test_warning_notes_never_show);
    RUN_TEST(test_warning_each_at_most_every_two_minutes);
    RUN_TEST(test_warning_kind_ignores_the_figures);
    RUN_TEST(test_check_shows_only_when_still);
    return UNITY_END();
}
