#include <unity.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/ui/cjkfont.h"
#include "../../src/ui/gojiraname.h"
#include "../../src/ui/termfont.h"
#include "../../src/ui/termlayout.h"
#include "../../src/util/deckmenu.h"
#include "../../src/util/ledpresets.h"
#include "../../src/util/lightstatus.h"

using termfont::Face;
using namespace termlayout;

void setUp() {}
void tearDown() {}

static constexpr int MARGIN = 4;  // test_termfont's: every fixed label keeps this much room
static const char GOJIRA[] = {gojiraname::MARK, 0};

static bool inked(const uint8_t* g, int row, int col) { return (((g[2 * row] << 8) | g[2 * row + 1]) >> (15 - col)) & 1; }
static int inkInRow(int row) {
    int n = 0;
    for (int i = 0; i < gojiraname::N_GLYPHS; i++) {
        for (int col = 0; col < 16; col++) n += inked(gojiraname::glyph(i), row, col);
    }
    return n;
}

// The mark is GOJIRA's screen name, and nothing else's.
void test_the_mark_is_gojiras() {
    TEST_ASSERT_EQUAL_STRING(GOJIRA, ledpresets::shown(ledpresets::PRESETS[2]));
    TEST_ASSERT_EQUAL_STRING("GOJIRA", ledpresets::PRESETS[2].name);
    for (int i = 0; i < ledpresets::N; i++) {
        TEST_ASSERT_NULL(strchr(ledpresets::PRESETS[i].name, gojiraname::MARK));  // never on the wire
    }
}

// The mark measures as three 16 px glyphs, alone or among text, in every face.
void test_width_of_the_mark() {
    TEST_ASSERT_EQUAL_INT(48, gojiraname::WIDTH);
    for (Face f : {Face::SMALL, Face::BODY, Face::TITLE, Face::BIG}) {
        TEST_ASSERT_EQUAL_INT(48, gojiraname::width(f, GOJIRA));
        TEST_ASSERT_EQUAL_INT(48, gojiraname::advance(f, GOJIRA));
        char m[32];
        lightstatus::applied(GOJIRA, m, sizeof(m));
        TEST_ASSERT_EQUAL_INT(termfont::advance(f, ">") + 48 + termfont::width(f, " APPLIED"), gojiraname::width(f, m));
        TEST_ASSERT_EQUAL_INT(termfont::advance(f, ">") + 48 + termfont::advance(f, " APPLIED"), gojiraname::advance(f, m));
        // without the mark, termfont's own
        TEST_ASSERT_EQUAL_INT(termfont::width(f, ">ROSE APPLIED"), gojiraname::width(f, ">ROSE APPLIED"));
        TEST_ASSERT_EQUAL_INT(termfont::advance(f, ">ROSE APPLIED"), gojiraname::advance(f, ">ROSE APPLIED"));
        TEST_ASSERT_EQUAL_INT(0, gojiraname::width(f, ""));
    }
}

// The ink is rows 1..14; the smaller faces drop rows 1 and 14, which hold only three pixels.
void test_rows() {
    for (int row : {0, 15}) TEST_ASSERT_EQUAL_INT(0, inkInRow(row));
    TEST_ASSERT_TRUE(inkInRow(gojiraname::INK_FIRST) > 0 && inkInRow(gojiraname::INK_LAST) > 0);
    TEST_ASSERT_TRUE(inkInRow(gojiraname::INK_FIRST) + inkInRow(gojiraname::INK_LAST) <= 3);
    for (Face f : {Face::SMALL, Face::BODY}) {
        TEST_ASSERT_EQUAL_INT(2, gojiraname::rows(f).first);
        TEST_ASSERT_EQUAL_INT(13, gojiraname::rows(f).last);
    }
    for (Face f : {Face::TITLE, Face::BIG}) {
        TEST_ASSERT_EQUAL_INT(1, gojiraname::rows(f).first);
        TEST_ASSERT_EQUAL_INT(14, gojiraname::rows(f).last);
    }
    // sitting on the baseline: TITLE's 14 rows are exactly its caps
    TEST_ASSERT_EQUAL_INT(100 - termfont::capHeight(Face::TITLE), gojiraname::top(Face::TITLE, 100));
    TEST_ASSERT_EQUAL_INT(100 - 12, gojiraname::top(Face::SMALL, 100));
}

// ">ゴジラ APPLIED" on the Lights status line: its rows stay inside STATUS (whose clear is the rect),
// and it fits across; on the Menu's prompt line, inside the band's clear, with the cursor after it.
void test_message_fits() {
    char m[32];
    lightstatus::applied(GOJIRA, m, sizeof(m));
    const deckmenu::Rect& r = deckmenu::STATUS;
    const int base = r.y + r.h;  // drawStatus: BL_DATUM on the rect's bottom
    const gojiraname::Rows rows = gojiraname::rows(Face::SMALL);
    const int top = gojiraname::top(Face::SMALL, base);
    TEST_ASSERT_TRUE(top >= r.y);
    TEST_ASSERT_TRUE(top + rows.last - rows.first <= r.y + r.h - 1);
    TEST_ASSERT_TRUE(STATUS_TEXT_X + gojiraname::width(Face::SMALL, m) <= r.w - MARGIN);
    TEST_ASSERT_TRUE(DASH_X + gojiraname::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) <= DASH_RIGHT);
    TEST_ASSERT_TRUE(gojiraname::top(Face::SMALL, PROMPT_BASE) > LIGHTS_RULE_Y);
}

// The PRESETS tab: the name (TITLE) centred in its 100 px button, its rows above the chips.
void test_preset_button_fits() {
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Presets, deckmenu::P_PRE0 + 2);
    TEST_ASSERT_EQUAL_INT(100, r.w);
    TEST_ASSERT_TRUE(gojiraname::width(Face::TITLE, GOJIRA) <= r.w - 2 * MARGIN);
    const int base = PRESET_NAME_Y + termfont::capHeight(Face::TITLE);  // TC_DATUM: the caps' top at PRESET_NAME_Y
    TEST_ASSERT_EQUAL_INT(PRESET_NAME_Y, gojiraname::top(Face::TITLE, base));
    TEST_ASSERT_TRUE(base - 1 < PRESET_CHIP_Y - 1);
}

// The committed bitmaps are the Unifont glyphs of the blob the black deck ships, read with its own
// reader (tools/gen_gojira_glyphs.py made them from it).
void test_glyphs_are_unifonts() {
    std::vector<uint8_t> blob;
    std::string path = __FILE__;
    for (char& c : path) c = c == '\\' ? '/' : c;
    path = path.substr(0, path.rfind("/test/")) + "/data/cjk16.bin";
    FILE* f = fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
    uint8_t buf[4096];
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) blob.insert(blob.end(), buf, buf + n);
    fclose(f);
    cjkfont::Font font;
    TEST_ASSERT_TRUE(font.open(blob.data(), blob.size()));
    const uint32_t cps[] = {0x30B4, 0x30B8, 0x30E9};  // ゴ ジ ラ
    for (int i = 0; i < gojiraname::N_GLYPHS; i++) {
        int w = 0;
        const uint8_t* g = font.glyph(cps[i], &w);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_EQUAL_INT(16, w);
        TEST_ASSERT_EQUAL_MEMORY(g, gojiraname::glyph(i), 32);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_mark_is_gojiras);
    RUN_TEST(test_width_of_the_mark);
    RUN_TEST(test_rows);
    RUN_TEST(test_message_fits);
    RUN_TEST(test_preset_button_fits);
    RUN_TEST(test_glyphs_are_unifonts);
    return UNITY_END();
}
