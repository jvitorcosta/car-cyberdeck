#include <unity.h>
#include <cstdint>
#include <initializer_list>
#include "../../src/util/obdplan.h"

using obd::Pid;
using obd::Reply;
using obdplan::Link;

void setUp() {}
void tearDown() {}

static bool isFast(Pid p) {
    return p == Pid::Rpm || p == Pid::Speed || p == Pid::PedalD || p == Pid::PedalE || p == Pid::Throttle ||
           p == Pid::Maf || p == Pid::Map;
}
// Unread bits: every PID supported except those cleared.
static void allBut(uint32_t bits[4], std::initializer_list<uint8_t> off) {
    for (int i = 0; i < 4; ++i) bits[i] = 0xFFFFFFFFu;
    for (uint8_t pid : off) bits[(pid - 1) / 32] &= ~(1u << (31 - (pid - 1) % 32));
}

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
    for (int i = 0; i < (int)Pid::COUNT; ++i)              // all of them, in turn; the fallbacks
        if ((Pid)i != Pid::PedalE && (Pid)i != Pid::Map)   // (pedal E, MAP) only without D / MAF
            TEST_ASSERT_TRUE(seen[i]);
}

void test_rpm_and_speed_at_least_three_a_second() {
    obdplan::Plan plan;
    int rpm = 0, speed = 0;
    for (uint32_t t = 1000; t < 11000; t += 80) {          // 10 s at 80 ms a request
        const Pid p = plan.next(t);
        rpm += p == Pid::Rpm;
        speed += p == Pid::Speed;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_GREATER_OR_EQUAL(30, rpm);
    TEST_ASSERT_GREATER_OR_EQUAL(30, speed);
}

void test_pedal_falls_back_to_e_then_nothing() {
    uint32_t bits[4];
    allBut(bits, {0x49});
    obdplan::Plan plan;
    plan.setSupported(bits);
    bool d = false, e = false;
    for (uint32_t t = 0; t < 5000; t += 80) {
        const Pid p = plan.next(t);
        d |= p == Pid::PedalD;
        e |= p == Pid::PedalE;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_FALSE(d);
    TEST_ASSERT_TRUE(e);
    allBut(bits, {0x49, 0x4A});
    obdplan::Plan none;
    none.setSupported(bits);
    for (uint32_t t = 0; t < 5000; t += 80) {
        const Pid p = none.next(t);
        TEST_ASSERT_FALSE(p == Pid::PedalD || p == Pid::PedalE);
        none.done(p, Reply::Ok, t);
    }
}

void test_maf_preferred_map_otherwise() {
    uint32_t bits[4];
    allBut(bits, {});
    obdplan::Plan maf;
    maf.setSupported(bits);
    bool sawMaf = false;
    for (uint32_t t = 0; t < 5000; t += 80) {
        const Pid p = maf.next(t);
        sawMaf |= p == Pid::Maf;
        TEST_ASSERT_FALSE(p == Pid::Map);
        maf.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_TRUE(sawMaf);
    allBut(bits, {0x10});
    obdplan::Plan map;
    map.setSupported(bits);
    bool sawMap = false;
    for (uint32_t t = 0; t < 5000; t += 80) {
        const Pid p = map.next(t);
        sawMap |= p == Pid::Map;
        TEST_ASSERT_FALSE(p == Pid::Maf);
        map.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_TRUE(sawMap);
}

void test_unsupported_slow_pid_is_never_asked() {
    uint32_t bits[4];
    allBut(bits, {0x52, 0x2F});                           // no ethanol, no fuel level
    obdplan::Plan plan;
    plan.setSupported(bits);
    bool battery = false;
    for (uint32_t t = 0; t < 40000; t += 80) {
        const Pid p = plan.next(t);
        TEST_ASSERT_FALSE(p == Pid::Ethanol || p == Pid::Fuel);
        battery |= p == Pid::Battery;                     // ATRV is the adapter's: always asked
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_TRUE(battery);
}

void test_crank_watch_is_mostly_battery() {
    obdplan::Plan plan;
    plan.crankWatch(true);
    int bat = 0, rpm = 0;
    for (uint32_t t = 0; t < 2000; t += 100) {
        const Pid p = plan.next(t);
        bat += p == Pid::Battery;
        rpm += p == Pid::Rpm;
        TEST_ASSERT_TRUE(p == Pid::Battery || p == Pid::Rpm);
    }
    TEST_ASSERT_EQUAL_INT(18, bat);
    TEST_ASSERT_EQUAL_INT(2, rpm);
    plan.crankWatch(false);
    bool other = false;
    for (uint32_t t = 2000; t < 4000; t += 100) {
        const Pid p = plan.next(t);
        other |= p != Pid::Battery && p != Pid::Rpm;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_TRUE(other);
}

void test_stale_and_warnings() {
    TEST_ASSERT_EQUAL_UINT32(3000, obdplan::staleAfterMs(Pid::Rpm));
    TEST_ASSERT_EQUAL_UINT32(3000, obdplan::staleAfterMs(Pid::PedalE));
    TEST_ASSERT_EQUAL_UINT32(30000, obdplan::staleAfterMs(Pid::Coolant));
    TEST_ASSERT_EQUAL_UINT32(30000, obdplan::staleAfterMs(Pid::Battery));
    TEST_ASSERT_TRUE(obdplan::batteryWarn(12.9f, true));
    TEST_ASSERT_FALSE(obdplan::batteryWarn(12.9f, false));
    TEST_ASSERT_TRUE(obdplan::batteryWarn(12.1f, false));
    TEST_ASSERT_FALSE(obdplan::batteryWarn(14.1f, true));
    TEST_ASSERT_TRUE(obdplan::coolantWarn(106));
    TEST_ASSERT_FALSE(obdplan::coolantWarn(105));
    TEST_ASSERT_TRUE(obdplan::rpmWarn(5525, 6500));
    TEST_ASSERT_FALSE(obdplan::rpmWarn(5500, 6500));
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
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Pid::Rpm, Reply::Ok));
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Pid::Load, Reply::NoData));   // just unsupported
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)h.reply(Pid::Speed, Reply::NoEngine));
    h.reply(Pid::Rpm, Reply::Error);
    h.reply(Pid::Rpm, Reply::Error);
    TEST_ASSERT_FALSE(h.lost());
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Pid::Rpm, Reply::Ok));       // errors reset
    for (int i = 0; i < obdplan::LOST_AFTER; ++i) h.reply(Pid::Rpm, Reply::Error);
    TEST_ASSERT_TRUE(h.lost());
}

