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
        assertRgb(atPct(PURPLE, 50), t.k[t.n - 1].c);
        assertRgb(atPct(PURPLE, 50), t.k[t.n - 2].c);
    }
}

void test_inside_swells_past_the_look_then_settles() {
    const Track t = plan(true, PURPLE, 50);
    int top = 0;
    for (int i = 1; i < t.n; i++) {
        if (t.k[i].c.b > t.k[top].c.b) top = i;
    }
    assertRgb(atPct(PURPLE, 80), t.k[top].c);
    for (int i = 1; i <= top; i++) TEST_ASSERT_TRUE(t.k[i].c.b >= t.k[i - 1].c.b);  // rises
    for (int i = top + 1; i < t.n; i++) TEST_ASSERT_TRUE(t.k[i].c.b <= t.k[i - 1].c.b);  // settles
}

void test_steps_keep_each_controllers_write_rate() {
    for (bool inside : {true, false}) {
        const Track t = plan(inside, PURPLE, 30);
        const uint32_t gap = inside ? IN_GAP_MS : OUT_GAP_MS;
        for (int i = 1; i < t.n; i++) TEST_ASSERT_TRUE(t.k[i].at - t.k[i - 1].at >= gap);
    }
}

void test_outside_waits_for_the_inside_peak_and_the_whole_wave_fits_5_s() {
    const Track in = plan(true, PURPLE, 60), out = plan(false, PURPLE, 60);
    TEST_ASSERT_EQUAL_UINT32(OUT_DELAY_MS, out.k[1].at);
    TEST_ASSERT_TRUE(length(in) >= 2000 && length(in) <= 3000);
    TEST_ASSERT_TRUE(length(out) >= 3000 && length(out) <= 5000);
    TEST_ASSERT_EQUAL_UINT32(0, length(Track()));
}

void test_the_hue_never_shifts() {
    const Rgb orange{255, 80, 0};  // every key is the swatch scaled: green never above red's ratio
    for (bool inside : {true, false}) {
        const Track t = plan(inside, orange, 40);
        for (int i = 0; i < t.n; i++) {
            TEST_ASSERT_EQUAL_UINT8(0, t.k[i].c.b);
            TEST_ASSERT_TRUE(t.k[i].c.g <= t.k[i].c.r);
        }
    }
}

void test_a_full_look_still_rises_and_holds() {
    const Track t = plan(true, PURPLE, 100);
    assertRgb(PURPLE, t.k[5].c);  // the top of the rise is the look itself
    for (int i = 5; i < t.n; i++) assertRgb(PURPLE, t.k[i].c);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_peak_lifts_the_level_up_to_full);
    RUN_TEST(test_both_tracks_start_dark_and_end_on_the_look_twice);
    RUN_TEST(test_inside_swells_past_the_look_then_settles);
    RUN_TEST(test_steps_keep_each_controllers_write_rate);
    RUN_TEST(test_outside_waits_for_the_inside_peak_and_the_whole_wave_fits_5_s);
    RUN_TEST(test_the_hue_never_shifts);
    RUN_TEST(test_a_full_look_still_rises_and_holds);
    return UNITY_END();
}
