#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/util/car/carhealth.h"
using namespace carhealth;

void setUp() {}
void tearDown() {}

static Reading rd(uint32_t ms, bool run, float volts) {
    Reading r;
    r.nowMs = ms;
    r.engineRunning = run;
    r.volts = volts;
    return r;
}

void test_battery_low_needs_sixty_seconds_running() {
    Watch w;
    for (uint32_t s = 0; s <= 59; s++) w.feed(rd(s * 1000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    w.feed(rd(60000, true, 11.9f));
    Verdict v = w.item(Item::Battery);
    TEST_ASSERT_EQUAL(Level::Problem, v.level);
    TEST_ASSERT_EQUAL_STRING("BATTERY LOW WHILE RUNNING", v.why);
}

void test_battery_recovery_resets_the_hold() {
    Watch w;
    for (uint32_t s = 0; s < 50; s++) w.feed(rd(s * 1000, true, 11.9f));
    w.feed(rd(50000, true, 13.8f));
    for (uint32_t s = 51; s <= 100; s++) w.feed(rd(s * 1000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
}

void test_battery_low_ignored_with_engine_off() {
    Watch w;
    for (uint32_t s = 0; s <= 120; s++) w.feed(rd(s * 1000, false, 11.5f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    TEST_ASSERT_EQUAL_STRING("", w.item(Item::Battery).why);
}

void test_smart_charging_idle_is_ok() {
    Watch w;
    for (uint32_t s = 0; s <= 600; s++) w.feed(rd(s * 1000, true, 12.3f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
}

void test_coolant_hot_is_critical_problem() {
    Watch w;
    Reading r;
    r.engineRunning = true;
    r.coolantC = 106.0f;
    w.feed(r);
    Verdict v = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Problem, v.level);
    TEST_ASSERT_EQUAL_STRING("ENGINE HOT 106C", v.why);
    TEST_ASSERT_TRUE(w.critical());
    r.coolantC = 105.0f;  // at the limit is not over it
    w.feed(r);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
    TEST_ASSERT_FALSE(w.critical());
}

void test_mil_or_new_stored_code_is_problem() {
    Watch w;
    Reading r;
    r.mil = true;
    w.feed(r);
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Engine).level);
    TEST_ASSERT_EQUAL_STRING("CHECK ENGINE LIGHT ON", w.item(Item::Engine).why);
    TEST_ASSERT_TRUE(w.critical());
    r.mil = false;
    r.storedCodes = 2;
    w.feed(r);
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Engine).level);
    TEST_ASSERT_EQUAL_STRING("2 STORED CODES", w.item(Item::Engine).why);
    TEST_ASSERT_FALSE(w.critical());
    r.storedCodes = 0;
    w.feed(r);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
}

void test_pending_code_is_note() {
    Watch w;
    Reading r;
    r.pendingCodes = 1;
    w.feed(r);
    Verdict v = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Note, v.level);
    TEST_ASSERT_EQUAL_STRING("1 PENDING CODE, NOT CONFIRMED", v.why);
    TEST_ASSERT_FALSE(w.critical());
}

void test_worst_condition_wins() {
    Watch w;
    Reading r;
    r.pendingCodes = 1;
    r.coolantC = 110.0f;
    w.feed(r);
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Engine).level);
    TEST_ASSERT_EQUAL_STRING("ENGINE HOT 110C", w.item(Item::Engine).why);
}

static void drive(Watch& w, float ltft) {
    for (uint32_t s = 0; s < 10; s++) {
        Reading r;
        r.nowMs = s * 1000;
        r.engineRunning = true;
        r.ltftPct = ltft;
        w.feed(r);
    }
}

void test_ltft_note_only_over_whole_trip() {
    Watch w;
    drive(w, 12.0f);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
    w.endTrip();
    Verdict v = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Note, v.level);
    TEST_ASSERT_EQUAL_STRING("FUEL TRIM +12% THIS TRIP", v.why);
}

void test_ltft_problem_over_twenty() {
    Watch w;
    drive(w, -22.0f);
    w.endTrip();
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Engine).level);
    TEST_ASSERT_EQUAL_STRING("FUEL TRIM -22% THIS TRIP", w.item(Item::Engine).why);
}

void test_ltft_clears_after_a_good_trip() {
    Watch w;
    drive(w, 12.0f);
    w.endTrip();
    drive(w, 3.0f);
    w.endTrip();
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
}

void test_unknown_readings_never_warn() {
    Watch w;
    Reading r;  // all unknown
    r.engineRunning = true;
    for (uint32_t s = 0; s <= 120; s++) {
        r.nowMs = s * 1000;
        w.feed(r);
    }
    w.endTrip();
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
    TEST_ASSERT_FALSE(w.critical());
}

void test_no_reading_yet_is_ok() {
    Watch w;
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
    TEST_ASSERT_FALSE(w.critical());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_battery_low_needs_sixty_seconds_running);
    RUN_TEST(test_battery_recovery_resets_the_hold);
    RUN_TEST(test_battery_low_ignored_with_engine_off);
    RUN_TEST(test_smart_charging_idle_is_ok);
    RUN_TEST(test_coolant_hot_is_critical_problem);
    RUN_TEST(test_mil_or_new_stored_code_is_problem);
    RUN_TEST(test_pending_code_is_note);
    RUN_TEST(test_worst_condition_wins);
    RUN_TEST(test_ltft_note_only_over_whole_trip);
    RUN_TEST(test_ltft_problem_over_twenty);
    RUN_TEST(test_ltft_clears_after_a_good_trip);
    RUN_TEST(test_unknown_readings_never_warn);
    RUN_TEST(test_no_reading_yet_is_ok);
    return UNITY_END();
}
