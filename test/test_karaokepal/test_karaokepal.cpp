#include <unity.h>
#include "../../src/util/black/karaokepal.h"
#include "../../src/util/common/termpal.h"

void setUp() {}
void tearDown() {}

void test_tint_mixes_per_channel() {
    TEST_ASSERT_EQUAL_HEX32(0x231A16, karaokepal::tint(0xDC6400, 10, 0x0E1218));
}

void test_orange_day() {
    const karaokepal::Pal p = karaokepal::of(1, false);
    TEST_ASSERT_EQUAL_HEX32(0xFFAA64, p.song);
    TEST_ASSERT_EQUAL_HEX32(0xF57814, p.artist);
    TEST_ASSERT_EQUAL_HEX32(0x231A16, p.bg);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, p.lit);
}

void test_orange_night() {
    const karaokepal::Pal p = karaokepal::of(1, true);
    TEST_ASSERT_EQUAL_HEX32(0xFF4600, p.song);
    TEST_ASSERT_EQUAL_HEX32(0xEDE6D6, p.lit);
}

void test_none_is_white() {
    TEST_ASSERT_EQUAL_HEX32(0xD2D2C8, karaokepal::of(-1, false).song);
    TEST_ASSERT_EQUAL_HEX32(0xD2D2C8, karaokepal::of(42, false).song);
}

void test_cyan_follows_blue() {
    TEST_ASSERT_EQUAL_HEX32(karaokepal::of(4, false).song, karaokepal::of(8, false).song);
    TEST_ASSERT_EQUAL_HEX32(karaokepal::of(4, true).bg, karaokepal::of(8, true).bg);
}

// The sung words are the brightest thing on screen, for every light and both modes.
void test_lit_is_brightest() {
    for (int sw = -1; sw <= 8; ++sw) {
        for (int night = 0; night <= 1; ++night) {
            const karaokepal::Pal p = karaokepal::of(sw, night != 0);
            const double lit = termpal::contrastOnBlack(p.lit);
            TEST_ASSERT_TRUE(lit > termpal::contrastOnBlack(p.unsung));
            TEST_ASSERT_TRUE(lit > termpal::contrastOnBlack(p.prev));
            TEST_ASSERT_TRUE(lit > termpal::contrastOnBlack(p.next));
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tint_mixes_per_channel);
    RUN_TEST(test_orange_day);
    RUN_TEST(test_orange_night);
    RUN_TEST(test_none_is_white);
    RUN_TEST(test_cyan_follows_blue);
    RUN_TEST(test_lit_is_brightest);
    return UNITY_END();
}
