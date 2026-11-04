#include <unity.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../../src/util/car/cameras.h"

void setUp() {}
void tearDown() {}

using cameras::Stage;

// Manaus, Av. Djalma Batista-ish: the tests run where the deck will.
static const int32_t LAT = -3100000, LON = -60020000;
static const double M_PER_E6 = 6371000.0 * 3.14159265358979323846 / 180.0 / 1e6;

static camlist::Point pt(int32_t lat, int32_t lon, uint16_t heading, uint8_t limit, camlist::Kind k) {
    camlist::Point p;
    memset(&p, 0, sizeof p);
    p.latE6 = lat;
    p.lonE6 = lon;
    p.heading = heading;
    p.limit = limit;
    p.kind = k;
    strcpy(p.id, "t");
    return p;
}

// The car `metresBefore` metres before (lat, lon) along `course` (negative: past it), and `metresRight`
// to the right of that line, driving along `course`.
static cameras::Fix beside(int32_t lat, int32_t lon, uint16_t course, double metresBefore, double metresRight,
                           uint16_t kmh, uint32_t ms) {
    double c = course * 3.14159265358979323846 / 180.0;
    double east = -metresBefore * sin(c) + metresRight * cos(c);
    double north = -metresBefore * cos(c) - metresRight * sin(c);
    double cosLat = cos(lat / 1e6 * 3.14159265358979323846 / 180.0);
    cameras::Fix f;
    f.valid = true;
    f.latE6 = lat + (int32_t)lround(north / M_PER_E6);
    f.lonE6 = lon + (int32_t)lround(east / (M_PER_E6 * cosLat));
    f.kmh = kmh;
    f.course = course;
    f.ms = ms;
    return f;
}

static cameras::Fix along(int32_t lat, int32_t lon, uint16_t course, int metresBefore, uint16_t kmh, uint32_t ms) {
    return beside(lat, lon, course, metresBefore, 0, kmh, ms);
}

static cameras::Fix noFix(uint32_t ms) {
    cameras::Fix f;
    memset(&f, 0, sizeof f);
    f.valid = false;
    f.ms = ms;
    return f;
}

// --- geometry ---

void test_geometry_helpers() {
    // 0.001 degree of latitude is 111.2 m; north is 0, east is 90.
    TEST_ASSERT_UINT32_WITHIN(1, 111, cameras::distanceM(LAT, LON, LAT + 1000, LON));
    TEST_ASSERT_EQUAL_UINT16(0, cameras::bearing(LAT, LON, LAT + 1000, LON));
    TEST_ASSERT_EQUAL_UINT16(90, cameras::bearing(LAT, LON, LAT, LON + 1000));
    TEST_ASSERT_EQUAL_UINT16(180, cameras::bearing(LAT, LON, LAT - 1000, LON));
    TEST_ASSERT_EQUAL_UINT16(270, cameras::bearing(LAT, LON, LAT, LON - 1000));
    TEST_ASSERT_EQUAL_INT(15, cameras::angleDiff(10, 355));
    TEST_ASSERT_EQUAL_INT(-15, cameras::angleDiff(355, 10));
    TEST_ASSERT_EQUAL_INT(180, cameras::angleDiff(0, 180));
    TEST_ASSERT_EQUAL_INT(180, cameras::angleDiff(180, 0));
    // A point 20 m to the right of a car heading north, and to its left.
    camlist::Point p = pt(LAT, LON, 0, 60, camlist::Kind::Speed);
    TEST_ASSERT_INT32_WITHIN(1, 20, cameras::crossTrackM(beside(LAT, LON, 0, 100, -20, 60, 0), p));
    TEST_ASSERT_INT32_WITHIN(1, -20, cameras::crossTrackM(beside(LAT, LON, 0, 100, 20, 60, 0), p));
}

// --- stages ---

