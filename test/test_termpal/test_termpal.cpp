#include <unity.h>
#include <initializer_list>
#include "../../src/util/yellow/termpal.h"

using namespace termpal;

void setUp() {}
void tearDown() {}

static uint32_t red(uint32_t c) { return (c >> 16) & 0xFF; }
static uint32_t green(uint32_t c) { return (c >> 8) & 0xFF; }
static uint32_t blue(uint32_t c) { return c & 0xFF; }
static bool near(double want, double got, double tol) { return got > want - tol && got < want + tol; }

void test_values() {
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG));
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, rgb(TEXT));
    TEST_ASSERT_EQUAL_HEX32(0x5ABE28, rgb(MID));
    TEST_ASSERT_EQUAL_HEX32(0x308A24, rgb(DIM));
    TEST_ASSERT_EQUAL_HEX32(0xFF3C28, rgb(ALERT));
    TEST_ASSERT_EQUAL_HEX32(0x030503, scan());
}

// WCAG 2.x: black on black is 1:1, white 21:1, and known greys and greens give their known figures.
void test_contrast_on_black() {
    TEST_ASSERT_TRUE(near(1.0, contrastOnBlack(0x000000), 1e-9));
    TEST_ASSERT_TRUE(near(21.0, contrastOnBlack(0xFFFFFF), 1e-9));
    TEST_ASSERT_TRUE(near(4.69, contrastOnBlack(0x777777), 0.01));
    TEST_ASSERT_TRUE(near(15.30, contrastOnBlack(0x00FF00), 0.01));
    TEST_ASSERT_TRUE(contrastOnBlack(0x0000FF) < contrastOnBlack(0xFF0000));  // blue weighs least
}

// Read at a glance in a moving car: every role at least 4.5:1 on black (DIM included), and the three
// brightnesses keep their order.
void test_contrast_thresholds() {
    for (int r = TEXT; r <= ALERT; r++) TEST_ASSERT_TRUE(contrastOnBlack(rgb(static_cast<Role>(r))) >= 4.5);
    TEST_ASSERT_TRUE(contrastOnBlack(rgb(TEXT)) > contrastOnBlack(rgb(MID)));
    TEST_ASSERT_TRUE(contrastOnBlack(rgb(MID)) > contrastOnBlack(rgb(DIM)));
}

// The hues: the darker greens carry little blue (the panel turns a blue-green to teal); ALERT is red
// and a hue apart from TEXT.
void test_hues() {
    for (Role r : {MID, DIM}) TEST_ASSERT_TRUE(blue(rgb(r)) < green(rgb(r)) / 2);
    TEST_ASSERT_TRUE(green(rgb(TEXT)) > red(rgb(TEXT)));
    const uint32_t a = rgb(ALERT);
    TEST_ASSERT_TRUE(a != rgb(TEXT));
    TEST_ASSERT_TRUE(red(a) > green(a));
}

// ---- Themes: the terminal in the INSIDE light's colour (a ledlook swatch index, -1 the default) ----
static constexpr int N_THEME = 8;  // red, orange, pink, purple, blue, green, white, yellow (ledlook's order)
static constexpr int RED_T = 0, ORANGE_T = 1, PINK_T = 2, PURPLE_T = 3, BLUE_T = 4, GREEN_T = 5, WHITE_T = 6, YELLOW_T = 7;
static bool warmAlertTheme(int t) { return t == RED_T || t == ORANGE_T || t == PINK_T; }
static uint32_t chanDiff(uint32_t a, uint32_t b) {
    uint32_t d = 0;
    for (int s = 0; s <= 16; s += 8) {
        const uint32_t x = (a >> s) & 0xFF, y = (b >> s) & 0xFF, e = x > y ? x - y : y - x;
        if (e > d) d = e;
    }
    return d;
}

// The default theme is today's palette, whatever asks for it (-1, or an index out of range).
void test_theme_default() {
    for (int r = BG; r <= ALERT; r++) {
        const Role role = static_cast<Role>(r);
        TEST_ASSERT_EQUAL_HEX32(rgb(role), rgb(role, -1));
        TEST_ASSERT_EQUAL_HEX32(rgb(role), rgb(role, N_THEME));
        TEST_ASSERT_EQUAL_HEX32(rgb(role), rgb(role, -2));
    }
}

