#include <unity.h>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include "../../src/ui/termfont.h"
#include "../../src/ui/termlayout.h"
#include "../../src/util/dashtext.h"
#include "../../src/util/deckmenu.h"
#include "../../src/util/karaoke.h"
#include "../../src/util/ledlook.h"
#include "../../src/util/ledpresets.h"
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

// Ink rows [top, bottom] of a text in face f: from its caps' top row y, or from its baseline.
struct Rows { int top, bottom; };
static Rows fromCapTop(Face f, int y) {
    const int cap = termfont::capHeight(f);
    return {y + cap - termfont::ascent(f), y + cap + termfont::descent(f) - 1};
}
static Rows fromBase(Face f, int base) { return {base - termfont::ascent(f), base + termfont::descent(f) - 1}; }
static void above(const Rows& a, const Rows& b, const char* what) { TEST_ASSERT_TRUE_MESSAGE(a.bottom < b.top, what); }
static void between(int ruleAbove, const Rows& r, int ruleBelow, const char* what) {
    TEST_ASSERT_TRUE_MESSAGE(ruleAbove < r.top && r.bottom < ruleBelow, what);
}

// Right-aligned at `right` (its ink ends there), clear of `left` by the margin; `right` itself is
// the margin in from the rules' end (x 316).
static void fitsRight(Face f, const char* s, int right, int left) {
    TEST_ASSERT_TRUE_MESSAGE(right <= 316 - MARGIN, s);
    TEST_ASSERT_TRUE_MESSAGE(right - termfont::width(f, s) >= left + MARGIN, s);
}

void test_menu_items_fit() {
    for (int i = 0; i < deckmenu::N_ITEMS; i++) {
        const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0 + i);
        TEST_ASSERT_EQUAL_INT(154, r.w);
        for (const char* s : {"> [LIGHTS]", "> [KARAOKE]"}) fitsLeft(Face::BODY, s, ROW_TEXT_X, r.w);
        TEST_ASSERT_TRUE(termfont::capHeight(Face::BODY) <= r.h - 2 * MARGIN);
    }
    // KARAOKE's LIVE tag: inside the item at its right edge, clear of the item's text, its caps
    // LIVE_PAD from the label's edges
    const deckmenu::Rect k = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0 + deckmenu::KARAOKE);
    const int tagW = termfont::width(Face::SMALL, LIVE_TAG) + 2 * LIVE_PAD, tagH = termfont::capHeight(Face::SMALL) + 2 * LIVE_PAD;
    TEST_ASSERT_TRUE(LIVE_RIGHT >= MARGIN);
    TEST_ASSERT_TRUE(ROW_TEXT_X + termfont::width(Face::BODY, "> [KARAOKE]") + MARGIN <= k.w - LIVE_RIGHT - tagW);
    TEST_ASSERT_TRUE(tagH <= k.h - 2 * MARGIN);
    TEST_ASSERT_TRUE(LIVE_PAD >= 2);
}

void test_preset_names_fit() {
    const deckmenu::Rect lights = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_PRE0);
    TEST_ASSERT_TRUE(lights.w == 75 && lights.h == 36);
    for (int i = 0; i < ledpresets::N; i++) fitsCentred(Face::BODY, ledpresets::PRESETS[i].name, lights.w);
    // the name over the two chips (12-14 px squares), the chips' 1 px edge on a bar clear of the
    // name's ink and the margin, the pair centred within the margins
    TEST_ASSERT_TRUE(PRESET_CHIP >= 12 && PRESET_CHIP <= 14);
    TEST_ASSERT_TRUE(fromCapTop(Face::BODY, PRESET_NAME_Y).bottom < PRESET_CHIP_Y - 1);
    TEST_ASSERT_TRUE(PRESET_NAME_Y >= MARGIN);  // the names are capitals: their ink starts at the caps
    TEST_ASSERT_TRUE(PRESET_CHIP_Y + PRESET_CHIP <= lights.h - MARGIN);
    TEST_ASSERT_TRUE(2 * PRESET_CHIP + PRESET_CHIP_GAP <= lights.w - 2 * MARGIN);
}