void test_alert_at_200m() {
    camlist::Point p = pt(LAT, LON, 90, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 210, 60, 1000)).stage == Stage::Off);
    cameras::Status s = m.update(along(LAT, LON, 90, 190, 60, 2000));
    TEST_ASSERT_TRUE(s.stage == Stage::Alert);
    TEST_ASSERT_EQUAL_INT(0, s.index);
    TEST_ASSERT_UINT16_WITHIN(5, 190, s.distM);
    TEST_ASSERT_EQUAL_UINT16(60, s.kmh);
}

void test_alert_reaches_further_when_fast() {
    camlist::Point p = pt(LAT, LON, 90, 80, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 350, 100, 1000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 320, 100, 2000)).stage == Stage::Alert);
}

// The driver brakes for the alert: the shorter 12 s distance must not take the alert back.
void test_alert_stays_when_braking() {
    camlist::Point p = pt(LAT, LON, 90, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 320, 100, 1000)).stage == Stage::Alert);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 305, 89, 2000)).stage == Stage::Alert);   // 296 m at 89
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 290, 80, 3000)).stage == Stage::Alert);   // 266 m at 80
    // Off the road (a turn onto a parallel street) drops the latch: back at 305 m it is Off again.
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 0, 280, 60, 4000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 305, 89, 5000)).stage == Stage::Off);
}

void test_redlight_alerts_at_200m_never_alarms() {
    camlist::Point p = pt(LAT, LON, 0, camlist::NO_LIMIT, camlist::Kind::RedLight);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 0, 250, 90, 1000)).stage == Stage::Off);  // not 12 s (300 m)
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 0, 190, 90, 2000)).stage == Stage::Alert);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 0, 100, 90, 3000)).stage == Stage::Alert);
}

void test_alarm_over_limit_in_last_100m() {
    camlist::Point p = pt(LAT, LON, 180, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 180, 90, 61, 1000)).stage == Stage::Alarm);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 180, 90, 60, 2000)).stage == Stage::Alert);
    cameras::Matcher m2;
    m2.load(&p, 1);
    TEST_ASSERT_TRUE(m2.update(along(LAT, LON, 180, 110, 61, 1000)).stage == Stage::Alert);
}

void test_bump_alarms_too() {
    camlist::Point p = pt(LAT, LON, 180, 30, camlist::Kind::Bump);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 180, 90, 40, 1000)).stage == Stage::Alarm);
}

void test_unknown_limit_never_alarms() {
    camlist::Point p = pt(LAT, LON, 45, camlist::NO_LIMIT, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 45, 100, 120, 1000)).stage == Stage::Alert);
}

// --- which point ---

void test_opposite_lane_silent() {
    camlist::Point p = pt(LAT, LON, 90, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    cameras::Status s = m.update(along(LAT, LON, 270, 150, 80, 1000));
    TEST_ASSERT_TRUE(s.stage == Stage::Off);
    TEST_ASSERT_EQUAL_INT(-1, s.index);
}

void test_parallel_road_silent() {
    camlist::Point p = pt(LAT, LON, 0, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(beside(LAT, LON, 0, 90, 40, 80, 1000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(beside(LAT, LON, 0, 90, 20, 80, 2000)).stage == Stage::Alarm);
    TEST_ASSERT_TRUE(m.update(beside(LAT, LON, 0, 90, 20, 50, 3000)).stage == Stage::Alert);
}

void test_overpass_silent() {
    camlist::Point p = pt(LAT, LON, 0, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(beside(LAT, LON, 0, 100, -35, 50, 1000)).stage == Stage::Off);
}

void test_point_behind_is_silent() {
    camlist::Point p = pt(LAT, LON, 0, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 0, -100, 50, 1000)).stage == Stage::Off);
}

// Drive through at 60 km/h (17 m per 1 s fix), then loop round the block and come at it again.
void test_passed_clears_and_waits_60s() {
    camlist::Point p = pt(LAT, LON, 90, 50, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    uint32_t t = 1000;
    bool alerted = false;
    for (int d = 300; d >= -80; d -= 17, t += 1000) {
        cameras::Status s = m.update(along(LAT, LON, 90, d, 60, t));
        if (s.stage != Stage::Off) alerted = true;
        if (d < 0) TEST_ASSERT_TRUE(s.stage == Stage::Off);
    }
    TEST_ASSERT_TRUE(alerted);
    uint32_t past = t - 1000;  // the last fix, ~80 m past
    // Back on the same road the same way within the minute: still passed.
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 190, 60, past + 30000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 173, 60, past + 31000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 190, 60, past + 61000)).stage == Stage::Alert);
}