// Ignition off: the adapter still reads the battery, and on CAN every PID may say NO DATA. The
// gauges must not come back for either (RPM is mandatory on every OBD-II car).
void test_ignition_off_never_reads_as_live() {
    obdplan::Health h;
    h.reply(Pid::Rpm, Reply::NoEngine);
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)h.reply(Pid::Battery, Reply::Ok));
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)h.reply(Pid::Rpm, Reply::NoData));
    obdplan::Health fresh;
    TEST_ASSERT_EQUAL_INT((int)Link::Connecting, (int)fresh.reply(Pid::Battery, Reply::Ok));
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Pid::Rpm, Reply::Ok));       // engine back
}

// RPM is never backed off: after a NO DATA (ignition off) it is asked again at once, so the
// gauges return as soon as the engine runs.
void test_rpm_is_never_backed_off() {
    obdplan::Plan plan;
    uint32_t t = 0;
    for (int i = 0; i < 20; ++i, t += 100) {
        const Pid p = plan.next(t);
        plan.done(p, Reply::NoData, t);
    }
    bool rpm = false;
    for (int i = 0; i < 4 && !rpm; ++i, t += 100) rpm = plan.next(t) == Pid::Rpm;
    TEST_ASSERT_TRUE(rpm);
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

void test_frac_clamps() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(-5, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, obdplan::frac(150, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(5, 10, 10));   // empty range: no division by 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fast_pair_every_cycle_slow_ones_in_turn);
    RUN_TEST(test_rpm_and_speed_at_least_three_a_second);
    RUN_TEST(test_pedal_falls_back_to_e_then_nothing);
    RUN_TEST(test_maf_preferred_map_otherwise);
    RUN_TEST(test_unsupported_slow_pid_is_never_asked);
    RUN_TEST(test_crank_watch_is_mostly_battery);
    RUN_TEST(test_stale_and_warnings);
    RUN_TEST(test_unsupported_pid_backs_off_then_is_asked_again);
    RUN_TEST(test_every_pid_unsupported_still_gives_a_request);
    RUN_TEST(test_health_states);
    RUN_TEST(test_ignition_off_never_reads_as_live);
    RUN_TEST(test_rpm_is_never_backed_off);
    RUN_TEST(test_hold_shows_at_once_and_leaves_after_5_s_off);
    RUN_TEST(test_frac_clamps);
    return UNITY_END();
}
