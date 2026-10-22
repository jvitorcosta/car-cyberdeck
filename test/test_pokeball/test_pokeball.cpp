#include <unity.h>
#include "../../src/ui/pokeball.h"

using pokeball::Px;
void setUp() {}
void tearDown() {}

static void assertPx(Px want, int x, int y, int frame) {
    TEST_ASSERT_EQUAL_INT((int)want, (int)pokeball::pixel(x, y, frame));
}

void test_upright_ball_red_top_white_bottom() {
    assertPx(Px::Red, 6, 2, 0);
    assertPx(Px::Red, 4, 3, 0);
    assertPx(Px::White, 6, 10, 0);
    assertPx(Px::White, 8, 9, 0);
}
void test_centre_button_and_ring() {
    for (int f = 0; f < pokeball::FRAMES; ++f) {
        assertPx(Px::White, 6, 6, f);   // button never rotates away
        assertPx(Px::Dark, 6, 4, f);    // ring around it
    }
}
void test_outside_is_clear() {
    assertPx(Px::Clear, 0, 0, 0);
    assertPx(Px::Clear, 12, 12, 3);
    assertPx(Px::Clear, 0, 12, 5);
}
void test_quarter_turn_puts_red_on_the_right() {
    const int q = pokeball::FRAMES / 4;
    assertPx(Px::Red, 10, 6, q);
    assertPx(Px::White, 2, 6, q);
    assertPx(Px::Dark, 6, 2, q);        // band is now vertical
}
void test_frames_wrap() {
    for (int y = 0; y < pokeball::SIZE; ++y)
        for (int x = 0; x < pokeball::SIZE; ++x) {
            TEST_ASSERT_EQUAL_INT((int)pokeball::pixel(x, y, 1), (int)pokeball::pixel(x, y, 1 + pokeball::FRAMES));
            TEST_ASSERT_EQUAL_INT((int)pokeball::pixel(x, y, 0), (int)pokeball::pixel(x, y, -pokeball::FRAMES));
        }
}
void test_every_frame_has_both_halves() {
    for (int f = 0; f < pokeball::FRAMES; ++f) {
        int red = 0, white = 0;
        for (int y = 0; y < pokeball::SIZE; ++y)
            for (int x = 0; x < pokeball::SIZE; ++x) {
                Px p = pokeball::pixel(x, y, f);
                red += p == Px::Red;
                white += p == Px::White;
            }
        TEST_ASSERT_TRUE(red >= 15);
        TEST_ASSERT_TRUE(white >= 15);
    }
}
// The ball sits left of the time text; when the time is too wide (episodes of 100 min or more:
// font 2 digits are 8 px) it would leave the area drawProgressRegion clears and cover the name.
void test_ball_hides_when_the_time_is_wide() {
    TEST_ASSERT_EQUAL_INT(210, pokeball::left(304, 77));   // "12:34/12:34", today's case
    TEST_ASSERT_EQUAL_INT(207, pokeball::left(304, 80));
    TEST_ASSERT_EQUAL_INT(-1, pokeball::left(304, 83));    // would be 204
    TEST_ASSERT_EQUAL_INT(-1, pokeball::left(304, 85));    // "10:00/100:00", would be 202
    TEST_ASSERT_EQUAL_INT(-1, pokeball::left(304, 93));    // "100:00/100:00", would be 194
    TEST_ASSERT_EQUAL_INT(pokeball::AREA_X, pokeball::left(304, 304 - pokeball::AREA_X - 4 - pokeball::SIZE));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ball_hides_when_the_time_is_wide);
    RUN_TEST(test_upright_ball_red_top_white_bottom);
    RUN_TEST(test_centre_button_and_ring);
    RUN_TEST(test_outside_is_clear);
    RUN_TEST(test_quarter_turn_puts_red_on_the_right);
    RUN_TEST(test_frames_wrap);
    RUN_TEST(test_every_frame_has_both_halves);
    return UNITY_END();
}