void test_colour_names_fit() {
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_TILE0);
    TEST_ASSERT_EQUAL_INT(75, r.w);
    TEST_ASSERT_TRUE(TILE_CHIP_X + TILE_CHIP_W + MARGIN <= TILE_TEXT_X);
    TEST_ASSERT_TRUE(TILE_CHIP_H <= r.h - 2 * MARGIN);
    TEST_ASSERT_TRUE(TILE_CHIP_X >= MARGIN && TILE_CHIP_W == 16 && TILE_CHIP_H == 28);
    for (const ledlook::Swatch& s : ledlook::SWATCHES) {
        char name[16];
        size_t i = 0;
        for (; s.name[i]; i++) name[i] = static_cast<char>(toupper(static_cast<unsigned char>(s.name[i])));
        name[i] = 0;
        fitsLeft(Face::BODY, name, TILE_TEXT_X, r.w);  // one face with the preset names
    }
}

void test_chrome_fits() {
    fitsCentred(Face::BODY, "< BACK", deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_BACK).w);
    const int tab = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_ZONE0).w;
    TEST_ASSERT_EQUAL_INT(76, tab);
    for (const char* s : {"BOTH", "INSIDE", "OUTSIDE"}) fitsCentred(Face::BODY, s, tab);
    const int pm = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_MINUS).w;
    for (const char* s : {"[-]", "[+]"}) fitsCentred(Face::TITLE, s, pm);
    for (const char* s : {"100%", "MIX", "OFF"}) fitsCentred(Face::TITLE, s, deckmenu::LEVEL.w);
}

// The boot's typed lines: on screen, none over the next.
void test_boot_lines_fit() {
    for (const char* s : BOOT_LINES) fitsLeft(Face::SMALL, s, BOOT_X, 320);
    const int cap = termfont::capHeight(Face::SMALL), up = termfont::ascent(Face::SMALL);
    TEST_ASSERT_TRUE(BOOT_Y0 - (up - cap) >= 0);
    TEST_ASSERT_TRUE(BOOT_DY >= cap + termfont::descent(Face::SMALL));  // descenders clear of the next caps
}

// ---- the dashboard (the Menu), band by band ----

void test_dash_header_fits() {
    fitsLeft(Face::SMALL, DASH_TITLE, DASH_X, 316);  // the rule's ends, 4 in
    between(-1, fromCapTop(Face::SMALL, DASH_TITLE_Y), HEADER_RULE_Y, DASH_TITLE);
    TEST_ASSERT_TRUE(HEADER_RULE_Y < deckmenu::HEADER.y + deckmenu::HEADER.h);
}

