#include <unity.h>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include "../../src/util/yellow/padguard.h"

using namespace padguard;

void setUp() {}
void tearDown() {}

// The accelerometer swings 20 units between calls unless a test gives its own Z, so the pad is
// "in the hand" and never rests: only the rest tests below put it on the table.
static int16_t handZ() {
    static int n = 0;
    return (n++ % 2) ? -110 : -90;
}

static Report rep(uint32_t now, std::initializer_list<std::pair<int, uint8_t>> pressed, int16_t az = 0) {
    Report r{};
    for (const auto& p : pressed) {
        r.bits |= 1u << p.first;
        r.pressure[p.first] = p.second;
    }
    r.az = az ? az : handZ();
    r.now = now;
    return r;
}

static bool on(uint32_t out, int bit) { return (out >> bit) & 1u; }

void test_a_light_touch_is_not_a_press() {
    Guard g;
    TEST_ASSERT_FALSE(on(g.feed(rep(0, {{CROSS, 30}})), CROSS));
    TEST_ASSERT_TRUE(on(g.feed(rep(50, {{CROSS, 64}})), CROSS));
}

void test_a_press_releases_only_well_below_its_line() {
    Guard g;
    TEST_ASSERT_TRUE(on(g.feed(rep(0, {{CROSS, 200}})), CROSS));
    TEST_ASSERT_TRUE(on(g.feed(rep(50, {{CROSS, 30}})), CROSS));
    TEST_ASSERT_FALSE(on(g.feed(rep(100, {{CROSS, 23}})), CROSS));
}

void test_triggers_need_a_firm_squeeze() {
    Guard g;
    TEST_ASSERT_FALSE(on(g.feed(rep(0, {{R2, 176}})), R2));
    TEST_ASSERT_TRUE(on(g.feed(rep(50, {{R2, 200}})), R2));
    TEST_ASSERT_TRUE(on(g.feed(rep(100, {{R2, 61}})), R2));
    TEST_ASSERT_FALSE(on(g.feed(rep(150, {{R2, 59}})), R2));
}

void test_a_full_press_in_one_report_counts() {
    Guard g;
    TEST_ASSERT_TRUE(on(g.feed(rep(0, {{CROSS, 255}})), CROSS));
    TEST_ASSERT_FALSE(on(g.feed(rep(50, {})), CROSS));
}

void test_the_measured_set_down_does_nothing() {
    Guard g;
    TEST_ASSERT_EQUAL_UINT32(0, g.feed(rep(0, {{CROSS, 21}, {CIRCLE, 27}, {SQUARE, 15}, {TRIANGLE, 26}, {R1, 20}, {R2, 1}})));
    TEST_ASSERT_EQUAL_UINT32(0, g.feed(rep(50, {{CROSS, 255}, {CIRCLE, 27}, {SQUARE, 15}, {TRIANGLE, 26}, {R1, 20}, {R2, 1}})));
}

void test_two_buttons_at_once_still_count() {
    Guard g;
    const uint32_t out = g.feed(rep(0, {{L1, 255}, {R1, 255}}));
    TEST_ASSERT_TRUE(on(out, L1));
    TEST_ASSERT_TRUE(on(out, R1));
}

void test_a_held_button_survives_a_chord() {
    Guard g;
    g.feed(rep(0, {{CROSS, 255}}));
    const uint32_t out = g.feed(rep(50, {{CROSS, 255}, {CIRCLE, 30}, {SQUARE, 30}}));
    TEST_ASSERT_TRUE(on(out, CROSS));
    TEST_ASSERT_FALSE(on(out, CIRCLE));
    TEST_ASSERT_FALSE(on(out, SQUARE));
}

void test_a_stuck_button_drops_out_after_five_seconds() {
    Guard g;
    TEST_ASSERT_TRUE(on(g.feed(rep(0, {{UP, 200}})), UP));
    TEST_ASSERT_TRUE(on(g.feed(rep(4999, {{UP, 200}})), UP));
    TEST_ASSERT_FALSE(on(g.feed(rep(5000, {{UP, 200}})), UP));
    TEST_ASSERT_FALSE(on(g.feed(rep(5050, {{UP, 200}})), UP));
    TEST_ASSERT_FALSE(on(g.feed(rep(5100, {{UP, 10}})), UP));
    TEST_ASSERT_TRUE(on(g.feed(rep(5150, {{UP, 200}})), UP));
}

void test_a_stuck_button_does_not_lock_the_others() {
    Guard g;
    g.feed(rep(0, {{UP, 200}}));
    g.feed(rep(5000, {{UP, 200}}));
    const uint32_t out = g.feed(rep(5050, {{UP, 200}, {CROSS, 255}}));
    TEST_ASSERT_FALSE(on(out, UP));
    TEST_ASSERT_TRUE(on(out, CROSS));
}

