#include <unity.h>
#include "../../src/util/black/karaokepal.h"
#include "../../src/util/common/termpal.h"

void setUp() {}
void tearDown() {}

void test_tint_mixes_per_channel() {
    TEST_ASSERT_EQUAL_HEX32(0x231A16, karaokepal::tint(0xDC6400, 10, 0x0E1218));
}

// The yellow deck's look: black, the theme's MID on the song, DIM on the artist and the other lines,
// TEXT on the sung words and the played bar.
void test_orange_day_is_the_yellow_decks() {
    const karaokepal::Pal p = karaokepal::of(1, false);
    TEST_ASSERT_EQUAL_HEX32(0x000000, p.bg);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::MID, 1, false), p.song);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::DIM, 1, false), p.artist);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::TEXT, 1, false), p.lit);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::TEXT, 1, false), p.barOn);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::DIM, 1, false), p.barOff);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::DIM, 1, false), p.unsung);
    TEST_ASSERT_EQUAL_HEX32(termpal::rgb(termpal::DIM, 1, false), p.next);
}

void test_orange_night_is_vivid() {
    const karaokepal::Pal p = karaokepal::of(1, true);
    TEST_ASSERT_EQUAL_HEX32(0x000000, p.bg);
    TEST_ASSERT_EQUAL_HEX32(0xFF4600, p.lit);
    TEST_ASSERT_EQUAL_HEX32(0xE03400, p.song);
}

// No light (off, no link, none sent): the yellow deck's default green.
void test_none_is_the_default_green() {
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, karaokepal::of(-1, false).lit);
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, karaokepal::of(42, false).lit);
}

void test_cyan_follows_blue() {
    TEST_ASSERT_EQUAL_HEX32(karaokepal::of(4, false).lit, karaokepal::of(8, false).lit);
    TEST_ASSERT_EQUAL_HEX32(karaokepal::of(4, true).unsung, karaokepal::of(8, true).unsung);
}

// The sweep shows: the sung words clearly brighter than the rest, for every theme and both modes
// (night yellow is one colour at all three levels).
void test_lit_is_brightest() {
    for (int sw = -1; sw <= 8; ++sw) {
        for (int night = 0; night <= 1; ++night) {
            const karaokepal::Pal p = karaokepal::of(sw, night != 0);
            const double lit = termpal::contrastOnBlack(p.lit);
            TEST_ASSERT_TRUE(lit >= 1.5 * termpal::contrastOnBlack(p.unsung));
            TEST_ASSERT_TRUE(lit >= 1.5 * termpal::contrastOnBlack(p.prev));
            TEST_ASSERT_TRUE(lit >= 1.5 * termpal::contrastOnBlack(p.next));
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tint_mixes_per_channel);
    RUN_TEST(test_orange_day_is_the_yellow_decks);
    RUN_TEST(test_orange_night_is_vivid);
    RUN_TEST(test_none_is_the_default_green);
    RUN_TEST(test_cyan_follows_blue);
    RUN_TEST(test_lit_is_brightest);
    return UNITY_END();
}