void test_dash_clock_and_weather_fit() {
    // the clock (BIG) and the date (SMALL) left of DASH_SPLIT, whatever the digits / the day
    for (const char* s : {"--:--", "00:00", "23:59", "18:47", "20:08", "11:11"}) fitsLeft(Face::BIG, s, DASH_X, DASH_SPLIT);
    for (const char* s : {"--- -- ---", "WED 23 SEP", "THU 30 NOV", "SAT 28 MAY"}) fitsLeft(Face::SMALL, s, DASH_X, DASH_SPLIT);
    // the temperature (BIG) and the sky (SMALL) right of it
    for (const char* s : {"--C", "-10C", "32C", "40C"}) fitsRight(Face::BIG, s, DASH_RIGHT, DASH_SPLIT);
    for (int code : {0, 1, 3, 45, 51, 61, 71, 80, 85, 95, 999}) fitsRight(Face::SMALL, weather::words(code), DASH_RIGHT, DASH_SPLIT);
    for (const char* s : {"PARTLY CLOUDY", "THUNDERSTORM", "SNOW SHOWERS"}) fitsRight(Face::SMALL, s, DASH_RIGHT, DASH_SPLIT);
    // the HI / LO line across the band, with the longest city a reading holds (23 characters)
    char s[48];
    dashtext::hiLo(true, -9, -12, "ABCDEFGHIJKLMNOPQRSTUVW", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI -9 LO -12  ABCDEFGHIJKLMNOPQRSTUVW", s);
    fitsRight(Face::SMALL, s, DASH_RIGHT, 4);
    dashtext::hiLo(true, -10, -20, "ABCDEFGHIJKLMNOPQRSTUVW", s, sizeof(s));
    fitsRight(Face::SMALL, s, DASH_RIGHT, 4);
    fitsRight(Face::SMALL, ">OFFLINE", DASH_RIGHT, 4);
    // rows: the clock over the date over the HI / LO line, between the header rule and the band's
    const Rows clock = fromCapTop(Face::BIG, CLOCK_Y), date = fromCapTop(Face::SMALL, DATE_Y);
    const Rows words = fromCapTop(Face::SMALL, WORDS_Y), hiLo = fromCapTop(Face::SMALL, HILO_Y);
    between(HEADER_RULE_Y, clock, CLOCK_RULE_Y, "clock");
    above(clock, date, "clock over the date");
    above(clock, words, "temperature over the sky");
    above(date, hiLo, "date over HI / LO");
    above(words, hiLo, "sky over HI / LO");
    between(HEADER_RULE_Y, hiLo, CLOCK_RULE_Y, "HI / LO");
    TEST_ASSERT_TRUE(CLOCK_RULE_Y >= deckmenu::CLOCK_BAND.y && CLOCK_RULE_Y < deckmenu::LIGHTS_BAND.y + 2);
    // the split clears: the left column's clear ends before the right column's ink starts
    TEST_ASSERT_TRUE(DASH_SPLIT < DASH_RIGHT - termfont::width(Face::BIG, "-10C"));
}

void test_dash_lights_fit() {
    // INSIDE / OUTSIDE (BODY), the chip, the widest colour name (TITLE), its level right-aligned
    for (const char* s : {"INSIDE", "OUTSIDE"}) fitsLeft(Face::BODY, s, DASH_X, LIGHT_CHIP_X);
    TEST_ASSERT_TRUE(LIGHT_CHIP_X + LIGHT_CHIP_W + MARGIN <= LIGHT_NAME_X);
    for (const char* name : {"ORANGE", "PURPLE", "YELLOW"}) {
        TEST_ASSERT_TRUE_MESSAGE(LIGHT_NAME_X + termfont::width(Face::TITLE, name) + MARGIN <=
                                     DASH_RIGHT - termfont::width(Face::TITLE, "100%"), name);
    }
    for (const char* s : {"OFF", "-- NO LINK"}) fitsLeft(Face::TITLE, s, LIGHT_NAME_X, DASH_RIGHT + MARGIN);
    // rows: the two rows between the band's rules (no label: the rows say what they are)
    const Rows row0 = fromBase(Face::TITLE, LIGHT_ROW_BASE), row1 = fromBase(Face::TITLE, LIGHT_ROW_BASE + LIGHT_ROW_DY);
    TEST_ASSERT_TRUE(fromBase(Face::BODY, LIGHT_ROW_BASE).top >= row0.top);  // TITLE is the taller face
    between(CLOCK_RULE_Y, row0, LIGHTS_RULE_Y, "INSIDE");
    above(row0, row1, "INSIDE over OUTSIDE");
    between(CLOCK_RULE_Y, row1, LIGHTS_RULE_Y, "OUTSIDE");
    // spaced evenly: the gaps above, between and below the rows' caps (what the eye sees: names,
    // digits, %) within a row of each other
    const int cap = termfont::capHeight(Face::TITLE);
    const int gapTop = (LIGHT_ROW_BASE - cap) - (CLOCK_RULE_Y + 1);
    const int gapMid = (LIGHT_ROW_BASE + LIGHT_ROW_DY - cap) - LIGHT_ROW_BASE;
    const int gapBottom = LIGHTS_RULE_Y - (LIGHT_ROW_BASE + LIGHT_ROW_DY);
    for (int g : {gapTop, gapMid, gapBottom}) {
        for (int h : {gapTop, gapMid, gapBottom}) TEST_ASSERT_TRUE(g - h <= 1 && h - g <= 1);
    }
    // the chip as tall as TITLE's caps, on them
    TEST_ASSERT_EQUAL_INT(termfont::capHeight(Face::TITLE), LIGHT_CHIP_H);
    // the band's rule above the items
    TEST_ASSERT_TRUE(LIGHTS_RULE_Y < deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0).y);
}

