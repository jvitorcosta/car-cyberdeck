#include <unity.h>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include "../../src/ui/termfont.h"
#include "../../src/ui/termlayout.h"
#include "../../src/util/deckmenu.h"
#include "../../src/util/karaoke.h"
#include "../../src/util/ledlook.h"
#include "../../src/util/ledpresets.h"
#include "../../src/util/lightstatus.h"
#include "../../src/util/tripmeter.h"
#include "../../src/util/weather.h"

using termfont::Face;
using namespace termlayout;

void setUp() {}
void tearDown() {}

static constexpr int MARGIN = 4;  // every fixed label keeps this much room to its rect's edges

// ---- the tables ----

void test_width_follows_tft_espi() {
    // SMALL's 'H': advance 7, ink 5 wide from x 1
    TEST_ASSERT_EQUAL_INT(6, termfont::width(Face::SMALL, "H"));
    TEST_ASSERT_EQUAL_INT(7 + 6, termfont::width(Face::SMALL, "HH"));
    TEST_ASSERT_EQUAL_INT(14, termfont::advance(Face::SMALL, "HH"));
    TEST_ASSERT_EQUAL_INT(0, termfont::width(Face::SMALL, ""));
    TEST_ASSERT_EQUAL_INT(0, termfont::width(Face::SMALL, " "));         // a space has no ink
    TEST_ASSERT_EQUAL_INT(7, termfont::width(Face::SMALL, "H\x7F"));     // H not last: its advance; 0x7F: nothing
    TEST_ASSERT_EQUAL_INT(termfont::width(Face::BIG, "AB"), termfont::widthOf("AB", static_cast<int>(Face::BIG)));
}

void test_metrics() {
    const int cap[] = {10, 11, 14, 21}, cell[] = {7, 8, 10, 14};
    for (int i = 0; i < termfont::N_FACES; i++) {
        const Face f = static_cast<Face>(i);
        TEST_ASSERT_EQUAL_INT(cap[i], termfont::capHeight(f));
        TEST_ASSERT_EQUAL_INT(cell[i], termfont::cell(f));
        TEST_ASSERT_TRUE(termfont::ascent(f) >= termfont::capHeight(f));
        TEST_ASSERT_TRUE(termfont::descent(f) > 0);
        TEST_ASSERT_EQUAL_UINT16(0x20, termfont::font(f).first);
        TEST_ASSERT_EQUAL_UINT16(0x7E, termfont::font(f).last);
        TEST_ASSERT_EQUAL_INT(termfont::cell(f) * 4, termfont::advance(f, "M i."));  // monospace
    }
}

// The bitmap is packed MSB-first with rows running on: 'H' has both stems lit on every row and its
// bar across one row.
void test_bitmap_packing() {
    for (int i = 0; i < termfont::N_FACES; i++) {
        const termfont::Font& f = termfont::font(static_cast<Face>(i));
        const termfont::Glyph& g = f.glyph['H' - f.first];
        auto px = [&](int x, int y) {
            const unsigned bit = static_cast<unsigned>(y * g.width + x);
            return (f.bitmap[g.bitmapOffset + bit / 8] >> (7 - bit % 8)) & 1;
        };
        int fullRows = 0;
        for (int y = 0; y < g.height; y++) {
            TEST_ASSERT_EQUAL_INT(1, px(0, y));
            TEST_ASSERT_EQUAL_INT(1, px(g.width - 1, y));
            int lit = 0;
            for (int x = 0; x < g.width; x++) lit += px(x, y);
            if (lit == g.width) fullRows++;
        }
        TEST_ASSERT_TRUE(fullRows >= 1 && fullRows < g.height);
    }
}

// ---- every fixed label fits its rect ----

static void fitsLeft(Face f, const char* s, int x, int right) {
    TEST_ASSERT_TRUE_MESSAGE(x >= MARGIN, s);
    TEST_ASSERT_TRUE_MESSAGE(x + termfont::width(f, s) <= right - MARGIN, s);
}

static void fitsCentred(Face f, const char* s, int w) { TEST_ASSERT_TRUE_MESSAGE(termfont::width(f, s) <= w - 2 * MARGIN, s); }

void test_menu_items_fit() {
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0);
    TEST_ASSERT_EQUAL_INT(100, r.w);
    for (const char* s : {"> [LIGHTS]", "> [WEATHER]", "> [KARAOKE]"}) fitsLeft(Face::BODY, s, ROW_TEXT_X, r.w);
}

