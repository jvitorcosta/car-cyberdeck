#include <unity.h>
#include "../../src/util/car/camsign.h"

void setUp() {}
void tearDown() {}

using camlist::Kind;

void test_alert_distance_is_the_matchers() {
    TEST_ASSERT_EQUAL_UINT32(200, cameras::alertDistM(Kind::Speed, 50));    // 166 m < 200
    TEST_ASSERT_EQUAL_UINT32(333, cameras::alertDistM(Kind::Speed, 100));   // 12 s at 100 km/h
    TEST_ASSERT_EQUAL_UINT32(333, cameras::alertDistM(Kind::Bump, 100));
    TEST_ASSERT_EQUAL_UINT32(200, cameras::alertDistM(Kind::RedLight, 120));
}

void test_bar_full_middle_empty() {
    camsign::Countdown c;
    c.start(Kind::Speed, 60, 320);   // 200 m
    TEST_ASSERT_EQUAL_INT(320, c.update(200));
    TEST_ASSERT_EQUAL_INT(160, c.update(100));
    TEST_ASSERT_EQUAL_INT(80, c.update(50));
    TEST_ASSERT_EQUAL_INT(0, c.update(0));
}

void test_bar_never_grows_back() {
    camsign::Countdown c;
    c.start(Kind::Speed, 60, 320);
    TEST_ASSERT_EQUAL_INT(240, c.update(150));
    TEST_ASSERT_EQUAL_INT(240, c.update(175));   // GPS jitter, a nearer point dropped: stays
    TEST_ASSERT_EQUAL_INT(240, c.width());
    TEST_ASSERT_EQUAL_INT(160, c.update(100));
}

void test_bar_full_width_from_when_the_alert_began() {
    camsign::Countdown c;
    c.start(Kind::Speed, 100, 320);   // 333 m at the start...
    TEST_ASSERT_EQUAL_INT(320, c.update(333));
    // ...braking for it doesn't shrink the full width (the matcher's distance would be 200 m now)
    TEST_ASSERT_EQUAL_INT(160, c.update(167));
}

void test_bar_further_than_full_is_full() {
    camsign::Countdown c;
    c.start(Kind::RedLight, 50, 320);   // 200 m
    TEST_ASSERT_EQUAL_INT(320, c.update(600));
}

void test_bar_restarts_full() {
    camsign::Countdown c;
    c.start(Kind::Speed, 60, 320);
    c.update(40);
    c.start(Kind::Bump, 60, 320);   // the next point
    TEST_ASSERT_EQUAL_INT(320, c.width());
    TEST_ASSERT_EQUAL_INT(320, c.update(400));
}

void test_bar_unstarted_is_empty() {
    camsign::Countdown c;
    TEST_ASSERT_EQUAL_INT(0, c.update(100));
}

void test_over_by() {
    TEST_ASSERT_EQUAL_UINT16(12, camsign::overBy(72, 60));
    TEST_ASSERT_EQUAL_UINT16(1, camsign::overBy(61, 60));
    TEST_ASSERT_EQUAL_UINT16(1, camsign::overBy(60, 60));   // in an alarm: never under +1
    TEST_ASSERT_EQUAL_UINT16(1, camsign::overBy(55, 60));   // the speed fell since the stage
    TEST_ASSERT_EQUAL_UINT16(0, camsign::overBy(90, camlist::NO_LIMIT));
    TEST_ASSERT_EQUAL_UINT16(camsign::MAX_OVER, camsign::overBy(2000, 20));
}

void test_blink_2hz_red_first() {
    TEST_ASSERT_TRUE(camsign::redFace(1000, 1000));    // the alarm's first look is the red face
    TEST_ASSERT_TRUE(camsign::redFace(1000, 1249));
    TEST_ASSERT_FALSE(camsign::redFace(1000, 1250));
    TEST_ASSERT_FALSE(camsign::redFace(1000, 1499));
    TEST_ASSERT_TRUE(camsign::redFace(1000, 1500));
    TEST_ASSERT_TRUE(camsign::redFace(0xFFFFFF80u, 0x00000010u));   // millis() wraps: 144 ms in
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_alert_distance_is_the_matchers);
    RUN_TEST(test_bar_full_middle_empty);
    RUN_TEST(test_bar_never_grows_back);
    RUN_TEST(test_bar_full_width_from_when_the_alert_began);
    RUN_TEST(test_bar_further_than_full_is_full);
    RUN_TEST(test_bar_restarts_full);
    RUN_TEST(test_bar_unstarted_is_empty);
    RUN_TEST(test_over_by);
    RUN_TEST(test_blink_2hz_red_first);
    return UNITY_END();
}
