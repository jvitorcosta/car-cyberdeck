#include <unity.h>
#include <cstdint>
#include "../../src/util/obdplan.h"

using obd::Pid;
using obd::Reply;
using obdplan::Band;
using obdplan::Link;

void setUp() {}
void tearDown() {}

static bool isFast(Pid p) { return p == Pid::Rpm || p == Pid::Speed; }

void test_fast_pair_every_cycle_slow_ones_in_turn() {
    obdplan::Plan plan;
    uint32_t t = 1000;
    int slow = 0, fast = 0;
    bool seen[(int)Pid::COUNT] = {};
    for (int i = 0; i < 200; ++i, t += 100) {            // 20 s of 100 ms requests
        const Pid p = plan.next(t);
        seen[(int)p] = true;
        (isFast(p) ? fast : slow)++;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_INT_WITHIN(2, 10, slow);                 // one slow request per SLOW_MS
    TEST_ASSERT_EQUAL_INT(200 - slow, fast);
    for (int i = 0; i < (int)Pid::COUNT; ++i) TEST_ASSERT_TRUE(seen[i]);   // all of them, in turn
}

void test_unsupported_pid_backs_off_then_is_asked_again() {
    obdplan::Plan plan;
    uint32_t t = 0xFFFFF000u;                             // the back-off crosses the millis() wrap
    bool marked = false;
    uint32_t noDataAt = 0;
    for (int i = 0; i < 600; ++i, t += 100) {            // 60 s of 100 ms requests
        const Pid p = plan.next(t);
        if (p == Pid::Speed) {
            if (!marked) {                               // the car says NO DATA once
                marked = true;
                noDataAt = t;
                plan.done(p, Reply::NoData, t);
                continue;
            }
            // asked again only once RETRY_MS has passed, at the next fast turn after it
            TEST_ASSERT_UINT32_WITHIN(500, obdplan::RETRY_MS + 400, t - noDataAt);
            return;
        }
        plan.done(p, Reply::Ok, t);
    }
    TEST_FAIL_MESSAGE("speed was never asked again");
}

void test_every_pid_unsupported_still_gives_a_request() {
    obdplan::Plan plan;
    uint32_t t = 0;
    for (int i = 0; i < 100; ++i, t += 50) {
        const Pid p = plan.next(t);
        TEST_ASSERT_TRUE((int)p < (int)Pid::COUNT);
        plan.done(p, p == Pid::Battery ? Reply::Ok : Reply::NoData, t);
    }
}

void test_health_states() {
    obdplan::Health h;
    TEST_ASSERT_EQUAL_INT((int)Link::Connecting, (int)h.state());
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::Ok));
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::NoData));   // the car answered
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)h.reply(Reply::NoEngine));
    h.reply(Reply::Error);
    h.reply(Reply::Error);
    TEST_ASSERT_FALSE(h.lost());
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::Ok));       // errors reset
    for (int i = 0; i < obdplan::LOST_AFTER; ++i) h.reply(Reply::Error);
    TEST_ASSERT_TRUE(h.lost());
}

void test_hold_shows_at_once_and_leaves_after_5_s_off() {
    obdplan::Hold hold;
    uint32_t t = 0xFFFFFF00u;                             // across the millis() wrap
    TEST_ASSERT_FALSE(hold.update(false, t));
    TEST_ASSERT_TRUE(hold.update(true, t += 10));
    TEST_ASSERT_TRUE(hold.update(false, t += 10));
    TEST_ASSERT_TRUE(hold.update(true, t += 4000));       // a blip: still shown, timer resets
    const uint32_t live = t;
    TEST_ASSERT_TRUE(hold.update(false, live + 10));
    TEST_ASSERT_TRUE(hold.update(false, live + obdplan::HOLD_MS));
    TEST_ASSERT_FALSE(hold.update(false, live + obdplan::HOLD_MS + 1));
}

void test_colour_bands_at_their_edges() {
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::rpmBand(4500, 6500));     // 69 %
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::rpmBand(4600, 6500));   // 71 %
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::rpmBand(5800, 6500));   // 89 %
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::rpmBand(5900, 6500));    // 91 %
    TEST_ASSERT_EQUAL_INT((int)Band::Cold, (int)obdplan::coolantBand(59.9f));
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::coolantBand(105.0f));
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::coolantBand(105.1f));
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::batteryBand(11.79f));
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::batteryBand(12.19f));
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::batteryBand(12.2f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(-5, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, obdplan::frac(150, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(5, 10, 10));   // empty range: no division by 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fast_pair_every_cycle_slow_ones_in_turn);
    RUN_TEST(test_unsupported_pid_backs_off_then_is_asked_again);
    RUN_TEST(test_every_pid_unsupported_still_gives_a_request);
    RUN_TEST(test_health_states);
    RUN_TEST(test_hold_shows_at_once_and_leaves_after_5_s_off);
    RUN_TEST(test_colour_bands_at_their_edges);
    return UNITY_END();
}