void test_preset_names_fit() {
    const deckmenu::Rect menu = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_PRE0);
    const deckmenu::Rect lights = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_PRE0);
    TEST_ASSERT_EQUAL_INT(154, menu.w);
    TEST_ASSERT_EQUAL_INT(75, lights.w);
    for (int i = 0; i < ledpresets::N; i++) {
        const char* name = ledpresets::PRESETS[i].name;
        fitsLeft(Face::TITLE, name, PRESET_TEXT_X, menu.w - PRESET_CHIPS_FROM_RIGHT);  // left of the chips
        fitsCentred(Face::BODY, name, lights.w);
    }
}

void test_colour_names_fit() {
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_TILE0);
    TEST_ASSERT_EQUAL_INT(75, r.w);
    TEST_ASSERT_TRUE(TILE_CHIP_X + TILE_CHIP_W + MARGIN <= TILE_TEXT_X);
    TEST_ASSERT_TRUE(TILE_CHIP_H <= r.h - 2 * MARGIN);
    for (const ledlook::Swatch& s : ledlook::SWATCHES) {
        char name[16];
        size_t i = 0;
        for (; s.name[i]; i++) name[i] = static_cast<char>(toupper(static_cast<unsigned char>(s.name[i])));
        name[i] = 0;
        fitsLeft(Face::SMALL, name, TILE_TEXT_X, r.w);
    }
}

void test_chrome_fits() {
    fitsCentred(Face::BODY, "< BACK", deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_BACK).w);
    const int tab = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_ZONE0).w;
    TEST_ASSERT_EQUAL_INT(76, tab);
    for (const char* s : {"BOTH", "INSIDE", "OUTSIDE"}) fitsCentred(Face::BODY, s, tab);
    fitsCentred(Face::BODY, "WEATHER", WEATHER_TAB_W);
    const int pm = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_MINUS).w;
    for (const char* s : {"[-]", "[+]"}) fitsCentred(Face::TITLE, s, pm);
    for (const char* s : {"100%", "MIX", "OFF"}) fitsCentred(Face::TITLE, s, deckmenu::LEVEL.w);
}

void test_header_fits() {
    for (const char* s : HEADER_LINES) fitsLeft(Face::SMALL, s, HEADER_X, 316);  // the rule's ends, 4 in
    // three lines in the header above its rule (y 41), none over the next
    const int cap = termfont::capHeight(Face::SMALL), up = termfont::ascent(Face::SMALL);
    TEST_ASSERT_TRUE(HEADER_Y0 - (up - cap) >= 0);
    TEST_ASSERT_TRUE(HEADER_Y0 + 2 * HEADER_DY + cap + termfont::descent(Face::SMALL) <= deckmenu::HEADER.h - 3);
    TEST_ASSERT_TRUE(HEADER_DY >= cap + 2);
}

