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
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, false));
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, rgb(TEXT, false));
    TEST_ASSERT_EQUAL_HEX32(0x5ABE28, rgb(MID, false));
    TEST_ASSERT_EQUAL_HEX32(0x308A24, rgb(DIM, false));
    TEST_ASSERT_EQUAL_HEX32(0xFF3C28, rgb(ALERT, false));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, true));
    TEST_ASSERT_EQUAL_HEX32(0x50FF46, rgb(TEXT, true));
    TEST_ASSERT_EQUAL_HEX32(0x46E03C, rgb(MID, true));
    TEST_ASSERT_EQUAL_HEX32(0x3ABC32, rgb(DIM, true));
    TEST_ASSERT_EQUAL_HEX32(0xFF0000, rgb(ALERT, true));
    TEST_ASSERT_EQUAL_HEX32(0x030503, scan(false));
    TEST_ASSERT_EQUAL_HEX32(0x030503, scan(true));
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
// night (the default's phosphor green) too; and the three brightnesses keep their order.
void test_contrast_thresholds() {
    for (int night = 0; night < 2; night++) {
        for (int r = TEXT; r <= ALERT; r++) {
            TEST_ASSERT_TRUE_MESSAGE(contrastOnBlack(rgb(static_cast<Role>(r), night)) >= 4.5, night ? "night role" : "day role");
        }
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(TEXT, night)) > contrastOnBlack(rgb(MID, night)));
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(MID, night)) > contrastOnBlack(rgb(DIM, night)));
    }
}

// The hues: green day and night, the darker greens carrying little blue (the panel turns a
// blue-green to teal); ALERT is red in both modes and a hue apart from TEXT.
void test_hues() {
    for (int night = 0; night < 2; night++) {
        for (Role r : {MID, DIM}) TEST_ASSERT_TRUE(blue(rgb(r, night)) < green(rgb(r, night)) / 2);
        TEST_ASSERT_TRUE(green(rgb(TEXT, night)) > red(rgb(TEXT, night)));
        const uint32_t a = rgb(ALERT, night);
        TEST_ASSERT_TRUE(a != rgb(TEXT, night));
        TEST_ASSERT_TRUE(red(a) > green(a));
    }
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
    for (int night = 0; night < 2; night++) {
        for (int r = BG; r <= ALERT; r++) {
            const Role role = static_cast<Role>(r);
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, -1));
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, N_THEME));
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, -2));
        }
    }
}

// The owner's fixed points: green is the green theme day and night, the warm themes' problems are
// white, and the night colours picked on the panel (2026-11-05).
void test_theme_fixed() {
    for (int night = 0; night < 2; night++) {
        for (int r = TEXT; r <= ALERT; r++) {
            const Role role = static_cast<Role>(r);
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, GREEN_T));
        }
        for (int t : {RED_T, ORANGE_T, PINK_T}) TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, rgb(ALERT, night, t));
    }
    static const uint32_t PICKED[N_THEME][3] = {
        {0xFF0000, 0xE00000, 0xBC0000}, {0xFF4600, 0xE03400, 0xBC2800}, {0xFF0040, 0xE00038, 0xBC0030},
        {0xD200FF, 0xB800E0, 0x9A00BC}, {0x0046FF, 0x003CE0, 0x0032BC}, {0x50FF46, 0x46E03C, 0x3ABC32},
        {0xFFFFFF, 0xEBEBEB, 0xD2D2D2}, {0xFFB400, 0xFFB400, 0xFFB400},
    };
    for (int t = 0; t < N_THEME; t++) {
        for (int k = 0; k < 3; k++) TEST_ASSERT_EQUAL_HEX32(PICKED[t][k], rgb(static_cast<Role>(TEXT + k), true, t));
    }
}