void test_dash_strip_fits() {
    // the longest trip line, the block cursor after it
    tripmeter::Trip t;
    t.seconds = 99 * 3600 + 59 * 60;
    t.meters = 999900;
    t.hasDistance = true;
    char s[40];
    tripmeter::line(t, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("TRIP 999.9 KM  99:59", s);
    TEST_ASSERT_TRUE(DASH_X + termfont::advance(Face::SMALL, s) + termfont::cell(Face::SMALL) <= DASH_RIGHT);
    // every message on the line under it, the block cursor after it
    tripmeter::last(t, s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 999.9 KM 99:59", s);
    for (const char* m : {static_cast<const char*>(s), ">NO SYNCED LYRICS", ">INSIDE: NO LINK", ">OUTSIDE: LINKED", ">PURPURA APPLIED",
                          ">LIGHTS: NO LINK", ">WRONG KEY", ">NO SIGNAL", ">STANDBY"}) {
        TEST_ASSERT_TRUE_MESSAGE(DASH_X + termfont::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) <= DASH_RIGHT, m);
    }
    // rows: the rule, the trip line, the message line, all on screen
    const Rows trip = fromBase(Face::SMALL, TRIP_BASE), line = fromBase(Face::SMALL, MSG_BASE);
    const deckmenu::Rect item = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0);
    TEST_ASSERT_TRUE(STRIP_RULE_Y >= deckmenu::MENU_STRIP.y && item.y + item.h < STRIP_RULE_Y);
    between(STRIP_RULE_Y, trip, 240, "trip");
    above(trip, line, "trip over the message");
    TEST_ASSERT_TRUE(line.bottom <= 239);
}

void test_status_fits() {
    // the Lights status line sits on STATUS's bottom (12 rows between the tab rule and the colours):
    // its ink, '/' included, stays inside, and nothing goes below the baseline (the clear is the rect)
    const termfont::Font& f = termfont::font(Face::SMALL);
    for (const char* s : {"LINK 2/2", "OUTSIDE: NO LINK", "INSIDE: NO LINK", "NO LINK", ">PURPURA APPLIED", ">INSIDE: NO LINK",
                          ">OUTSIDE: LINKED", ">LIGHTS: NO LINK", ">NO SYNCED LYRICS", ">WRONG KEY", ">STANDBY", ">NO SIGNAL"}) {
        for (const char* p = s; *p; p++) {
            const termfont::Glyph& g = f.glyph[static_cast<unsigned char>(*p) - f.first];
            if (g.height == 0) continue;
            TEST_ASSERT_TRUE_MESSAGE(-g.yOffset <= deckmenu::STATUS.h, s);
            TEST_ASSERT_TRUE_MESSAGE(g.yOffset + g.height <= 0, s);
        }
    }
    for (const char* s : {"OUTSIDE: NO LINK", ">OUTSIDE: LINKED", ">PURPURA APPLIED"}) fitsLeft(Face::SMALL, s, STATUS_TEXT_X, deckmenu::STATUS.w);
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
    RUN_TEST(test_boot_lines_fit);
    RUN_TEST(test_dash_header_fits);
    RUN_TEST(test_dash_clock_and_weather_fit);
    RUN_TEST(test_dash_lights_fit);
    RUN_TEST(test_dash_strip_fits);
    RUN_TEST(test_status_fits);
    RUN_TEST(test_karaoke_pitches);
    RUN_TEST(test_wrap_with_a_face);
    return UNITY_END();
}
