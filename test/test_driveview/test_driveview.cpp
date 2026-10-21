#include <unity.h>
#include <cmath>
#include "../../src/util/driveview.h"

using driveview::Mode;
using driveview::Pin;
using driveview::Show;
void setUp() {}
void tearDown() {}

void test_no_car_is_the_menu() {
    Mode m;
    TEST_ASSERT_TRUE(m.update(Pin::Auto, false, 80, 0) == Show::Menu);
    TEST_ASSERT_TRUE(m.update(Pin::Drive, false, 80, 0) == Show::Menu);
    TEST_ASSERT_TRUE(m.update(Pin::Lyrics, false, 0, 0) == Show::Menu);
}

// Auto: the cluster while moving, the lyrics after STOP_MS at 0 km/h, the cluster again on pulling away.
void test_auto_follows_the_speed() {
    Mode m;
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 60, 1000) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 2000) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 2000 + driveview::STOP_MS - 1) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 2000 + driveview::STOP_MS) == Show::Stopped);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 3, 6000) == Show::Stopped);      // creeping
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 12, 7000) == Show::Cluster);     // pulling away
}

// A short stop in traffic doesn't flip the screen.
void test_auto_stop_and_go_stays_on_the_cluster() {
    Mode m;
    m.update(Pin::Auto, true, 20, 0);
    m.update(Pin::Auto, true, 0, 1000);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 2, 2500) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 3000) == Show::Cluster);      // the stop restarts
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 5999) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 6000) == Show::Stopped);
}

// No fresh speed: the view stays as it is.
void test_unknown_speed_keeps_the_view() {
    Mode m;
    m.update(Pin::Auto, true, 0, 0);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, driveview::STOP_MS) == Show::Stopped);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, NAN, 60000) == Show::Stopped);
    Mode n;
    TEST_ASSERT_TRUE(n.update(Pin::Auto, true, NAN, 60000) == Show::Cluster);   // starts on the cluster
}

void test_pins_override_the_speed() {
    Mode m;
    TEST_ASSERT_TRUE(m.update(Pin::Drive, true, 0, 0) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Drive, true, 0, 60000) == Show::Cluster);
    TEST_ASSERT_TRUE(m.update(Pin::Lyrics, true, 100, 61000) == Show::Stopped);
}

// The car leaving resets the stop: the next drive starts on the cluster.
void test_car_leaving_resets() {
    Mode m;
    m.update(Pin::Auto, true, 0, 0);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, driveview::STOP_MS) == Show::Stopped);
    m.update(Pin::Auto, false, NAN, 10000);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, NAN, 20000) == Show::Cluster);
}

// The stop clock is wrap-safe.
void test_stop_across_millis_wrap() {
    Mode m;
    m.update(Pin::Auto, true, 0, 0xFFFFF000u);
    TEST_ASSERT_TRUE(m.update(Pin::Auto, true, 0, 0xFFFFF000u + driveview::STOP_MS) == Show::Stopped);
}

void test_names() {
    Pin p = Pin::Drive;
    TEST_ASSERT_TRUE(driveview::parse("auto", &p) && p == Pin::Auto);
    TEST_ASSERT_TRUE(driveview::parse("drive", &p) && p == Pin::Drive);
    TEST_ASSERT_TRUE(driveview::parse("lyrics", &p) && p == Pin::Lyrics);
    TEST_ASSERT_FALSE(driveview::parse("AUTO", &p));
    TEST_ASSERT_FALSE(driveview::parse("", &p));
    TEST_ASSERT_FALSE(driveview::parse(nullptr, &p));
    TEST_ASSERT_TRUE(p == Pin::Lyrics);                                          // untouched
    TEST_ASSERT_EQUAL_STRING("menu", driveview::name(Show::Menu));
    TEST_ASSERT_EQUAL_STRING("drive", driveview::name(Show::Cluster));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_car_is_the_menu);
    RUN_TEST(test_auto_follows_the_speed);
    RUN_TEST(test_auto_stop_and_go_stays_on_the_cluster);
    RUN_TEST(test_unknown_speed_keeps_the_view);
    RUN_TEST(test_pins_override_the_speed);
    RUN_TEST(test_car_leaving_resets);
    RUN_TEST(test_stop_across_millis_wrap);
    RUN_TEST(test_names);
    return UNITY_END();
}
