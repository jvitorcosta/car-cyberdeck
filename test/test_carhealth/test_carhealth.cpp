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

// A missed ATRV (Bluetooth hiccup) is not a recovery: the hold keeps running from the first low reading.
void test_battery_missed_readings_keep_the_hold() {
    Watch w;
    w.feed(rd(0, true, 11.9f));
    for (uint32_t s = 1; s <= 20; s++) w.feed(rd(s * 1000, true, NAN));
    for (uint32_t s = 21; s <= 59; s++) w.feed(rd(s * 1000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    w.feed(rd(60000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Battery).level);
    w.feed(rd(61000, true, NAN));  // still low as far as we know
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Battery).level);
}

void test_battery_only_a_good_reading_or_engine_off_clears_the_hold() {
    Watch w;
    w.feed(rd(0, true, 11.9f));
    w.feed(rd(30000, false, NAN));  // engine off: the hold starts over
    for (uint32_t s = 31; s <= 89; s++) w.feed(rd(s * 1000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    w.feed(rd(91000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Battery).level);
    w.feed(rd(92000, true, 12.0f));  // at the limit is a good reading
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
}

void test_memo_roundtrip_and_bad_records() {
    Memo m{true, -37.5f, 12345u};
    uint8_t buf[MEMO_BYTES];
    encode(m, buf);
    TEST_ASSERT_EQUAL_UINT8(1, buf[0]);
    Memo r{false, 0.0f, 0u};
    TEST_ASSERT_TRUE(decode(buf, sizeof buf, &r));
    TEST_ASSERT_TRUE(r.battLowSeen);
    TEST_ASSERT_EQUAL_FLOAT(-37.5f, r.ltftSum);
    TEST_ASSERT_EQUAL_UINT32(12345u, r.ltftN);
    Memo z{false, 0.0f, 0u};
    encode(z, buf);
    TEST_ASSERT_TRUE(decode(buf, sizeof buf, &r));
    TEST_ASSERT_FALSE(r.battLowSeen);
    TEST_ASSERT_EQUAL_UINT32(0u, r.ltftN);

    Memo keep{true, 5.0f, 7u};
    TEST_ASSERT_FALSE(decode(buf, MEMO_BYTES - 1, &keep));
    TEST_ASSERT_FALSE(decode(buf, MEMO_BYTES + 1, &keep));
    TEST_ASSERT_FALSE(decode(nullptr, MEMO_BYTES, &keep));
    buf[0] = 2;
    TEST_ASSERT_FALSE(decode(buf, sizeof buf, &keep));
    Memo inf{false, INFINITY, 3u};
    encode(inf, buf);
    TEST_ASSERT_FALSE(decode(buf, sizeof buf, &keep));
    Memo nan{false, NAN, 3u};
    encode(nan, buf);
    TEST_ASSERT_FALSE(decode(buf, sizeof buf, &keep));
    TEST_ASSERT_TRUE(keep.battLowSeen);  // untouched by the failures
    TEST_ASSERT_EQUAL_UINT32(7u, keep.ltftN);
}

void test_memo_tracks_this_trip() {
    Watch w;
    Memo m = w.memo();
    TEST_ASSERT_FALSE(m.battLowSeen);
    TEST_ASSERT_EQUAL_UINT32(0u, m.ltftN);
    drive(w, 12.0f);
    for (uint32_t s = 0; s <= 60; s++) w.feed(rd(s * 1000, true, 11.9f));
    m = w.memo();
    TEST_ASSERT_TRUE(m.battLowSeen);
    TEST_ASSERT_EQUAL_UINT32(10u, m.ltftN);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, m.ltftSum);
    w.endTrip();
    m = w.memo();
    TEST_ASSERT_FALSE(m.battLowSeen);
    TEST_ASSERT_EQUAL_UINT32(0u, m.ltftN);
}

// At boot after a power cut: the last trip's memo is restored, then judged as if it had ended normally.
void test_restore_then_end_trip_judges_the_last_trip() {
    Watch w;
    w.restore(Memo{false, 120.0f, 10u});  // mean +12 %
    w.endTrip();
    Verdict e = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Note, e.level);
    TEST_ASSERT_EQUAL_STRING("FUEL TRIM +12% THIS TRIP", e.why);
    TEST_ASSERT_TRUE(e.known);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    TEST_ASSERT_FALSE(w.item(Item::Battery).known);
}

