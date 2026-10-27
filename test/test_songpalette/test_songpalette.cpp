#include <unity.h>
#include "../../src/util/common/rgb565.h"
#include "../../src/util/wire/songpalette.h"

using songpalette::Rgb;

void setUp() {}
void tearDown() {}

namespace {
int hueOf(Rgb c) {   // degrees 0..359
    int mx = c.r, mn = c.r;
    if (c.g > mx) mx = c.g;
    if (c.b > mx) mx = c.b;
    if (c.g < mn) mn = c.g;
    if (c.b < mn) mn = c.b;
    int d = mx - mn;
    if (!d) return 0;
    int h = mx == c.r ? 60 * (c.g - c.b) / d : mx == c.g ? 120 + 60 * (c.b - c.r) / d : 240 + 60 * (c.r - c.g) / d;
    return (h + 360) % 360;
}
int dist(int a, int b) { int d = ((a - b) % 360 + 360) % 360; return d > 180 ? 360 - d : d; }
uint16_t px(uint8_t r, uint8_t g, uint8_t b) { return rgb565::be(rgb565::rgb(r, g, b)); }   // as the art buffer stores it
constexpr int W = 92, H = 92;
uint16_t bmp[W * H];
void fill(uint16_t left, uint16_t right) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) bmp[y * W + x] = x < W / 2 ? left : right;
}
}  // namespace

void test_cover_red_blue() {
    fill(px(255, 0, 0), px(0, 0, 255));
    Rgb out[3];
    TEST_ASSERT_EQUAL_INT(2, songpalette::fromCover(bmp, W, H, out));
    bool red = false, blue = false;
    for (int i = 0; i < 2; ++i) {
        if (dist(hueOf(out[i]), 0) <= 8) red = true;
        if (dist(hueOf(out[i]), 240) <= 8) blue = true;
    }
    TEST_ASSERT_TRUE(red);
    TEST_ASSERT_TRUE(blue);
}
void test_cover_none_without_art() {
    Rgb out[3];
    TEST_ASSERT_EQUAL_INT(0, songpalette::fromCover(nullptr, W, H, out));
    TEST_ASSERT_EQUAL_INT(0, songpalette::fromCover(bmp, 0, H, out));
    TEST_ASSERT_EQUAL_INT(0, songpalette::fromCover(bmp, W, -1, out));
}
void test_cover_skips_dark_and_washed_out() {
    fill(px(10, 10, 10), px(250, 250, 250));   // near black, near white
    Rgb out[3];
    TEST_ASSERT_EQUAL_INT(0, songpalette::fromCover(bmp, W, H, out));
}
// The hour sets only the cap and the pulse depth.
void test_time_of_day() {
    struct Case { int minute; uint8_t cap, depth; };
    const Case cases[] = {
        {720, 100, 60}, {-1, 100, 60}, {360, 100, 60},           // day, clock not synced
        {20 * 60, 70, 45}, {1080, 70, 45}, {5 * 60 + 30, 70, 45}, // evening, before dawn
        {23 * 60 + 30, 45, 30}, {1320, 45, 30}, {0, 45, 30},      // late
    };
    for (const Case& c : cases) {
        uint8_t cap = 0, depth = 0;
        songpalette::timeOfDay(c.minute, &cap, &depth);
        TEST_ASSERT_EQUAL_UINT8(c.cap, cap);
        TEST_ASSERT_EQUAL_UINT8(c.depth, depth);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cover_red_blue);
    RUN_TEST(test_cover_none_without_art);
    RUN_TEST(test_cover_skips_dark_and_washed_out);
    RUN_TEST(test_time_of_day);
    return UNITY_END();
}