// The owner's fixed points: green is the Seegson green, the warm themes' problems are white, and the
// white theme is held under full white so it doesn't glare.
void test_theme_fixed() {
    for (int r = TEXT; r <= ALERT; r++) {
        const Role role = static_cast<Role>(r);
        TEST_ASSERT_EQUAL_HEX32(rgb(role), rgb(role, GREEN_T));
    }
    for (int t : {RED_T, ORANGE_T, PINK_T}) TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, rgb(ALERT, t));
    TEST_ASSERT_TRUE(contrastOnBlack(rgb(TEXT, WHITE_T)) < 15.0);
}

// Every theme: BG black; TEXT >= 7:1, MID, DIM and ALERT >= 4.5:1; TEXT > MID > DIM.
void test_theme_contrast() {
    for (int t = -1; t < N_THEME; t++) {
        TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, t));
        const double text = contrastOnBlack(rgb(TEXT, t)), mid = contrastOnBlack(rgb(MID, t)),
                     dim = contrastOnBlack(rgb(DIM, t)), alert = contrastOnBlack(rgb(ALERT, t));
        TEST_ASSERT_TRUE_MESSAGE(text >= 7.0, "TEXT contrast");
        TEST_ASSERT_TRUE_MESSAGE(mid >= 4.5, "MID contrast");
        TEST_ASSERT_TRUE_MESSAGE(dim >= 4.5, "DIM contrast");
        TEST_ASSERT_TRUE_MESSAGE(alert >= 4.5, "ALERT contrast");
        TEST_ASSERT_TRUE_MESSAGE(text > mid && mid > dim, "TEXT > MID > DIM");
    }
}

// Each theme reads as its light's colour (TEXT, MID, DIM by the order of their channels): white a
// neutral grey-white.
void test_theme_hues() {
    for (Role role : {TEXT, MID, DIM}) {
        uint32_t c = rgb(role, RED_T);
        TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && red(c) > blue(c) && red(c) >= 0xC0, "red");
        c = rgb(role, ORANGE_T);
        TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && green(c) > blue(c), "orange");
        c = rgb(role, PINK_T);
        TEST_ASSERT_TRUE_MESSAGE(red(c) > blue(c) && blue(c) > green(c), "pink");
        c = rgb(role, PURPLE_T);
        TEST_ASSERT_TRUE_MESSAGE(blue(c) > red(c) && red(c) > green(c), "purple");
        c = rgb(role, BLUE_T);
        TEST_ASSERT_TRUE_MESSAGE(blue(c) > green(c) && green(c) > red(c), "blue");
        c = rgb(role, GREEN_T);
        TEST_ASSERT_TRUE_MESSAGE(green(c) > red(c) && red(c) > blue(c), "green");
        c = rgb(role, YELLOW_T);
        TEST_ASSERT_TRUE_MESSAGE(red(c) >= green(c) && green(c) > 2 * blue(c), "yellow");
        c = rgb(role, WHITE_T);
        TEST_ASSERT_TRUE_MESSAGE(chanDiff(c, (red(c) << 16) | (red(c) << 8) | red(c)) <= 0x10, "white: grey");
    }
}

// The panel's lesson: dark greens carry little blue (it turns them teal).
void test_theme_panel_guards() {
    for (int t : {-1, GREEN_T}) {
        for (Role role : {MID, DIM}) TEST_ASSERT_TRUE_MESSAGE(blue(rgb(role, t)) < green(rgb(role, t)) / 2, "teal");
    }
}

// ALERT: white on the red / orange / pink themes, red on the rest; and always clearly apart from the
// theme's TEXT, a channel at least 0x60 apart.
void test_theme_alert() {
    for (int t = -1; t < N_THEME; t++) {
        const uint32_t a = rgb(ALERT, t);
        if (t >= 0 && warmAlertTheme(t)) {
            TEST_ASSERT_TRUE_MESSAGE(red(a) == green(a) && green(a) == blue(a) && red(a) >= 0xB0, "white alert");
        } else {
            TEST_ASSERT_TRUE_MESSAGE(red(a) >= 0xF0 && green(a) * 3 < red(a) && blue(a) * 3 < red(a), "red alert");
        }
        TEST_ASSERT_TRUE_MESSAGE(chanDiff(a, rgb(TEXT, t)) >= 0x60, "ALERT apart from TEXT");
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_values);
    RUN_TEST(test_contrast_on_black);
    RUN_TEST(test_contrast_thresholds);
    RUN_TEST(test_hues);
    RUN_TEST(test_theme_default);
    RUN_TEST(test_theme_fixed);
    RUN_TEST(test_theme_contrast);
    RUN_TEST(test_theme_hues);
    RUN_TEST(test_theme_panel_guards);
    RUN_TEST(test_theme_alert);
    return UNITY_END();
}