// Lying still for 1.1 s (Z within ±2): resting from t = 1000.
static void putDown(Guard& g, uint32_t from) {
    for (uint32_t t = 0; t <= 1100; t += 50) g.feed(rep(from + t, {}, (t / 50) % 2 ? -108 : -112));
}

void test_picking_it_up_pauses_presses() {
    Guard g;
    putDown(g, 0);
    TEST_ASSERT_TRUE(g.resting());
    TEST_ASSERT_FALSE(on(g.feed(rep(1200, {{CROSS, 255}}, -60)), CROSS));
    TEST_ASSERT_FALSE(g.resting());
    TEST_ASSERT_TRUE(on(g.feed(rep(1700, {{CROSS, 255}}, -60)), CROSS));
}

void test_hand_motion_never_pauses() {
    Guard g;
    for (uint32_t t = 0; t < 2000; t += 50) g.feed(rep(t, {}, (t / 50) % 2 ? -80 : -110));
    TEST_ASSERT_FALSE(g.resting());
    TEST_ASSERT_TRUE(on(g.feed(rep(2000, {{CROSS, 255}}, -110)), CROSS));
}

void test_reset_forgets_held_presses_and_rest() {
    Guard g;
    putDown(g, 0);
    g.feed(rep(1600, {{CROSS, 255}}, -110));
    TEST_ASSERT_TRUE(g.resting());
    g.reset();
    TEST_ASSERT_FALSE(g.resting());
    TEST_ASSERT_FALSE(g.paused(1650));
    TEST_ASSERT_FALSE(on(g.feed(rep(1650, {})), CROSS));
    TEST_ASSERT_TRUE(on(g.feed(rep(1700, {{CROSS, 255}})), CROSS));
}

void test_millis_wrap() {
    const uint32_t base = 0xFFFFFF00u;
    Guard g;
    TEST_ASSERT_TRUE(on(g.feed(rep(base, {{UP, 200}})), UP));
    TEST_ASSERT_TRUE(on(g.feed(rep(base + 4999, {{UP, 200}})), UP));
    TEST_ASSERT_FALSE(on(g.feed(rep(base + 5000, {{UP, 200}})), UP));

    Guard h;
    putDown(h, base);
    TEST_ASSERT_FALSE(on(h.feed(rep(base + 1200, {{CROSS, 255}}, -60)), CROSS));
    TEST_ASSERT_TRUE(on(h.feed(rep(base + 1700, {{CROSS, 255}}, -60)), CROSS));
}

void test_paused_and_stuck_report_their_state() {
    Guard g;
    putDown(g, 0);
    g.feed(rep(1200, {}, -60));
    TEST_ASSERT_TRUE(g.paused(1200));
    TEST_ASSERT_TRUE(g.paused(1699));
    TEST_ASSERT_FALSE(g.paused(1700));

    Guard s;
    s.feed(rep(0, {{UP, 200}}));
    TEST_ASSERT_EQUAL_UINT32(0, s.stuck());
    s.feed(rep(5000, {{UP, 200}}));
    TEST_ASSERT_EQUAL_UINT32(1u << UP, s.stuck());
    s.feed(rep(5050, {}));
    TEST_ASSERT_EQUAL_UINT32(0, s.stuck());
}

// A new connection arms once no button is past its press line: a resting finger (a light bit) or
// a trigger touched in the grip must not keep the pad dead.
void test_idle_ignores_light_touches() {
    TEST_ASSERT_TRUE(idle(rep(0, {})));
    TEST_ASSERT_TRUE(idle(rep(0, {{UP, 30}, {L2, 150}})));
    TEST_ASSERT_FALSE(idle(rep(0, {{CROSS, 64}})));
    TEST_ASSERT_FALSE(idle(rep(0, {{R2, 200}})));
    TEST_ASSERT_FALSE(idle(rep(0, {{START, 255}})));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_light_touch_is_not_a_press);
    RUN_TEST(test_a_press_releases_only_well_below_its_line);
    RUN_TEST(test_triggers_need_a_firm_squeeze);
    RUN_TEST(test_a_full_press_in_one_report_counts);
    RUN_TEST(test_the_measured_set_down_does_nothing);
    RUN_TEST(test_two_buttons_at_once_still_count);
    RUN_TEST(test_a_held_button_survives_a_chord);
    RUN_TEST(test_a_stuck_button_drops_out_after_five_seconds);
    RUN_TEST(test_a_stuck_button_does_not_lock_the_others);
    RUN_TEST(test_picking_it_up_pauses_presses);
    RUN_TEST(test_hand_motion_never_pauses);
    RUN_TEST(test_reset_forgets_held_presses_and_rest);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_paused_and_stuck_report_their_state);
    RUN_TEST(test_idle_ignores_light_touches);
    return UNITY_END();
}