// The strip's widest line: both lights on in ORANGE at 100 % (the longest name), chips and cursor.
void test_strip_longest_line_fits() {
    ledlook::Look l;
    l.swatch = 1;
    l.pct = 100;
    char line[48];
    lightstatus::strip(l, true, l, true, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("IN ORANGE 100%  OUT ORANGE 100%", line);
    const bool chip[2] = {true, true};
    StripPart parts[2];
    const int end = stripLayout(line, STRIP_TEXT_X, chip, parts);
    TEST_ASSERT_TRUE(end > STRIP_TEXT_X);
    TEST_ASSERT_TRUE_MESSAGE(end + termfont::cell(Face::SMALL) <= STRIP_RIGHT - MARGIN, "cursor past the strip");
    TEST_ASSERT_EQUAL_STRING("OUT", parts[1].name);
    TEST_ASSERT_EQUAL_STRING("ORANGE 100%", parts[1].state);
    TEST_ASSERT_TRUE(parts[0].chipX > parts[0].nameX && parts[1].chipX > parts[0].stateX);
}

void test_strip_plain_shapes() {
    char line[48] = "IN --  OUT OFF";
    const bool chip[2] = {false, false};
    StripPart parts[2];
    const int end = stripLayout(line, 100, chip, parts);
    TEST_ASSERT_EQUAL_INT(-1, parts[0].chipX);
    TEST_ASSERT_EQUAL_STRING("--", parts[0].state);
    TEST_ASSERT_EQUAL_INT(100 + termfont::advance(Face::SMALL, "IN --") + STRIP_LIGHT_GAP + termfont::advance(Face::SMALL, "OUT OFF"), end);
    char odd[16] = "HELLO";
    TEST_ASSERT_EQUAL_INT(-1, stripLayout(odd, 100, chip, parts));  // not two halves: the caller draws it plain
}

void test_strip_clock_and_lines_fit() {
    // the clock's ink ends left of the text, whatever the digits
    for (const char* s : {"--:--", "00:00", "23:59", "18:47", "20:08", "11:11"}) {
        TEST_ASSERT_TRUE_MESSAGE(STRIP_CLOCK_X + termfont::width(Face::BIG, s) + MARGIN <= STRIP_TEXT_X, s);
    }
    for (const char* s : {"--C", "-10C", "40C"}) TEST_ASSERT_TRUE_MESSAGE(termfont::width(Face::SMALL, s) <= STRIP_TEXT_X - 2 * MARGIN, s);
    tripmeter::Trip t;
    t.seconds = 99 * 3600 + 59 * 60;
    t.meters = 999900;
    t.hasDistance = true;
    char s[40];
    tripmeter::line(t, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("TRIP 999.9 KM  99:59", s);
    fitsLeft(Face::SMALL, s, STRIP_TEXT_X, STRIP_RIGHT);
    tripmeter::last(t, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 999.9 KM 99:59", s);
    for (const char* m : {static_cast<const char*>(s), ">NO SYNCED LYRICS", ">INSIDE: NO LINK", ">OUTSIDE: LINKED", ">PURPURA APPLIED"}) {
        TEST_ASSERT_TRUE_MESSAGE(STRIP_TEXT_X + termfont::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) <= STRIP_RIGHT - MARGIN, m);
    }
}

void test_status_and_weather_fit() {
    // the Lights status line sits in STATUS (12 rows between the tab rule and the colours)
    TEST_ASSERT_TRUE(termfont::ascent(Face::SMALL) <= deckmenu::STATUS.h);
    fitsLeft(Face::SMALL, "IN ON 100%  OUT ON 100%  LINK 2/2", STATUS_TEXT_X, deckmenu::STATUS.w);
    for (int code : {0, 1, 3, 45, 51, 61, 71, 80, 85, 95, 999}) fitsLeft(Face::TITLE, weather::words(code), WEATHER_X, 320);
    fitsLeft(Face::BODY, "HI -10C  LO -20C", WEATHER_X, 320);
    fitsLeft(Face::BIG, "-10C", WEATHER_X, 320);
}

// The lyric areas: a face's lines at its pitch don't overlap, and the most lines fit their area.
void test_karaoke_pitches() {
    auto rows = [](Face f) { return termfont::ascent(f) + termfont::descent(f); };
    TEST_ASSERT_TRUE(rows(Face::BIG) <= KARAOKE_BIG_DY && 3 * KARAOKE_BIG_DY <= KARAOKE_BIG_H);
    TEST_ASSERT_TRUE(rows(Face::TITLE) <= KARAOKE_FIT_DY && 4 * KARAOKE_FIT_DY <= KARAOKE_BIG_H);
    TEST_ASSERT_TRUE(rows(Face::BODY) <= KARAOKE_NEXT_DY && 2 * KARAOKE_NEXT_DY <= KARAOKE_NEXT_H);
    fitsCentred(Face::BIG, ">NO SYNCED LYRICS", KARAOKE_W + 2 * MARGIN);
    fitsCentred(Face::BIG, ">WRONG KEY", KARAOKE_W + 2 * MARGIN);
}

// karaoke::wrap measures with the face it draws in.
void test_wrap_with_a_face() {
    karaoke::Line lines[3];
    bool cut = true;
    const int n = karaoke::wrap("I walked along the empty road under a sky of stars", static_cast<int>(Face::BIG),
                                termfont::widthOf, KARAOKE_W, 3, lines, &cut);
    TEST_ASSERT_TRUE(n >= 2 && n <= 3);
    TEST_ASSERT_FALSE(cut);
    for (int i = 0; i < n; i++) TEST_ASSERT_TRUE(termfont::width(Face::BIG, lines[i].s) <= KARAOKE_W);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_width_follows_tft_espi);
    RUN_TEST(test_metrics);
    RUN_TEST(test_bitmap_packing);
    RUN_TEST(test_menu_items_fit);
    RUN_TEST(test_preset_names_fit);
    RUN_TEST(test_colour_names_fit);
    RUN_TEST(test_chrome_fits);
    RUN_TEST(test_header_fits);
    RUN_TEST(test_strip_longest_line_fits);
    RUN_TEST(test_strip_plain_shapes);
    RUN_TEST(test_strip_clock_and_lines_fit);
    RUN_TEST(test_status_and_weather_fit);
    RUN_TEST(test_karaoke_pitches);
    RUN_TEST(test_wrap_with_a_face);
    return UNITY_END();
}