// Closer than 60 m but stopped at a light right before it: jitter must not count as passed.
void test_slow_jitter_does_not_pass() {
    camlist::Point p = pt(LAT, LON, 90, 50, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 50, 30, 1000)).stage == Stage::Alert);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 52, 3, 2000)).stage == Stage::Off);  // stopped
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 45, 20, 3000)).stage == Stage::Alert);
}

void test_next_point_after_passing() {
    // Two points 300 m apart on one eastbound road; 100 km/h (333 m alert), a fix every 28 m.
    camlist::Point p[2];
    p[0] = pt(LAT, LON, 90, 60, camlist::Kind::Speed);
    cameras::Fix second = along(LAT, LON, 90, -300, 100, 0);
    p[1] = pt(second.latE6, second.lonE6, 90, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(p, 2);
    uint32_t t = 1000;
    int d = 320;
    for (; d >= 0; d -= 28, t += 1000) {
        cameras::Status s = m.update(along(LAT, LON, 90, d, 100, t));
        TEST_ASSERT_EQUAL_INT(0, s.index);
        TEST_ASSERT_TRUE(s.stage == Stage::Alarm || s.stage == Stage::Alert);
    }
    cameras::Status s = m.update(along(LAT, LON, 90, d, 100, t));  // first fix past the first point
    TEST_ASSERT_TRUE(d < 0);
    TEST_ASSERT_EQUAL_INT(1, s.index);
    TEST_ASSERT_TRUE(s.stage == Stage::Alert);
    TEST_ASSERT_UINT16_WITHIN(5, 300 + d, s.distM);  // d < 0: past the first
    s = m.update(along(LAT, LON, 90, d - 28, 100, t + 1000));
    TEST_ASSERT_EQUAL_INT(1, s.index);
}

void test_nearest_point_wins() {
    camlist::Point p[2];
    cameras::Fix far = along(LAT, LON, 0, -150, 0, 0);
    p[0] = pt(far.latE6, far.lonE6, 0, 60, camlist::Kind::Speed);  // 150 m beyond the other
    p[1] = pt(LAT, LON, 0, 40, camlist::Kind::Bump);
    cameras::Matcher m;
    m.load(p, 2);
    cameras::Status s = m.update(along(LAT, LON, 0, 100, 50, 1000));
    TEST_ASSERT_EQUAL_INT(1, s.index);
    TEST_ASSERT_TRUE(s.stage == Stage::Alarm);
}

// --- no fix / slow ---

void test_nofix_after_5s_moving() {
    camlist::Point p = pt(LAT, LON, 270, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    uint32_t t = 1000;
    for (int d = 190; d >= 120; d -= 14, t += 1000)
        TEST_ASSERT_TRUE(m.update(along(LAT, LON, 270, d, 50, t)).stage == Stage::Alert);
    uint32_t lastFix = t - 1000;
    for (uint32_t dt = 1000; dt <= 4000; dt += 1000)
        TEST_ASSERT_TRUE(m.update(noFix(lastFix + dt)).stage == Stage::Alert);
    TEST_ASSERT_TRUE(m.update(noFix(lastFix + 4900)).stage == Stage::Alert);
    cameras::Status s = m.update(noFix(lastFix + 5100));
    TEST_ASSERT_TRUE(s.stage == Stage::NoFix);
    TEST_ASSERT_EQUAL_INT(-1, s.index);
    TEST_ASSERT_TRUE(m.update(noFix(lastFix + 9000)).stage == Stage::NoFix);
    // The fix comes back: matching resumes.
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 270, 100, 50, lastFix + 10000)).stage == Stage::Alert);
}