// A restored last-trip LTFT must not hide a live reason of the same level (live readings win ties).
void test_live_mil_beats_restored_ltft_problem() {
    Watch w;
    w.restore(Memo{false, 250.0f, 10u});  // mean +25 %
    w.endTrip();
    Reading r;
    r.nowMs = 1000;
    r.engineRunning = true;
    r.mil = true;
    w.feed(r);
    Verdict e = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Problem, e.level);
    TEST_ASSERT_EQUAL_STRING("CHECK ENGINE LIGHT ON", e.why);
}

void test_live_pending_beats_restored_ltft_note() {
    Watch w;
    w.restore(Memo{false, 120.0f, 10u});  // mean +12 %
    w.endTrip();
    Reading r;
    r.nowMs = 1000;
    r.engineRunning = true;
    r.pendingCodes = 1;
    w.feed(r);
    Verdict e = w.item(Item::Engine);
    TEST_ASSERT_EQUAL(Level::Note, e.level);
    TEST_ASSERT_EQUAL_STRING("1 PENDING CODE, NOT CONFIRMED", e.why);
}

void test_restored_low_battery_is_a_note() {
    Watch w;
    w.restore(Memo{true, 0.0f, 0u});
    w.endTrip();
    Verdict b = w.item(Item::Battery);
    TEST_ASSERT_EQUAL(Level::Note, b.level);
    TEST_ASSERT_EQUAL_STRING("BATTERY WAS LOW LAST TRIP", b.why);
    TEST_ASSERT_TRUE(b.known);
    TEST_ASSERT_FALSE(w.item(Item::Engine).known);
    // a live problem later in the new trip still wins
    for (uint32_t s = 0; s <= 60; s++) w.feed(rd(s * 1000, true, 11.9f));
    TEST_ASSERT_EQUAL(Level::Problem, w.item(Item::Battery).level);
    TEST_ASSERT_EQUAL_STRING("BATTERY LOW WHILE RUNNING", w.item(Item::Battery).why);
}

void test_known_needs_a_reading_or_a_codes_read() {
    Watch w;
    TEST_ASSERT_FALSE(w.item(Item::Battery).known);
    TEST_ASSERT_FALSE(w.item(Item::Engine).known);
    w.feed(rd(0, true, NAN));
    TEST_ASSERT_FALSE(w.item(Item::Battery).known);
    w.feed(rd(1000, true, 13.9f));
    TEST_ASSERT_TRUE(w.item(Item::Battery).known);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Battery).level);
    TEST_ASSERT_FALSE(w.item(Item::Engine).known);
    w.codesRead();
    TEST_ASSERT_TRUE(w.item(Item::Engine).known);
    TEST_ASSERT_EQUAL(Level::Ok, w.item(Item::Engine).level);
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
    RUN_TEST(test_battery_missed_readings_keep_the_hold);
    RUN_TEST(test_battery_only_a_good_reading_or_engine_off_clears_the_hold);
    RUN_TEST(test_memo_roundtrip_and_bad_records);
    RUN_TEST(test_memo_tracks_this_trip);
    RUN_TEST(test_restore_then_end_trip_judges_the_last_trip);
    RUN_TEST(test_live_mil_beats_restored_ltft_problem);
    RUN_TEST(test_live_pending_beats_restored_ltft_note);
    RUN_TEST(test_restored_low_battery_is_a_note);
    RUN_TEST(test_known_needs_a_reading_or_a_codes_read);
    return UNITY_END();
}
