#include <unity.h>
#include <initializer_list>
#include "../../src/util/lights/ledintro.h"
#include "../../src/util/yellow/elkcmd.h"

using songpalette::Rgb;
using namespace ledintro;

void setUp() {}
void tearDown() {}

static const Rgb PURPLE{160, 0, 255};

static Rgb atPct(Rgb s, int pct) {
    const uint8_t p = static_cast<uint8_t>(pct);
    return {elkcmd::dim(s.r, p), elkcmd::dim(s.g, p), elkcmd::dim(s.b, p)};
}

static void assertRgb(Rgb want, Rgb got) {
    TEST_ASSERT_EQUAL_UINT8(want.r, got.r);
    TEST_ASSERT_EQUAL_UINT8(want.g, got.g);
    TEST_ASSERT_EQUAL_UINT8(want.b, got.b);
}

void test_peak_lifts_the_level_up_to_full() {
    TEST_ASSERT_EQUAL_INT(70, peakPct(40));
    TEST_ASSERT_EQUAL_INT(100, peakPct(70));
    TEST_ASSERT_EQUAL_INT(100, peakPct(100));
}

void test_both_tracks_start_dark_and_end_on_the_look_twice() {
    for (bool inside : {true, false}) {
        const Track t = plan(inside, PURPLE, 50);
        TEST_ASSERT_TRUE(t.n >= 4);
        TEST_ASSERT_EQUAL_UINT32(0, t.k[0].at);
        assertRgb({0, 0, 0}, t.k[0].c);
        TEST_ASSERT_FALSE(t.k[0].fill);
        for (int i : {t.n - 1, t.n - 2}) {
            assertRgb(atPct(PURPLE, 50), t.k[i].c);
            TEST_ASSERT_FALSE(t.k[i].fill);  // a colour frame: it also ends the effect
        }
    }
}

void test_inside_fills_for_4_s_then_takes_the_look() {
    const Track t = plan(true, PURPLE, 50);
    TEST_ASSERT_EQUAL_INT(4, t.n);
    TEST_ASSERT_TRUE(t.k[1].fill);
    assertRgb(atPct(PURPLE, 50), t.k[1].c);  // what a takeover fades from while it fills
    TEST_ASSERT_EQUAL_UINT32(FILL_MS, t.k[2].at - t.k[1].at);
}

void test_only_the_inside_light_runs_the_effect() {
    const Track t = plan(false, PURPLE, 50);
    for (int i = 0; i < t.n; i++) TEST_ASSERT_FALSE(t.k[i].fill);
}

void test_steps_keep_each_controllers_write_rate() {
    for (bool inside : {true, false}) {
        const Track t = plan(inside, PURPLE, 30);
        const uint32_t gap = inside ? IN_GAP_MS : OUT_GAP_MS;
        for (int i = 1; i < t.n; i++) TEST_ASSERT_TRUE(t.k[i].at - t.k[i - 1].at >= gap);
    }
}

void test_outside_peaks_as_the_fill_ends() {
    const Track in = plan(true, PURPLE, 60), out = plan(false, PURPLE, 60);
    TEST_ASSERT_EQUAL_UINT32(in.k[2].at, out.k[2].at);  // the inside look and the outside peak together
    assertRgb(atPct(PURPLE, 90), out.k[2].c);
    TEST_ASSERT_TRUE(length(out) <= 6500);
    TEST_ASSERT_EQUAL_UINT32(0, length(Track()));
}

void test_the_outside_hue_never_shifts() {
    const Rgb orange{255, 80, 0};  // every key is the swatch scaled: green never above red's ratio
    const Track t = plan(false, orange, 40);
    for (int i = 0; i < t.n; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, t.k[i].c.b);
        TEST_ASSERT_TRUE(t.k[i].c.g <= t.k[i].c.r);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_peak_lifts_the_level_up_to_full);
    RUN_TEST(test_both_tracks_start_dark_and_end_on_the_look_twice);
    RUN_TEST(test_inside_fills_for_4_s_then_takes_the_look);
    RUN_TEST(test_only_the_inside_light_runs_the_effect);
    RUN_TEST(test_steps_keep_each_controllers_write_rate);
    RUN_TEST(test_outside_peaks_as_the_fill_ends);
    RUN_TEST(test_the_outside_hue_never_shifts);
    return UNITY_END();
}