// Lost while stopped (GPS 2 IP killed while parked, the socket dropped): after NOFIX_MS it says so
// all the same, or the car drives off with the deck pretending the road is clear.
void test_nofix_while_stopped_after_5s() {
    cameras::Matcher m;
    camlist::Point p = pt(LAT, LON, 270, 60, camlist::Kind::Speed);
    m.load(&p, 1);
    m.update(along(LAT, LON, 270, 1000, 5, 20000));
    TEST_ASSERT_TRUE(m.update(noFix(21000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(noFix(24900)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(noFix(25100)).stage == Stage::NoFix);
    TEST_ASSERT_TRUE(m.update(noFix(40000)).stage == Stage::NoFix);
    // The fix comes back (still stopped): Off again.
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 270, 1000, 0, 41000)).stage == Stage::Off);
}

// GPS 2 IP never connected: the first "none" starts the clock (a short grace after boot), then NoFix.
void test_never_had_fix_is_nofix_after_5s() {
    cameras::Matcher m;
    camlist::Point p = pt(LAT, LON, 270, 60, camlist::Kind::Speed);
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(noFix(1000)).stage == Stage::Off);
    TEST_ASSERT_TRUE(m.update(noFix(5900)).stage == Stage::Off);
    cameras::Status s = m.update(noFix(6100));
    TEST_ASSERT_TRUE(s.stage == Stage::NoFix);
    TEST_ASSERT_EQUAL_INT(-1, s.index);
    TEST_ASSERT_TRUE(m.update(noFix(60000)).stage == Stage::NoFix);
    // A new list doesn't hide it: the clock is the feed's, not the list's.
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(noFix(61000)).stage == Stage::NoFix);
}

void test_slow_course_is_ignored() {
    camlist::Point p = pt(LAT, LON, 0, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    cameras::Status s = m.update(along(LAT, LON, 0, 100, 9, 1000));
    TEST_ASSERT_TRUE(s.stage == Stage::Off);
    TEST_ASSERT_EQUAL_INT(-1, s.index);
}

void test_heading_wraps_at_north() {
    camlist::Point p = pt(LAT, LON, 355, 60, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 10, 150, 50, 1000)).stage == Stage::Alert);
}

void test_load_clears_passed() {
    camlist::Point p = pt(LAT, LON, 90, 50, camlist::Kind::Speed);
    cameras::Matcher m;
    m.load(&p, 1);
    m.update(along(LAT, LON, 90, 20, 60, 1000));
    m.update(along(LAT, LON, 90, -30, 60, 2000));  // passed: within 60 m, then further away
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 150, 60, 3000)).stage == Stage::Off);
    m.load(&p, 1);
    TEST_ASSERT_TRUE(m.update(along(LAT, LON, 90, 150, 60, 4000)).stage == Stage::Alert);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_geometry_helpers);
    RUN_TEST(test_alert_at_200m);
    RUN_TEST(test_alert_reaches_further_when_fast);
    RUN_TEST(test_alert_stays_when_braking);
    RUN_TEST(test_redlight_alerts_at_200m_never_alarms);
    RUN_TEST(test_alarm_over_limit_in_last_100m);
    RUN_TEST(test_bump_alarms_too);
    RUN_TEST(test_unknown_limit_never_alarms);
    RUN_TEST(test_opposite_lane_silent);
    RUN_TEST(test_parallel_road_silent);
    RUN_TEST(test_overpass_silent);
    RUN_TEST(test_point_behind_is_silent);
    RUN_TEST(test_passed_clears_and_waits_60s);
    RUN_TEST(test_slow_jitter_does_not_pass);
    RUN_TEST(test_next_point_after_passing);
    RUN_TEST(test_nearest_point_wins);
    RUN_TEST(test_nofix_after_5s_moving);
    RUN_TEST(test_nofix_while_stopped_after_5s);
    RUN_TEST(test_never_had_fix_is_nofix_after_5s);
    RUN_TEST(test_slow_course_is_ignored);
    RUN_TEST(test_heading_wraps_at_north);
    RUN_TEST(test_load_clears_passed);
    return UNITY_END();
}
