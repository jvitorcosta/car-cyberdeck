#include <unity.h>
#include <initializer_list>
#include "../../src/util/termpal.h"

using namespace termpal;

void setUp() {}
void tearDown() {}

static uint32_t luma(uint32_t c) { return (77 * ((c >> 16) & 0xFF) + 151 * ((c >> 8) & 0xFF) + 28 * (c & 0xFF)) >> 8; }
static uint32_t red(uint32_t c) { return (c >> 16) & 0xFF; }
static uint32_t green(uint32_t c) { return (c >> 8) & 0xFF; }
static uint32_t blue(uint32_t c) { return c & 0xFF; }
static bool near(double want, double got, double tol) { return got > want - tol && got < want + tol; }

void test_values() {
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, false));
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, rgb(TEXT, false));
    TEST_ASSERT_EQUAL_HEX32(0x5ABE28, rgb(MID, false));
    TEST_ASSERT_EQUAL_HEX32(0x308A24, rgb(DIM, false));
    TEST_ASSERT_EQUAL_HEX32(0xFF3C28, rgb(ALERT, false));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, true));
    TEST_ASSERT_EQUAL_HEX32(0xE09A00, rgb(TEXT, true));
    TEST_ASSERT_EQUAL_HEX32(0xA06E00, rgb(MID, true));
    TEST_ASSERT_EQUAL_HEX32(0x7A5400, rgb(DIM, true));
    TEST_ASSERT_EQUAL_HEX32(0xE03418, rgb(ALERT, true));
    TEST_ASSERT_EQUAL_HEX32(0x030503, scan(false));
    TEST_ASSERT_EQUAL_HEX32(0x050300, scan(true));
    TEST_ASSERT_EQUAL_HEX32(0xFF0090, tile(0xFF0090, false));
    TEST_ASSERT_EQUAL_HEX32(0xB20064, tile(0xFF0090, true));
}

// WCAG 2.x: black on black is 1:1, white 21:1, and known greys and greens give their known figures.
void test_contrast_on_black() {
    TEST_ASSERT_TRUE(near(1.0, contrastOnBlack(0x000000), 1e-9));
    TEST_ASSERT_TRUE(near(21.0, contrastOnBlack(0xFFFFFF), 1e-9));
    TEST_ASSERT_TRUE(near(4.69, contrastOnBlack(0x777777), 0.01));
    TEST_ASSERT_TRUE(near(15.30, contrastOnBlack(0x00FF00), 0.01));
    TEST_ASSERT_TRUE(contrastOnBlack(0x0000FF) < contrastOnBlack(0xFF0000));  // blue weighs least
}

// Read at a glance in a moving car: by day every role at least 4.5:1 on black (DIM included), at
// night (a dimmer backlight on top) at least 3:1; and the three brightnesses keep their order.
void test_contrast_thresholds() {
    for (int night = 0; night < 2; night++) {
        const double least = night ? 3.0 : 4.5;
        for (int r = TEXT; r <= ALERT; r++) {
            TEST_ASSERT_TRUE_MESSAGE(contrastOnBlack(rgb(static_cast<Role>(r), night)) >= least, night ? "night role" : "day role");
        }
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(TEXT, night)) > contrastOnBlack(rgb(MID, night)));
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(MID, night)) > contrastOnBlack(rgb(DIM, night)));
    }
}

// The hues: night is amber (no role with more blue than red) and dimmer than day; the day's darker
// greens carry little blue (the panel turns a blue-green to teal); ALERT is red in both modes and a
// hue apart from TEXT.
void test_hues() {
    for (int r = BG; r <= ALERT; r++) {
        const uint32_t n = rgb(static_cast<Role>(r), true);
        TEST_ASSERT_TRUE(blue(n) <= red(n));
        if (r != BG) TEST_ASSERT_TRUE(luma(n) < luma(rgb(static_cast<Role>(r), false)));
    }
    for (Role r : {MID, DIM}) TEST_ASSERT_TRUE(blue(rgb(r, false)) < green(rgb(r, false)) / 2);
    TEST_ASSERT_TRUE(green(rgb(TEXT, false)) > red(rgb(TEXT, false)));
    for (int night = 0; night < 2; night++) {
        const uint32_t a = rgb(ALERT, night);
        TEST_ASSERT_TRUE(a != rgb(TEXT, night));
        TEST_ASSERT_TRUE(red(a) > green(a));
    }
    TEST_ASSERT_TRUE(luma(rgb(ALERT, true)) < luma(rgb(ALERT, false)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_values);
    RUN_TEST(test_contrast_on_black);
    RUN_TEST(test_contrast_thresholds);
    RUN_TEST(test_hues);
    return UNITY_END();
}
