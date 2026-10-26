#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/ui/lyricrun.h"
#include "../../src/util/text.h"

using lyricrun::Item;
using lyricrun::Kind;
using lyricrun::Span;
using termfont::Face;

void setUp() {}
void tearDown() {}

// A stand-in for data/jp16.bin: the kana, 米津玄師 and the halfwidth katakana.
static int fakeFont(uint32_t cp, const void*) {
    if (cp >= 0x3041 && cp <= 0x30FF) return 16;
    if (cp == 0x7C73 || cp == 0x6D25 || cp == 0x7384 || cp == 0x5E2B) return 16;  // 米津玄師
    if (cp >= 0xFF61 && cp <= 0xFF9F) return 8;
    return 0;
}

static constexpr size_t CAP = 160;
static Item items[CAP];

static size_t dec(const char* s, bool upper = false) { return lyricrun::decode(s, items, CAP, fakeFont, nullptr, upper); }

// The Latin-1 bytes of items[0..n) ('#' for a Wide item, '?' for a Box).
static std::string bytes(size_t from, size_t to, bool dots = false) {
    std::string r;
    for (size_t i = from; i < to; i++)
        r += items[i].kind == Kind::Latin1 ? static_cast<char>(items[i].cp) : items[i].kind == Kind::Wide ? '#' : '?';
    return dots ? r + ".." : r;
}

// Latin-1 text decodes to exactly txt::latin1Fold's bytes: accents, the degree sign, the folded
// quotes, dashes, ellipsis and spaces; emoji dropped.
void test_latin1_unchanged() {
    for (const char* s : {"Cora\xC3\xA7\xC3\xA3o de estudante", "na\xC3\xAFve \xE2\x80\x98quote\xE2\x80\x99 \xE2\x80\x94 end\xE2\x80\xA6",
                          "25\xC2\xB0", "\xC5\x93uvre\xC2\xA0x", "fire \xF0\x9F\x94\xA5 here", "ab\xE3\x80\x80" "cd", "bad\xE3\x81"}) {
        char folded[CAP];
        txt::latin1Fold(s, folded, sizeof(folded));
        const size_t n = dec(s);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(folded, bytes(0, n).c_str(), s);
    }
}

void test_japanese_items() {
    size_t n = dec("Lemon \xE7\xB1\xB3\xE6\xB4\xA5\xE7\x8E\x84\xE5\xB8\xAB");  // Lemon 米津玄師
    TEST_ASSERT_EQUAL_INT(10, n);
    TEST_ASSERT_EQUAL_STRING("Lemon ####", bytes(0, n).c_str());
    TEST_ASSERT_EQUAL_HEX32(0x7C73, items[6].cp);
    TEST_ASSERT_EQUAL_INT(16, items[6].w);
    // a missing kanji (the Chinese-only 们) and Hangul (한) are boxes; the ideographic space a space
    n = dec("\xE4\xBB\xAC\xE3\x80\x80\xED\x95\x9C");
    TEST_ASSERT_EQUAL_STRING("? ?", bytes(0, n).c_str());
    TEST_ASSERT_EQUAL_INT(lyricrun::BOX_W, items[0].w);
    // halfwidth katakana: 8 px; an emoji between is dropped
    n = dec("\xEF\xBD\xB1\xF0\x9F\x94\xA5\xE3\x81\x82");  // ｱ🔥あ
    TEST_ASSERT_EQUAL_STRING("##", bytes(0, n).c_str());
    TEST_ASSERT_EQUAL_INT(8, items[0].w);
    TEST_ASSERT_EQUAL_INT(16, items[1].w);
    // no font: CJK is all boxes
    n = lyricrun::decode("\xE3\x81\x82x", items, CAP, nullptr, nullptr);
    TEST_ASSERT_EQUAL_STRING("?x", bytes(0, n).c_str());
}

