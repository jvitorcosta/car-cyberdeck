#include <unity.h>
#include <cstdio>
#include <vector>
#include "../../src/yellow/ui/jpfont.h"

void setUp() {}
void tearDown() {}

// Synthetic blob: U+3042 and U+3044 16 px, U+FF71 8 px.
static std::vector<uint8_t> makeBlob() {
    std::vector<uint8_t> b = {'J', 'F', 'N', 'T', 3, 0};
    for (uint16_t cp : {0x3042, 0x3044, 0xFF71}) { b.push_back(cp & 0xFF); b.push_back(cp >> 8); }
    for (uint8_t w : {16, 16, 8}) b.push_back(w);
    for (int i = 0; i < 3 * 32; ++i) b.push_back((uint8_t)i);
    return b;
}

void test_open_and_lookup() {
    std::vector<uint8_t> b = makeBlob();
    jpfont::Font f;
    TEST_ASSERT_TRUE(f.open(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(3, f.count());
    int w = 0;
    const uint8_t* g = f.glyph(0x3042, &w);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT(16, w);
    TEST_ASSERT_EQUAL_HEX8(0, g[0]);
    g = f.glyph(0x3044, &w);
    TEST_ASSERT_EQUAL_HEX8(32, g[0]);
    g = f.glyph(0xFF71, &w);
    TEST_ASSERT_EQUAL_INT(8, w);
    TEST_ASSERT_EQUAL_HEX8(64, g[0]);
    TEST_ASSERT_EQUAL_INT(16, f.width(0x3044));
    TEST_ASSERT_EQUAL_INT(8, f.width(0xFF71));
}

void test_missing_codepoints() {
    std::vector<uint8_t> b = makeBlob();
    jpfont::Font f;
    f.open(b.data(), b.size());
    int w = 99;
    for (uint32_t cp : {0x3043u, 0x3041u, 0x41u, 0xFF72u, 0x13042u, 0x1F600u}) {
        TEST_ASSERT_NULL(f.glyph(cp, &w));
        TEST_ASSERT_EQUAL_INT(0, f.width(cp));
    }
}

void test_rejects_bad_blobs() {
    std::vector<uint8_t> b = makeBlob();
    jpfont::Font f;
    std::vector<uint8_t> bad = b;
    bad[0] = 'U';
    TEST_ASSERT_FALSE(f.open(bad.data(), bad.size()));  // magic
    TEST_ASSERT_FALSE(f.open(b.data(), b.size() - 1));  // truncated bitmaps
    TEST_ASSERT_FALSE(f.open(b.data(), 8));             // truncated index
    std::vector<uint8_t> unsorted = b;
    unsorted[8] = 0x41;                                 // 2nd codepoint 0x3041 < 0x3042
    TEST_ASSERT_FALSE(f.open(unsorted.data(), unsorted.size()));
    std::vector<uint8_t> width = b;
    width[13] = 12;                                     // a width other than 8 / 16
    TEST_ASSERT_FALSE(f.open(width.data(), width.size()));
    TEST_ASSERT_FALSE(f.open(nullptr, 0));
    TEST_ASSERT_NULL(f.glyph(0x3042, nullptr));         // closed font: no glyphs
    TEST_ASSERT_EQUAL_INT(0, f.count());
}

// The committed data/jp16.bin (tools/gen_jp_font.py): opens, holds the kana, the level 1 kanji of
// "米津玄師" and 々 〆 ヶ, halfwidth katakana 8 px wide; not the Chinese-only 们, Hangul or level 2's 鬱.
void test_the_committed_blob() {
    FILE* fp = fopen("data/jp16.bin", "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, "run from the repo root");
    std::vector<uint8_t> b;
    int c;
    while ((c = fgetc(fp)) != EOF) b.push_back((uint8_t)c);
    fclose(fp);
    jpfont::Font f;
    TEST_ASSERT_TRUE(f.open(b.data(), b.size()));
    TEST_ASSERT_TRUE(b.size() < 130000);
    for (uint32_t cp : {0x3042u, 0x30F3u, 0x7C73u, 0x6D25u, 0x7384u, 0x5E2Bu, 0x3005u, 0x3006u, 0x30F6u, 0x3002u, 0xFF01u})
        TEST_ASSERT_EQUAL_INT_MESSAGE(16, f.width(cp), "wide glyph");
    TEST_ASSERT_EQUAL_INT(8, f.width(0xFF71));  // ｱ
    for (uint32_t cp : {0x4EECu, 0xD55Cu, 0x9B31u}) TEST_ASSERT_EQUAL_INT(0, f.width(cp));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_open_and_lookup);
    RUN_TEST(test_missing_codepoints);
    RUN_TEST(test_rejects_bad_blobs);
    RUN_TEST(test_the_committed_blob);
    return UNITY_END();
}
