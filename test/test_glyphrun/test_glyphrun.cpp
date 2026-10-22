#include <unity.h>
#include <cstring>
#include "../../src/util/glyphrun.h"

using glyphrun::Item;
using glyphrun::Kind;
void setUp() {}
void tearDown() {}

// Fake fonts: ASCII 6 px; kana/hanzi 16 px; half-width katakana (U+FF61..FF9F) 8 px.
static int aw(char, void*) { return 6; }
static int ww(uint32_t cp, void*) {
    if (cp >= 0xFF61 && cp <= 0xFF9F) return 8;
    if ((cp >= 0x3000 && cp <= 0x30FF) || (cp >= 0x4E00 && cp <= 0x9FFF)) return 16;
    return 0;
}
static Item buf[128];

void test_decode_mixed_ascii_accent_cjk() {
    // "Aç あ愛" : A, c(cedilla), space, あ, 愛
    size_t n = glyphrun::decode("A\xC3\xA7 \xE3\x81\x82\xE6\x84\x9B", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(5, (int)n);
    TEST_ASSERT_EQUAL_INT((int)Kind::Ascii, (int)buf[1].kind);
    TEST_ASSERT_EQUAL_INT('c', buf[1].ch);
    TEST_ASSERT_EQUAL_INT((int)txt::Mark::Cedilla, (int)buf[1].mark);
    TEST_ASSERT_EQUAL_INT((int)Kind::Wide, (int)buf[3].kind);
    TEST_ASSERT_EQUAL_HEX32(0x3042, buf[3].cp);
    TEST_ASSERT_EQUAL_HEX32(0x611B, buf[4].cp);
    TEST_ASSERT_EQUAL_INT(6 * 3 + 16 * 2, glyphrun::width(buf, n));
}
void test_decode_drops_unsupported() {
    // emoji (4-byte), Hangul (not covered), stray continuation byte
    size_t n = glyphrun::decode("a\xF0\x9F\x98\x80" "b\xEA\xB0\x80" "c\x80" "d", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(4, (int)n);
    TEST_ASSERT_EQUAL_INT('a', buf[0].ch);
    TEST_ASSERT_EQUAL_INT('d', buf[3].ch);
}
void test_decode_half_width_and_upper() {
    size_t n = glyphrun::decode("ab\xEF\xBD\xB1", buf, 128, aw, ww, nullptr, true);   // "abｱ"
    TEST_ASSERT_EQUAL_INT(3, (int)n);
    TEST_ASSERT_EQUAL_INT('A', buf[0].ch);
    TEST_ASSERT_EQUAL_INT(8, buf[2].w);
}
void test_decode_respects_capacity() {
    size_t n = glyphrun::decode("abcdef", buf, 4, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(4, (int)n);
}
void test_fit_appends_ellipsis() {
    size_t n = glyphrun::decode("\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", buf, 128, aw, ww, nullptr);   // 4 x 16 px
    n = glyphrun::fit(buf, n, 128, 50, aw, nullptr);   // 50 px: 1 kana (16) + "..." (18) = 34; 2 kana = 50
    TEST_ASSERT_TRUE(glyphrun::width(buf, n) <= 50);
    TEST_ASSERT_EQUAL_INT('.', buf[n - 1].ch);
    TEST_ASSERT_EQUAL_INT((int)Kind::Wide, (int)buf[0].kind);
}
void test_fit_short_is_untouched() {
    size_t n = glyphrun::decode("hi", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(2, (int)glyphrun::fit(buf, n, 128, 100, aw, nullptr));
}
void test_wrap_ascii_at_space() {
    size_t n = glyphrun::decode("aaaa bbbb cccc", buf, 128, aw, ww, nullptr);   // 6 px each
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 60, 18);   // 10 chars per line
    TEST_ASSERT_EQUAL_INT(9, (int)w.aEnd);                  // "aaaa bbbb"
    TEST_ASSERT_EQUAL_INT(10, (int)w.bStart);               // "cccc"
    TEST_ASSERT_EQUAL_INT(14, (int)w.bEnd);
    TEST_ASSERT_FALSE(w.bEllipsis);
}
void test_wrap_cjk_between_characters() {
    // 10 kana, no spaces; 64 px per line = 4 kana
    size_t n = glyphrun::decode("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A"
                                "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\x8F\xE3\x81\x91\xE3\x81\x93",
                                buf, 128, aw, ww, nullptr);
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 64, 18);
    TEST_ASSERT_EQUAL_INT(4, (int)w.aEnd);
    TEST_ASSERT_EQUAL_INT(4, (int)w.bStart);
    TEST_ASSERT_TRUE(w.bEllipsis);                           // 6 left, only fits 2 + "..."
    TEST_ASSERT_TRUE(16 * (int)(w.bEnd - w.bStart) + 18 <= 64);
}
void test_wrap_single_line() {
    size_t n = glyphrun::decode("short", buf, 128, aw, ww, nullptr);
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 60, 18);
    TEST_ASSERT_EQUAL_INT(5, (int)w.aEnd);
    TEST_ASSERT_EQUAL_INT((int)w.bStart, (int)w.bEnd);       // empty line b
}
// ---- emoji ----
#define FIRE "\xF0\x9F\x94\xA5"
#define HEART "\xE2\x9D\xA4"
#define VS16 "\xEF\xB8\x8F"
#define ZWJ "\xE2\x80\x8D"
#define TONE "\xF0\x9F\x8F\xBD"
#define RI_B "\xF0\x9F\x87\xA7"
#define RI_R "\xF0\x9F\x87\xB7"
static const uint32_t BR = 0x110000 + 1 * 26 + 17;
// Fake emoji font: a handful of keys, 16 px.
static int ew(uint32_t k, void*) {
    switch (k) {
    case 0x1F525: case 0x2764: case 0x1F44D: case 0x1F468: case 0x1F3F3: case 0x1F3F4: case 0x3030:
        return 16;
    default: return k == BR ? 16 : 0;
    }
}
static size_t dec(const char* s) { return glyphrun::decode(s, buf, 128, aw, ww, nullptr, false, ew); }
static void expectEmoji(size_t i, uint32_t key) {
    TEST_ASSERT_EQUAL_INT((int)Kind::Emoji, (int)buf[i].kind);
    TEST_ASSERT_EQUAL_HEX32(key, buf[i].cp);
    TEST_ASSERT_EQUAL_INT(16, buf[i].w);
}
static void expectAscii(size_t i, char c) {
    TEST_ASSERT_EQUAL_INT((int)Kind::Ascii, (int)buf[i].kind);
    TEST_ASSERT_EQUAL_INT(c, buf[i].ch);
}
void test_emoji_decoded() {
    TEST_ASSERT_EQUAL_INT(2, (int)dec("A" FIRE));
    expectAscii(0, 'A');
    expectEmoji(1, 0x1F525);
    TEST_ASSERT_EQUAL_INT(6 + 16, glyphrun::width(buf, 2));
}
void test_emoji_variation_selector_and_skin_tone_dropped() {
    TEST_ASSERT_EQUAL_INT(1, (int)dec(HEART VS16));
    expectEmoji(0, 0x2764);
    TEST_ASSERT_EQUAL_INT(1, (int)dec("\xF0\x9F\x91\x8D" TONE));          // thumbs up, medium tone
    expectEmoji(0, 0x1F44D);
}
void test_emoji_zwj_sequence_shows_its_first_emoji() {
    // family: man ZWJ woman ZWJ girl
    TEST_ASSERT_EQUAL_INT(1, (int)dec("\xF0\x9F\x91\xA8" ZWJ "\xF0\x9F\x91\xA9" ZWJ "\xF0\x9F\x91\xA7"));
    expectEmoji(0, 0x1F468);
    // rainbow flag: white flag VS16 ZWJ rainbow, then "x"
    TEST_ASSERT_EQUAL_INT(2, (int)dec("\xF0\x9F\x8F\xB3" VS16 ZWJ "\xF0\x9F\x8C\x88" "x"));
    expectEmoji(0, 0x1F3F3);
    expectAscii(1, 'x');
    // a ZWJ never swallows ASCII
    TEST_ASSERT_EQUAL_INT(2, (int)dec("a" ZWJ "b"));
    expectAscii(1, 'b');
}
void test_emoji_flags() {
    TEST_ASSERT_EQUAL_HEX32(BR, glyphrun::flagKey(0x1F1E7, 0x1F1F7));
    TEST_ASSERT_EQUAL_INT(1, (int)dec(RI_B RI_R));
    expectEmoji(0, BR);
    TEST_ASSERT_EQUAL_INT(1, (int)dec(RI_B "z"));                         // lone indicator
    expectAscii(0, 'z');
    TEST_ASSERT_EQUAL_INT(0, (int)dec(RI_R RI_B));                        // flag not in the font
}
void test_emoji_keycap_and_tags() {
    TEST_ASSERT_EQUAL_INT(1, (int)dec("1" VS16 "\xE2\x83\xA3"));          // keycap 1
    expectAscii(0, '1');
    // England: black flag + tags g b e n g + cancel tag
    TEST_ASSERT_EQUAL_INT(1, (int)dec("\xF0\x9F\x8F\xB4\xF3\xA0\x81\xA7\xF3\xA0\x81\xA2\xF3\xA0\x81\xA5"
                                      "\xF3\xA0\x81\xAE\xF3\xA0\x81\xA7\xF3\xA0\x81\xBF"));
    expectEmoji(0, 0x1F3F4);
}
void test_emoji_uncovered_or_no_font_dropped() {
    TEST_ASSERT_EQUAL_INT(2, (int)dec("a\xF0\x9F\x98\x80" "b"));          // grinning face: not covered
    expectAscii(1, 'b');
    size_t n = glyphrun::decode("a" FIRE "b", buf, 128, aw, ww, nullptr);   // no emoji font
    TEST_ASSERT_EQUAL_INT(2, (int)n);
    expectAscii(1, 'b');
}
void test_emoji_wins_over_cjk_and_cut_sequence() {
    TEST_ASSERT_EQUAL_INT(1, (int)dec("\xE3\x80\xB0"));                   // wavy dash
    expectEmoji(0, 0x3030);
    size_t n = glyphrun::decode("\xE3\x80\xB0", buf, 128, aw, ww, nullptr);
    TEST_ASSERT_EQUAL_INT(1, (int)n);
    TEST_ASSERT_EQUAL_INT((int)Kind::Wide, (int)buf[0].kind);
    TEST_ASSERT_EQUAL_INT(1, (int)dec("x\xF0\x9F"));                       // emoji cut by the 96 B limit
    expectAscii(0, 'x');
}
void test_emoji_fit_and_wrap() {
    size_t n = dec(FIRE FIRE FIRE FIRE);                                   // 64 px
    n = glyphrun::fit(buf, n, 128, 40, aw, nullptr);                       // 1 emoji + "..." = 34
    TEST_ASSERT_EQUAL_INT(4, (int)n);
    expectEmoji(0, 0x1F525);
    expectAscii(3, '.');
    n = dec("aaaaaa" FIRE "bb");                                           // 36 + 16 + 12 px
    glyphrun::Wrap w = glyphrun::wrapTwo(buf, n, 48, 18);                  // breaks before the emoji
    TEST_ASSERT_EQUAL_INT(6, (int)w.aEnd);
    TEST_ASSERT_EQUAL_INT(6, (int)w.bStart);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_mixed_ascii_accent_cjk);
    RUN_TEST(test_decode_drops_unsupported);
    RUN_TEST(test_decode_half_width_and_upper);
    RUN_TEST(test_decode_respects_capacity);
    RUN_TEST(test_fit_appends_ellipsis);
    RUN_TEST(test_fit_short_is_untouched);
    RUN_TEST(test_wrap_ascii_at_space);
    RUN_TEST(test_wrap_cjk_between_characters);
    RUN_TEST(test_wrap_single_line);
    RUN_TEST(test_emoji_decoded);
    RUN_TEST(test_emoji_variation_selector_and_skin_tone_dropped);
    RUN_TEST(test_emoji_zwj_sequence_shows_its_first_emoji);
    RUN_TEST(test_emoji_flags);
    RUN_TEST(test_emoji_keycap_and_tags);
    RUN_TEST(test_emoji_uncovered_or_no_font_dropped);
    RUN_TEST(test_emoji_wins_over_cjk_and_cut_sequence);
    RUN_TEST(test_emoji_fit_and_wrap);
    return UNITY_END();
}