void test_upper_and_cap() {
    size_t n = dec("lemon a\xC3\xA7\xC3\xA3o \xC3\xBF\xE3\x81\x82", true);
    TEST_ASSERT_EQUAL_STRING("LEMON A\xC7\xC3O Y#", bytes(0, n).c_str());
    // a folded character goes in whole: "..." doesn't fit the 2 items left
    Item few[4];
    n = lyricrun::decode("ab\xE2\x80\xA6", few, 4, fakeFont, nullptr);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(0, lyricrun::decode(nullptr, few, 4, fakeFont, nullptr));
}

void test_isCjk() {
    for (uint32_t cp : {0x3042u, 0x4E00u, 0x9FFFu, 0x3400u, 0xAC00u, 0xD7A3u, 0xF900u, 0xFF01u, 0x20000u, 0x3105u})
        TEST_ASSERT_TRUE(lyricrun::isCjk(cp));
    for (uint32_t cp : {0x41u, 0xE9u, 0x2026u, 0x1F525u, 0x0410u}) TEST_ASSERT_FALSE(lyricrun::isCjk(cp));
}

// Pure Latin-1 measures as termfont does; a Wide glyph is 16 px, 32 in BIG.
void test_width() {
    for (Face f : {Face::SMALL, Face::BODY, Face::TITLE, Face::BIG}) {
        const char* s = "Hello, S\xC3\xA3o Jo\xC3\xA3o!";
        char folded[64];
        txt::latin1Fold(s, folded, sizeof(folded));
        const size_t n = dec(s);
        TEST_ASSERT_EQUAL_INT(termfont::width(f, folded), lyricrun::width(f, items, n));
        TEST_ASSERT_EQUAL_INT(termfont::width(f, "Hello.."), lyricrun::width(f, items, 5, true));
    }
    TEST_ASSERT_EQUAL_INT(2, lyricrun::scale(Face::BIG));
    TEST_ASSERT_EQUAL_INT(1, lyricrun::scale(Face::TITLE));
    TEST_ASSERT_EQUAL_INT(1, lyricrun::scale(Face::BODY));
    const size_t n = dec("ab\xE3\x81\x82\xE4\xBB\xAC");  // ab あ 们
    TEST_ASSERT_EQUAL_INT(termfont::advance(Face::BIG, "ab") + 64, lyricrun::width(Face::BIG, items, n));
    TEST_ASSERT_EQUAL_INT(termfont::advance(Face::BODY, "ab") + 32, lyricrun::width(Face::BODY, items, n));
    TEST_ASSERT_EQUAL_INT(32, lyricrun::advance(Face::BIG, items[3]));  // the box: one wide cell
}

// Wide glyphs centred on the caps: BIG's 32 rows, the others' 16.
void test_glyph_top() {
    TEST_ASSERT_EQUAL_INT(100 - 26, lyricrun::glyphTop(Face::BIG, 100));    // (21 + 32) / 2
    TEST_ASSERT_EQUAL_INT(100 - 13, lyricrun::glyphTop(Face::BODY, 100));   // (11 + 16) / 2
    TEST_ASSERT_EQUAL_INT(100 - 15, lyricrun::glyphTop(Face::TITLE, 100));  // (14 + 16) / 2
}

static Span lines[4];

// A pure-Japanese line wraps between characters: BIG's 32 px glyphs, 9 to a 312 px line.
void test_wrap_japanese_between_characters() {
    // 12 hiragana: あいうえおかきくけこさし
    const size_t n = dec("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A\xE3\x81\x8B"
                         "\xE3\x81\x8D\xE3\x81\x8F\xE3\x81\x91\xE3\x81\x93\xE3\x81\x95\xE3\x81\x97");
    TEST_ASSERT_EQUAL_INT(12, n);
    bool cut = true;
    TEST_ASSERT_EQUAL_INT(2, lyricrun::wrap(items, n, Face::BIG, 312, 3, lines, &cut));
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(0, lines[0].start);
    TEST_ASSERT_EQUAL_INT(9, lines[0].end);
    TEST_ASSERT_EQUAL_INT(9, lines[1].start);
    TEST_ASSERT_EQUAL_INT(12, lines[1].end);
    TEST_ASSERT_FALSE(lines[1].dots);
}

