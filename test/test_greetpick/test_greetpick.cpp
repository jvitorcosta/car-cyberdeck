#include <unity.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/util/black/greetpick.h"
#include "../../src/black/ui/cjkfont.h"

using namespace greetpick;

void setUp() {}
void tearDown() {}

static int glyphs(const char16_t* s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}

void test_slots() {
    TEST_ASSERT_EQUAL(NIGHT, slot(0));
    TEST_ASSERT_EQUAL(NIGHT, slot(4 * 60 + 59));
    TEST_ASSERT_EQUAL(MORNING, slot(5 * 60));
    TEST_ASSERT_EQUAL(MORNING, slot(10 * 60 + 59));
    TEST_ASSERT_EQUAL(DAY, slot(11 * 60));
    TEST_ASSERT_EQUAL(DAY, slot(17 * 60 + 59));
    TEST_ASSERT_EQUAL(EVENING, slot(18 * 60));
    TEST_ASSERT_EQUAL(EVENING, slot(21 * 60 + 59));
    TEST_ASSERT_EQUAL(NIGHT, slot(22 * 60));
    TEST_ASSERT_EQUAL(NIGHT, slot(1439));
}

// Every part of the day has a greeting of its own.
void test_each_slot_has_one() {
    for (int w = MORNING; w <= NIGHT; ++w) {
        bool found = false;
        for (int i = 0; i < count(); ++i) found = found || whenOf(i) == w;
        TEST_ASSERT_TRUE(found);
    }
}

// Clock not set: only the ANY ones, all of them reachable, never last boot's.
void test_unset_clock_is_neutral_and_never_repeats() {
    std::vector<bool> seen(count(), false);
    for (int last = -1; last < count(); ++last)
        for (uint32_t r = 0; r < 64; ++r) {
            const int g = pick(r * 2654435761u, last, -1);
            TEST_ASSERT_TRUE(g >= 0 && g < count());
            TEST_ASSERT_EQUAL(ANY, whenOf(g));
            TEST_ASSERT_NOT_EQUAL(last, g);
            seen[g] = true;
        }
    for (int i = 0; i < count(); ++i) TEST_ASSERT_EQUAL(whenOf(i) == ANY, (bool)seen[i]);
}

// Clock set: the ANY ones and that part of the day's, never another part's.
void test_clock_adds_its_part_of_the_day() {
    const int minutes[] = {7 * 60, 14 * 60, 20 * 60, 23 * 60 + 30};
    for (int m : minutes) {
        bool timed = false;
        for (uint32_t r = 0; r < 256; ++r) {
            const int g = pick(r * 2654435761u, -1, m);
            TEST_ASSERT_TRUE(whenOf(g) == ANY || whenOf(g) == slot(m));
            timed = timed || whenOf(g) == slot(m);
        }
        TEST_ASSERT_TRUE(timed);
    }
}

// NVS may hold anything.
void test_bad_last_is_ignored() {
    for (uint32_t r = 0; r < 32; ++r) {
        const int a = pick(r, 99, 600), b = pick(r, -7, -1);
        TEST_ASSERT_TRUE(a >= 0 && a < count());
        TEST_ASSERT_TRUE(b >= 0 && b < count());
    }
}

// What the sky beside the terminal fits (ui/bootscene draws them there).
void test_fits_the_sky() {
    for (int i = 0; i < count(); ++i) {
        const Greeting& g = at(i);
        TEST_ASSERT_TRUE(glyphs(g.jp) > 0 && glyphs(g.jp) <= MAX_GLYPHS);
        TEST_ASSERT_TRUE(strlen(g.romaji) > 0 && (int)strlen(g.romaji) <= MAX_LATIN);
        TEST_ASSERT_TRUE(strlen(g.english) > 0 && (int)strlen(g.english) <= MAX_LATIN);
        for (const char* s : {g.romaji, g.english})
            for (; *s; ++s) TEST_ASSERT_TRUE(*s >= ' ' && *s <= '~' && !(*s >= 'a' && *s <= 'z'));
    }
}

// Every kana/kanji is in the font the black deck ships, 16 px wide and not blank (tools/gen_cjk_font.py
// writes a codepoint Unifont lacks as an empty cell): no boxes, no holes.
void test_every_glyph_is_in_the_font() {
    std::vector<uint8_t> blob;
    std::string path = __FILE__;
    for (char& c : path) c = c == '\\' ? '/' : c;
    path = path.substr(0, path.rfind("test/test_greetpick")) + "data/cjk16.bin";   // absolute or relative
    FILE* f = fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
    uint8_t buf[4096];
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) blob.insert(blob.end(), buf, buf + n);
    fclose(f);
    cjkfont::Font font;
    TEST_ASSERT_TRUE(font.open(blob.data(), blob.size()));
    for (int i = 0; i < count(); ++i)
        for (const char16_t* p = at(i).jp; *p; ++p) {
            char msg[48];
            snprintf(msg, sizeof msg, "greeting %d, U+%04X", i, (unsigned)*p);
            int w = 0;
            const uint8_t* g = font.glyph(*p, &w);
            TEST_ASSERT_NOT_NULL_MESSAGE(g, msg);
            TEST_ASSERT_EQUAL_INT_MESSAGE(16, w, msg);
            bool ink = false;
            for (int k = 0; k < 32; ++k) ink = ink || g[k];
            TEST_ASSERT_TRUE_MESSAGE(ink, msg);
        }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_slots);
    RUN_TEST(test_each_slot_has_one);
    RUN_TEST(test_unset_clock_is_neutral_and_never_repeats);
    RUN_TEST(test_clock_adds_its_part_of_the_day);
    RUN_TEST(test_bad_last_is_ignored);
    RUN_TEST(test_fits_the_sky);
    RUN_TEST(test_every_glyph_is_in_the_font);
    return UNITY_END();
}
