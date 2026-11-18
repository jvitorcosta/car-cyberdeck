#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "../../src/util/car/sukihud.h"

using sukihud::Rect;

void setUp() {}
void tearDown() {}

static void le16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void le32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }
static uint16_t be(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static const uint16_t BG = 0x1234, INK = 0xF800;   // native RGB565: the background, a red glyph

// A whole file: the background BG everywhere but pixel (x 5, y 7) = 0x0001; every font has two
// glyphs, '1' (2 px wide, solid INK) and '.' (1 px wide, alpha 128), cell height 3, tracking 1.
static std::vector<uint8_t> file() {
    std::vector<uint8_t> b = {'S', 'U', 'K', '1'};
    le16(b, sukihud::W);
    le16(b, sukihud::H);
    le16(b, sukihud::FONTS);
    le16(b, 0);
    for (int y = 0; y < sukihud::H; ++y)
        for (int x = 0; x < sukihud::W; ++x) {
            const uint16_t c = (x == 5 && y == 7) ? 0x0001 : BG;
            b.push_back(c >> 8);   // big-endian
            b.push_back(c & 0xFF);
        }
    std::vector<size_t> fix;   // where each glyph's offset goes
    for (int f = 0; f < sukihud::FONTS; ++f) {
        b.push_back(3); b.push_back(1); b.push_back(2); b.push_back(0);
        const char ch[2] = {'1', '.'};
        const uint8_t w[2] = {2, 1};
        for (int g = 0; g < 2; ++g) {
            b.push_back((uint8_t)ch[g]); b.push_back(w[g]); le16(b, 0);
            fix.push_back(b.size());
            le32(b, 0);
        }
    }
    for (size_t i = 0; i < fix.size(); ++i) {
        const uint32_t off = (uint32_t)b.size();
        for (int k = 0; k < 4; ++k) b[fix[i] + k] = (off >> (8 * k)) & 0xFF;
        const bool one = (i % 2) == 0;
        const int px = one ? 2 * 3 : 1 * 3;
        for (int p = 0; p < px; ++p) { b.push_back(INK >> 8); b.push_back(INK & 0xFF); b.push_back(one ? 255 : 128); }
    }
    return b;
}

void test_parse_takes_a_whole_file_only() {
    std::vector<uint8_t> f = file();
    sukihud::Art a;
    TEST_ASSERT_TRUE(sukihud::parse(f.data(), f.size(), &a));
    TEST_ASSERT_EQUAL_UINT8(3, a.font[sukihud::F_SPEED].h);
    TEST_ASSERT_FALSE(sukihud::parse(f.data(), f.size() - 1, &a));   // the last glyph cut short
    TEST_ASSERT_FALSE(sukihud::parse(f.data(), 100, &a));
    TEST_ASSERT_FALSE(sukihud::parse(nullptr, 0, &a));
    std::vector<uint8_t> g = f;
    g[0] = 'X';
    TEST_ASSERT_FALSE(sukihud::parse(g.data(), g.size(), &a));
    g = f;
    g[4] = 0x41;   // 321 wide: not this screen
    g[5] = 0x01;
    TEST_ASSERT_FALSE(sukihud::parse(g.data(), g.size(), &a));
}

void test_width_counts_tracking_and_skips_unknown_chars() {
    std::vector<uint8_t> f = file();
    sukihud::Art a;
    sukihud::parse(f.data(), f.size(), &a);
    TEST_ASSERT_EQUAL_INT(2, sukihud::width(a, sukihud::F_BAR, "1"));
    TEST_ASSERT_EQUAL_INT(2 + 1 + 1, sukihud::width(a, sukihud::F_BAR, "1."));
    TEST_ASSERT_EQUAL_INT(2 + 1 + 2, sukihud::width(a, sukihud::F_BAR, "1x1"));
    TEST_ASSERT_EQUAL_INT(0, sukihud::width(a, sukihud::F_BAR, ""));
    // tracking is signed: -1 lets the glyphs' edges overlap
    std::vector<uint8_t> g = f;
    g[sukihud::HEAD + (size_t)sukihud::W * sukihud::H * 2 + 1] = 0xFF;   // the first font's tracking
    sukihud::parse(g.data(), g.size(), &a);
    TEST_ASSERT_EQUAL_INT(2 + 2 - 1, sukihud::width(a, sukihud::F_SMALL, "11"));
}

void test_background_copies_the_rect() {
    std::vector<uint8_t> f = file();
    sukihud::Art a;
    sukihud::parse(f.data(), f.size(), &a);
    uint16_t buf[4 * 3];
    sukihud::background(a, Rect{4, 6, 4, 3}, buf);
    TEST_ASSERT_EQUAL_HEX16(be(BG), buf[0]);
    TEST_ASSERT_EQUAL_HEX16(be(0x0001), buf[1 * 4 + 1]);   // (5, 7)
}

void test_text_blends_and_clips() {
    std::vector<uint8_t> f = file();
    sukihud::Art a;
    sukihud::parse(f.data(), f.size(), &a);
    const Rect r{10, 10, 6, 3};
    uint16_t buf[6 * 3];
    sukihud::background(a, r, buf);
    sukihud::text(a, sukihud::F_RPM, r, buf, 10, 11, "1.", sukihud::Align::Left);
    TEST_ASSERT_EQUAL_HEX16(be(INK), buf[0]);   // '1' solid
    TEST_ASSERT_EQUAL_HEX16(be(INK), buf[2 * 6 + 1]);
    TEST_ASSERT_EQUAL_HEX16(be(BG), buf[2]);    // the tracking gap
    const uint16_t half = be(buf[3]);           // '.' at alpha 128: half way from BG to INK
    TEST_ASSERT_INT_WITHIN(1, ((BG >> 11) + 31 + 1) / 2, half >> 11);
    TEST_ASSERT_EQUAL_HEX16(be(BG), buf[4]);
    // right-aligned at x 16, and a text hanging out of the rect is clipped, not written past it
    uint16_t big[8 * 3 + 4];
    for (int i = 0; i < 8 * 3 + 4; ++i) big[i] = 0xAAAA;
    const Rect s{10, 10, 8, 3};
    sukihud::text(a, sukihud::F_RPM, s, big, 18, 11, "1111111", sukihud::Align::Right);
    for (int i = 8 * 3; i < 8 * 3 + 4; ++i) TEST_ASSERT_EQUAL_HEX16(0xAAAA, big[i]);
    TEST_ASSERT_EQUAL_HEX16(be(INK), big[7]);   // the last column: the last '1' ends at 17
}

void test_capsule_is_a_pill_with_a_core() {
    const Rect r{0, 0, 7, 12};
    uint16_t buf[7 * 12];
    for (auto& p : buf) p = 0;
    sukihud::capsule(r, buf, 3, 0, 11, 5, 0x1111, 0x2222);
    TEST_ASSERT_EQUAL_HEX16(0, buf[0 * 7 + 0]);        // the corner stays background
    TEST_ASSERT_EQUAL_HEX16(0x1111, buf[6 * 7 + 1]);   // the rim, mid height
    TEST_ASSERT_EQUAL_HEX16(0x2222, buf[6 * 7 + 3]);   // the core
    TEST_ASSERT_EQUAL_HEX16(0, buf[6 * 7 + 6]);        // past the right edge
    sukihud::capsule(r, buf, 3, 0, 11, 5, 0x3333, 0);  // no core
    TEST_ASSERT_EQUAL_HEX16(0x3333, buf[6 * 7 + 3]);
    sukihud::capsule(r, buf, 20, 0, 11, 5, 0x4444, 0);   // outside: nothing written, no crash
}

void test_tach_and_load() {
    TEST_ASSERT_EQUAL_INT(0, sukihud::tachLit(NAN));
    TEST_ASSERT_EQUAL_INT(0, sukihud::tachLit(-5.0f));
    TEST_ASSERT_EQUAL_INT(10, sukihud::tachLit(3500.0f));
    TEST_ASSERT_EQUAL_INT(sukihud::TACH_N, sukihud::tachLit(9000.0f));
    TEST_ASSERT_FALSE(sukihud::tachHot(16));
    TEST_ASSERT_TRUE(sukihud::tachHot(17));   // 6000 of 7000: from capsule 17
    TEST_ASSERT_TRUE(sukihud::tachX(sukihud::TACH_N - 1) + 3 <= sukihud::TACH.x + sukihud::TACH.w);
    TEST_ASSERT_TRUE(sukihud::tachTop(sukihud::TACH_N - 1) >= sukihud::TACH.y);
    TEST_ASSERT_TRUE(sukihud::tachTop(0) > sukihud::tachTop(sukihud::TACH_N - 1));
    TEST_ASSERT_EQUAL_INT(0, sukihud::loadFill(NAN));
    TEST_ASSERT_EQUAL_INT(15, sukihud::loadFill(50.0f));
    TEST_ASSERT_EQUAL_INT(sukihud::LOAD_H, sukihud::loadFill(140.0f));
}

void test_regions_stay_on_screen_and_fit_the_buffer() {
    const Rect all[] = {sukihud::WAVE, sukihud::BAT, sukihud::THR, sukihud::H2O, sukihud::IAT, sukihud::CLOCK,
                        sukihud::STAGE3, sukihud::STAGE2, sukihud::STAGE1, sukihud::SPEED, sukihud::TACH,
                        sukihud::RPM, sukihud::LOAD};
    for (const Rect& r : all) {
        TEST_ASSERT_TRUE(r.x >= 0 && r.y >= 0 && r.x + r.w <= sukihud::W && r.y + r.h <= sukihud::H);
        TEST_ASSERT_TRUE(r.w * r.h <= sukihud::MAX_PIXELS);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_takes_a_whole_file_only);
    RUN_TEST(test_width_counts_tracking_and_skips_unknown_chars);
    RUN_TEST(test_background_copies_the_rect);
    RUN_TEST(test_text_blends_and_clips);
    RUN_TEST(test_capsule_is_a_pill_with_a_core);
    RUN_TEST(test_tach_and_load);
    RUN_TEST(test_regions_stay_on_screen_and_fit_the_buffer);
    return UNITY_END();
}