// Every theme: BG black; by day TEXT >= 7:1, MID and DIM >= 4.5:1, TEXT > MID > DIM; at night (a
// fully saturated red or blue can't reach more) TEXT >= 3:1, DIM >= 2:1, TEXT >= MID >= DIM (the
// yellow is one colour); ALERT >= 4.5:1.
void test_theme_contrast() {
    for (int t = -1; t < N_THEME; t++) {
        for (int night = 0; night < 2; night++) {
            TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, night, t));
            const double text = contrastOnBlack(rgb(TEXT, night, t)), mid = contrastOnBlack(rgb(MID, night, t)),
                         dim = contrastOnBlack(rgb(DIM, night, t)), alert = contrastOnBlack(rgb(ALERT, night, t));
            TEST_ASSERT_TRUE_MESSAGE(text >= (night ? 3.0 : 7.0), "TEXT contrast");
            TEST_ASSERT_TRUE_MESSAGE(dim >= (night ? 2.0 : 4.5), "DIM contrast");
            TEST_ASSERT_TRUE_MESSAGE(alert >= 4.5, "ALERT contrast");
            if (night) TEST_ASSERT_TRUE_MESSAGE(text >= mid && mid >= dim, "TEXT >= MID >= DIM");
            else TEST_ASSERT_TRUE_MESSAGE(mid >= 4.5 && text > mid && mid > dim, "TEXT > MID > DIM");
        }
    }
}

// Each theme reads as its light's colour (TEXT, MID, DIM by the order of their channels): white a
// neutral grey-white; at night fully saturated, the weakest channel at most 0x46 (white aside).
void test_theme_hues() {
    for (int night = 0; night < 2; night++) {
        for (Role role : {TEXT, MID, DIM}) {
            uint32_t c = rgb(role, night, RED_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && red(c) > blue(c) && red(c) >= 0xB0, "red");
            c = rgb(role, night, ORANGE_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && green(c) > blue(c), "orange");
            c = rgb(role, night, PINK_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > blue(c) && blue(c) > green(c), "pink");
            c = rgb(role, night, PURPLE_T);
            TEST_ASSERT_TRUE_MESSAGE(blue(c) > red(c) && red(c) > green(c), "purple");
            c = rgb(role, night, BLUE_T);
            TEST_ASSERT_TRUE_MESSAGE(blue(c) > green(c) && green(c) > red(c), "blue");
            c = rgb(role, night, GREEN_T);
            TEST_ASSERT_TRUE_MESSAGE(green(c) > red(c) && red(c) > blue(c), "green");
            c = rgb(role, night, YELLOW_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) >= green(c) && green(c) > 2 * blue(c), "yellow");
            c = rgb(role, night, WHITE_T);
            TEST_ASSERT_TRUE_MESSAGE(chanDiff(c, (red(c) << 16) | (red(c) << 8) | red(c)) <= 0x10, "white: grey");
            for (int t = 0; night && t < N_THEME; t++) {
                if (t == WHITE_T) continue;
                c = rgb(role, true, t);
                uint32_t lo = red(c);
                if (green(c) < lo) lo = green(c);
                if (blue(c) < lo) lo = blue(c);
                TEST_ASSERT_TRUE_MESSAGE(lo <= 0x46, "night: saturated");
            }
        }
    }
}

// The panel's lessons: dark greens carry little blue (it turns them teal); at night it pushes green,
// so the red carries none (or it reads orange) and the yellow's darker levels stay its TEXT (a darker
// yellow reads olive).
void test_theme_panel_guards() {
    for (int t : {-1, GREEN_T}) {
        for (int night = 0; night < 2; night++) {
            for (Role role : {MID, DIM}) TEST_ASSERT_TRUE_MESSAGE(blue(rgb(role, night, t)) < green(rgb(role, night, t)) / 2, "teal");
        }
    }
    for (Role role : {TEXT, MID, DIM}) {
        TEST_ASSERT_TRUE_MESSAGE(green(rgb(role, true, RED_T)) == 0, "red: no green");
        TEST_ASSERT_EQUAL_HEX32(rgb(TEXT, true, YELLOW_T), rgb(role, true, YELLOW_T));
    }
}

// ALERT: white on the red / orange / pink themes, red on the rest; and always clearly apart from the
// theme's TEXT, a channel at least 0x60 apart.
void test_theme_alert() {
    for (int t = -1; t < N_THEME; t++) {
        for (int night = 0; night < 2; night++) {
            const uint32_t a = rgb(ALERT, night, t), tx = rgb(TEXT, night, t);
            if (t >= 0 && warmAlertTheme(t)) {
                TEST_ASSERT_TRUE_MESSAGE(red(a) == green(a) && green(a) == blue(a) && red(a) >= 0xB0, "white alert");
            } else {
                TEST_ASSERT_TRUE_MESSAGE(red(a) >= 0xF0 && green(a) * 3 < red(a) && blue(a) * 3 < red(a), "red alert");
            }
            TEST_ASSERT_TRUE_MESSAGE(chanDiff(a, tx) >= 0x60, "ALERT apart from TEXT");
        }
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
