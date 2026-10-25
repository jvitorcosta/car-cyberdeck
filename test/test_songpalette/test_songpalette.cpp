#include <unity.h>
#include "../../src/util/genre.h"
#include "../../src/util/rgb565.h"
#include "../../src/util/songpalette.h"

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
bool same(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
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
void test_grey_cover_uses_genre() {
    fill(px(128, 128, 128), px(128, 128, 128));
    Rgb out[3];
    int n = songpalette::fromCover(bmp, W, H, out);
    TEST_ASSERT_EQUAL_INT(0, n);
    songpalette::Palette p = songpalette::build(out, n, 7, 720);
    Rgb fam[3];
    songpalette::family(7, fam);
    for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(same(fam[i], p.c[i]));
}
void test_no_genre_no_cover_neutral() {
    songpalette::Palette p = songpalette::build(nullptr, 0, genre::NONE, 720);
    Rgb neutral[3], q[3];
    songpalette::family(30, neutral);
    songpalette::family(99, q);
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(same(neutral[i], p.c[i]));
        TEST_ASSERT_TRUE(same(neutral[i], q[i]));
    }
}
void test_families_differ() {
    Rgb a[3], b[3];
    songpalette::family(6, a);    // rock
    songpalette::family(9, b);    // electro
    TEST_ASSERT_FALSE(same(a[0], b[0]));
    songpalette::family(0, a);    // pop
    songpalette::family(1, b);    // k-pop: same family
    TEST_ASSERT_TRUE(same(a[0], b[0]));
}
void test_nudge_toward_genre() {
    Rgb orange = {255, 128, 0};   // hue ~30
    Rgb other = {0, 255, 0};      // second colour so the cover counts
    Rgb cover[2] = {orange, other};
    songpalette::Palette p = songpalette::build(cover, 2, 9, 720);   // electro: blue, purple, cyan
    Rgb fam[3];
    songpalette::family(9, fam);
    int h0 = hueOf(orange), h1 = hueOf(p.c[0]);
    TEST_ASSERT_TRUE(dist(h0, h1) <= 16);
    TEST_ASSERT_TRUE(dist(h0, h1) >= 10);
    int nearest = 0, bestD = 999;
    for (int k = 0; k < 3; ++k)
        if (dist(h0, hueOf(fam[k])) < bestD) { bestD = dist(h0, hueOf(fam[k])); nearest = hueOf(fam[k]); }
    TEST_ASSERT_TRUE(dist(h1, nearest) < dist(h0, nearest));   // moved toward it, by the 15 deg cap
    // a hue ~8 deg from a family hue lands on it, not past it (the step is the whole distance)
    Rgb close = {178, 0, 255};    // hue ~281; electro's purple {160, 32, 255} is ~274
    TEST_ASSERT_TRUE(dist(hueOf(close), hueOf(fam[1])) >= 6 && dist(hueOf(close), hueOf(fam[1])) <= 9);
    Rgb c2[2] = {close, other};
    songpalette::Palette q = songpalette::build(c2, 2, 9, 720);
    TEST_ASSERT_TRUE(dist(hueOf(q.c[0]), hueOf(fam[1])) <= 1);
}
void test_one_cover_colour_uses_genre() {
    Rgb cover[1] = {{255, 0, 0}};
    songpalette::Palette p = songpalette::build(cover, 1, 9, 720);
    Rgb fam[3];
    songpalette::family(9, fam);
    for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(same(p.c[i], fam[i]));
}
void test_two_cover_colours_fill_third_from_genre() {
    Rgb cover[2] = {{255, 0, 0}, {0, 255, 0}};   // red ~0, green ~120
    songpalette::Palette p = songpalette::build(cover, 2, 9, 720);   // electro: blue ~221, purple ~274, cyan ~188
    Rgb fam[3];
    songpalette::family(9, fam);
    // the two cover hues, nudged (red toward purple to ~345, green toward cyan to ~135)...
    TEST_ASSERT_TRUE(dist(hueOf(p.c[0]), 345) <= 2);
    TEST_ASSERT_TRUE(dist(hueOf(p.c[1]), 135) <= 2);
    // ...and the family colour farthest from both fills the third slot: blue (86 deg away; purple 71, cyan 53)
    TEST_ASSERT_TRUE(same(p.c[2], fam[0]));
}
void test_time_of_day() {
    Rgb blue = {0, 80, 255}, red = {255, 0, 0};
    Rgb cover[2] = {blue, red};
    songpalette::Palette day = songpalette::build(cover, 2, genre::NONE, 720);
    TEST_ASSERT_EQUAL_UINT8(100, day.cap);
    TEST_ASSERT_EQUAL_UINT8(60, day.depth);
    songpalette::Palette eve = songpalette::build(cover, 2, genre::NONE, 20 * 60);
    TEST_ASSERT_EQUAL_UINT8(70, eve.cap);
    TEST_ASSERT_EQUAL_UINT8(45, eve.depth);
    TEST_ASSERT_TRUE(hueOf(eve.c[0]) > hueOf(day.c[0]));   // toward purple
    TEST_ASSERT_TRUE(hueOf(eve.c[0]) <= hueOf(day.c[0]) + 16);
    songpalette::Palette late = songpalette::build(cover, 2, genre::NONE, 23 * 60 + 30);
    TEST_ASSERT_EQUAL_UINT8(45, late.cap);
    TEST_ASSERT_EQUAL_UINT8(30, late.depth);
    TEST_ASSERT_EQUAL_INT(hueOf(eve.c[0]), hueOf(late.c[0]));
    songpalette::Palette early = songpalette::build(cover, 2, genre::NONE, 5 * 60 + 30);
    TEST_ASSERT_EQUAL_UINT8(70, early.cap);
    songpalette::Palette unsynced = songpalette::build(cover, 2, genre::NONE, -1);
    TEST_ASSERT_EQUAL_UINT8(100, unsynced.cap);
    TEST_ASSERT_EQUAL_UINT8(60, unsynced.depth);
}
void test_whites_turn_amber_at_night() {
    songpalette::Palette day = songpalette::build(nullptr, 0, 21, 720);     // jazz family has a warm white
    songpalette::Palette night = songpalette::build(nullptr, 0, 21, 23 * 60);
    TEST_ASSERT_TRUE(same(day.c[1], (Rgb{255, 214, 160})));
    TEST_ASSERT_TRUE(same(night.c[1], (Rgb{255, 176, 0})));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cover_red_blue);
    RUN_TEST(test_cover_none_without_art);
    RUN_TEST(test_cover_skips_dark_and_washed_out);
    RUN_TEST(test_grey_cover_uses_genre);
    RUN_TEST(test_no_genre_no_cover_neutral);
    RUN_TEST(test_families_differ);
    RUN_TEST(test_nudge_toward_genre);
    RUN_TEST(test_one_cover_colour_uses_genre);
    RUN_TEST(test_two_cover_colours_fill_third_from_genre);
    RUN_TEST(test_time_of_day);
    RUN_TEST(test_whites_turn_amber_at_night);
    return UNITY_END();
}