// "Lemon 米津玄師": breaks at the space or between the kanji, whichever fits more.
void test_wrap_mixed() {
    size_t n = dec("Lemon \xE7\xB1\xB3\xE6\xB4\xA5\xE7\x8E\x84\xE5\xB8\xAB");
    const int lemon = termfont::advance(Face::BODY, "Lemon ");
    bool cut = true;
    TEST_ASSERT_EQUAL_INT(2, lyricrun::wrap(items, n, Face::BODY, lemon + 2 * 16, 3, lines, &cut));
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_STRING("Lemon ##", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("##", bytes(lines[1].start, lines[1].end).c_str());
    // narrower than "Lemon 米" (6 cells + 16 px): at the space, the kanji on the next lines
    TEST_ASSERT_EQUAL_INT(3, lyricrun::wrap(items, n, Face::BODY, lemon + 15, 3, lines, &cut));
    TEST_ASSERT_EQUAL_STRING("Lemon", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("###", bytes(lines[1].start, lines[1].end).c_str());
    TEST_ASSERT_EQUAL_STRING("#", bytes(lines[2].start, lines[2].end).c_str());
    // no space: still breaks where the kanji start ("Lemon" 5 cells, "Lemon米" 5 cells + 16 px)
    n = dec("Lemon米津玄師");
    const int five = termfont::advance(Face::BODY, "Lemon");
    TEST_ASSERT_EQUAL_INT(3, lyricrun::wrap(items, n, Face::BODY, five + 15, 3, lines, &cut));
    TEST_ASSERT_EQUAL_STRING("Lemon", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("###", bytes(lines[1].start, lines[1].end).c_str());
    TEST_ASSERT_EQUAL_STRING("#", bytes(lines[2].start, lines[2].end).c_str());
    // all on one line when it fits
    TEST_ASSERT_EQUAL_INT(1, lyricrun::wrap(items, n, Face::BODY, 312, 3, lines, &cut));
    TEST_ASSERT_TRUE(lyricrun::hasWide(items, lines[0]));
}

// A long katakana line: cut in BIG's 3 lines, whole in TITLE's 4 (ledpanel's fit fallback).
void test_wrap_long_katakana_fits() {
    std::string s;
    for (int i = 0; i < 30; i++) s += "\xE3\x82\xAB";  // カ x 30
    const size_t n = dec(s.c_str());
    TEST_ASSERT_EQUAL_INT(30, n);
    bool cut = false;
    TEST_ASSERT_EQUAL_INT(3, lyricrun::wrap(items, n, Face::BIG, 312, 3, lines, &cut));
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_TRUE(lines[2].dots);
    TEST_ASSERT_TRUE(lyricrun::width(Face::BIG, items + lines[2].start, lines[2].end - lines[2].start, true) <= 312);
    const int k = lyricrun::wrap(items, n, Face::TITLE, 312, 4, lines, &cut);
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(2, k);  // 19 to a line
    for (int i = 0; i < k; i++)
        TEST_ASSERT_TRUE(lyricrun::width(Face::TITLE, items + lines[i].start, lines[i].end - lines[i].start) <= 312);
    TEST_ASSERT_EQUAL_INT(30, lines[k - 1].end);
}

// A missing kanji is one wide cell: it wraps like a kanji the font has.
void test_missing_kanji_is_one_cell() {
    // 们 x 10 and 米 x 10 in BIG: 9 and 1 either way
    std::string a, b;
    for (int i = 0; i < 10; i++) { a += "\xE4\xBB\xAC"; b += "\xE7\xB1\xB3"; }
    size_t n = dec(a.c_str());
    TEST_ASSERT_EQUAL_INT(10, n);
    TEST_ASSERT_EQUAL_INT(320, lyricrun::width(Face::BIG, items, n));
    TEST_ASSERT_EQUAL_INT(2, lyricrun::wrap(items, n, Face::BIG, 312, 3, lines));
    TEST_ASSERT_EQUAL_INT(9, lines[0].end);
    n = dec(b.c_str());
    TEST_ASSERT_EQUAL_INT(320, lyricrun::width(Face::BIG, items, n));
    TEST_ASSERT_EQUAL_INT(2, lyricrun::wrap(items, n, Face::BIG, 312, 3, lines));
    TEST_ASSERT_EQUAL_INT(9, lines[0].end);
}

// karaoke::wrap's rules for Latin text, which this replaces.
void test_wrap_latin_rules() {
    bool cut = true;
    TEST_ASSERT_EQUAL_INT(0, lyricrun::wrap(items, dec(""), Face::BODY, 100, 3, lines, &cut));
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(0, lyricrun::wrap(items, dec("   "), Face::BODY, 100, 3, lines, &cut));
    const int c = termfont::cell(Face::BODY);  // monospace
    // spaces: whole words, double spaces skipped at a line's start
    size_t n = dec("we sing  along the highway");
    TEST_ASSERT_EQUAL_INT(3, lyricrun::wrap(items, n, Face::BODY, 10 * c, 3, lines, &cut));
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_STRING("we sing", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("along the", bytes(lines[1].start, lines[1].end).c_str());
    TEST_ASSERT_EQUAL_STRING("highway", bytes(lines[2].start, lines[2].end).c_str());
    // a word wider than a line is broken
    n = dec("ab abcdefghijklmno");
    TEST_ASSERT_EQUAL_INT(3, lyricrun::wrap(items, n, Face::BODY, 10 * c, 3, lines, &cut));
    TEST_ASSERT_EQUAL_STRING("ab", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("abcdefghij", bytes(lines[1].start, lines[1].end).c_str());
    TEST_ASSERT_EQUAL_STRING("klmno", bytes(lines[2].start, lines[2].end).c_str());
    // the last line cut with "..", a space before it dropped
    n = dec("we sing along the highway all night long");
    TEST_ASSERT_EQUAL_INT(2, lyricrun::wrap(items, n, Face::BODY, 10 * c, 2, lines, &cut));
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_STRING("we sing", bytes(lines[0].start, lines[0].end).c_str());
    TEST_ASSERT_EQUAL_STRING("along th..", bytes(lines[1].start, lines[1].end, lines[1].dots).c_str());
    n = dec("Highway Song");
    TEST_ASSERT_EQUAL_INT(1, lyricrun::wrap(items, n, Face::BODY, 9 * c, 1, lines, &cut));
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_STRING("Highway..", bytes(lines[0].start, lines[0].end, lines[0].dots).c_str());
    // nothing fits: ".." alone
    n = dec("abc");
    TEST_ASSERT_EQUAL_INT(1, lyricrun::wrap(items, n, Face::BODY, 5, 1, lines, &cut));
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_INT(0, lines[0].end - lines[0].start);
    TEST_ASSERT_TRUE(lines[0].dots);
    TEST_ASSERT_EQUAL_INT(0, lyricrun::wrap(items, n, Face::BODY, 100, 0, lines));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_latin1_unchanged);
    RUN_TEST(test_japanese_items);
    RUN_TEST(test_upper_and_cap);
    RUN_TEST(test_isCjk);
    RUN_TEST(test_width);
    RUN_TEST(test_glyph_top);
    RUN_TEST(test_wrap_japanese_between_characters);
    RUN_TEST(test_wrap_mixed);
    RUN_TEST(test_wrap_long_katakana_fits);
    RUN_TEST(test_missing_kanji_is_one_cell);
    RUN_TEST(test_wrap_latin_rules);
    return UNITY_END();
}
