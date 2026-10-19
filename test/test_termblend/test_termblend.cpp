#include <unity.h>
#include <vector>
#include "../../src/util/termblend.h"
#include "../../src/util/rgb565.h"
#include "../../src/util/bootanim.h"

void setUp() {}
void tearDown() {}

static const uint16_t VIDEO = rgb565::be(0xFFFF);   // a white frame, big-endian like JPEGDEC's output
static const uint16_t GLYPH = rgb565::be(0x07E0);

// A 4x2 overlay at (10, 20): glyph at (11, 20), KEY elsewhere.
static std::vector<uint16_t> overlay() {
    std::vector<uint16_t> o(8, termblend::KEY);
    o[1] = GLYPH;
    return o;
}

void test_glyph_replaces_and_panel_darkens() {
    std::vector<uint16_t> b(8 * 4, VIDEO);              // block 8x4 at (8, 18)
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 8, 18, 8, 4, o.data(), 10, 20, 4, 2);
    TEST_ASSERT_EQUAL_HEX16(GLYPH, b[(20 - 18) * 8 + (11 - 8)]);
    const uint16_t dark = rgb565::be(termblend::shade(0xFFFF));
    TEST_ASSERT_EQUAL_HEX16(dark, b[(20 - 18) * 8 + (10 - 8)]);
    TEST_ASSERT_EQUAL_HEX16(dark, b[(21 - 18) * 8 + (13 - 8)]);
}

// Review focus 5: a block half over the panel changes only the overlapping pixels.
void test_block_partly_over_the_panel() {
    std::vector<uint16_t> b(8 * 4, VIDEO);
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 8, 18, 8, 4, o.data(), 10, 20, 4, 2);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 8; ++x) {
            const int sx = 8 + x, sy = 18 + y;
            const bool in = sx >= 10 && sx < 14 && sy >= 20 && sy < 22;
            if (!in) TEST_ASSERT_EQUAL_HEX16(VIDEO, b[y * 8 + x]);
            else TEST_ASSERT_NOT_EQUAL(VIDEO, b[y * 8 + x]);
        }
}

void test_block_outside_is_untouched() {
    std::vector<uint16_t> b(16 * 16, VIDEO);
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 100, 100, 16, 16, o.data(), 10, 20, 4, 2);   // below right
    termblend::composite(b.data(), 0, 0, 8, 8, o.data(), 10, 20, 4, 2);         // above left (8x8 view)
    for (uint16_t v : b) TEST_ASSERT_EQUAL_HEX16(VIDEO, v);
}

// The panel is 3/8 of the video, by shifts (a per-channel multiply cost 5 ms a frame on the deck).
void test_shade_is_three_eighths_per_channel() {
    TEST_ASSERT_EQUAL_HEX16(0x52CA, termblend::shade(0xFFFF));   // R 10/31, G 22/63, B 10/31
    TEST_ASSERT_EQUAL_HEX16(0x5000, termblend::shade(0xF800));   // red stays red: no bleed
    TEST_ASSERT_EQUAL_HEX16(0x0000, termblend::shade(0x0000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_glyph_replaces_and_panel_darkens);
    RUN_TEST(test_block_partly_over_the_panel);
    RUN_TEST(test_block_outside_is_untouched);
    RUN_TEST(test_shade_is_three_eighths_per_channel);
    return UNITY_END();
}
