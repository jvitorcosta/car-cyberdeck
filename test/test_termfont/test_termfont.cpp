#include <unity.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/yellow/ui/lyricrun.h"
#include "../../src/yellow/ui/termfont.h"
#include "../../src/yellow/ui/termlayout.h"
#include "../../src/util/yellow/dashtext.h"
#include "../../src/util/yellow/deckmenu.h"
#include "../../src/util/yellow/karaoke.h"
#include "../../src/util/lights/ledlook.h"
#include "../../src/util/yellow/ledpresets.h"
#include "../../src/util/yellow/otapage.h"
#include "../../src/util/yellow/tripmeter.h"
#include "../../src/util/common/version.h"
#include "../../src/util/yellow/weather.h"

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
        TEST_ASSERT_EQUAL_UINT16(0xFF, termfont::font(f).last);
        TEST_ASSERT_EQUAL_INT(termfont::cell(f) * 4, termfont::advance(f, "M i."));  // monospace
    }
}

// Latin-1: every face draws the accented letters and the degree sign (bytes 0x80-0xFF count, never
// as negative chars), monospace like ASCII; DEL and the C1 controls (0x7F-0x9F) are empty and don't
// move the pen. The accented capitals rise above the ASCII ascent; nothing goes below its descent.
void test_latin1() {
    const int ascii[] = {12, 13, 16, 24}, descent[] = {3, 3, 4, 6};  // ASCII's, as before Latin-1
    for (int i = 0; i < termfont::N_FACES; i++) {
        const Face f = static_cast<Face>(i);
        for (const char* s : {"\xE3", "\xC3", "\xB0", "\xE7", "\xC9"}) TEST_ASSERT_TRUE_MESSAGE(termfont::width(f, s) > 0, s);
        TEST_ASSERT_EQUAL_INT(termfont::width(f, "SAO"), termfont::width(f, "S\xC3O"));
        TEST_ASSERT_EQUAL_INT(termfont::cell(f) * 3, termfont::advance(f, "\xE3\xB0\xFF"));
        TEST_ASSERT_EQUAL_INT(0, termfont::advance(f, "\x7F\x80\x9F"));
        const termfont::Font& ft = termfont::font(f);
        for (int c = 0x7F; c <= 0x9F; c++) TEST_ASSERT_EQUAL_INT(0, ft.glyph[c - ft.first].height);
        TEST_ASSERT_EQUAL_INT(ascii[i], termfont::ascent(f));
        TEST_ASSERT_EQUAL_INT(descent[i], termfont::descent(f));
        const termfont::Glyph& a = ft.glyph[0xC3 - ft.first];  // Ã: its tilde over the caps, past ASCII
        TEST_ASSERT_TRUE(-a.yOffset > termfont::ascent(f));
        TEST_ASSERT_TRUE(termfont::accentAscent(f) >= -a.yOffset);
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
    TEST_ASSERT_EQUAL_INT(deckmenu::N_ITEMS, N_MENU_ITEMS);
    const int tagH = termfont::capHeight(Face::SMALL) + 2 * TAG_PAD;
    TEST_ASSERT_TRUE(TAG_PAD >= 2 && TAG_RIGHT >= MARGIN && TAG_BOTTOM >= MARGIN);
    for (int i = 0; i < deckmenu::N_ITEMS; i++) {
        const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0 + i);
        TEST_ASSERT_EQUAL_INT(100, r.w);
        TEST_ASSERT_TRUE(r.h == 44 && r.y + r.h <= 240 - MARGIN);  // at the bottom, 4 px off the edge
        // the label (BODY) top left, its ink inside the margins
        const char* label = MENU_ITEMS[i];
        fitsLeft(Face::BODY, label, ROW_TEXT_X, r.w);
        const Rows ink = fromCapTop(Face::BODY, ROW_TEXT_Y);
        TEST_ASSERT_TRUE_MESSAGE(ink.top >= MARGIN, label);
        // a tag (LYRICS's LIVE, MUSIC MODE's ON / OFF: SMALL, an inverted mini-label, its caps TAG_PAD from its
        // edges) bottom right, under the label's ink with a row to spare, inside the margins
        for (const char* tag : {LIVE_TAG, ON_TAG, OFF_TAG}) {
            const int tagW = termfont::width(Face::SMALL, tag) + 2 * TAG_PAD;
            TEST_ASSERT_TRUE_MESSAGE(r.w - TAG_RIGHT - tagW >= MARGIN, tag);
            TEST_ASSERT_TRUE_MESSAGE(ink.bottom + 1 < r.h - TAG_BOTTOM - tagH, label);
        }
    }
    TEST_ASSERT_EQUAL_STRING("[LYRICS]", MENU_ITEMS[deckmenu::KARAOKE]);
    TEST_ASSERT_EQUAL_STRING("MUSIC MODE", MENU_ITEMS[deckmenu::MUSIC]);
}

void test_preset_names_fit() {
    TEST_ASSERT_EQUAL_INT(deckmenu::P_N - deckmenu::P_PRE0, ledpresets::N);
    for (int i = 0; i < ledpresets::N; i++) {
        const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Presets, deckmenu::P_PRE0 + i);
        TEST_ASSERT_TRUE(r.w == 100 && r.h == 60);
        fitsCentred(Face::TITLE, ledpresets::PRESETS[i].name, r.w);  // "PURPURA", the widest: 69 px
    }
    // the name over the two chips (16-20 px squares), the chips' 1 px edge on a bar clear of the
    // name's ink and the margin, the pair centred within the margins
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Presets, deckmenu::P_PRE0);
    TEST_ASSERT_TRUE(PRESET_CHIP >= 16 && PRESET_CHIP <= 20);
    TEST_ASSERT_TRUE(fromCapTop(Face::TITLE, PRESET_NAME_Y).bottom < PRESET_CHIP_Y - 1);
    TEST_ASSERT_TRUE(PRESET_NAME_Y >= MARGIN);  // the names are capitals: their ink starts at the caps
    TEST_ASSERT_TRUE(PRESET_CHIP_Y + PRESET_CHIP <= r.h - MARGIN);
    TEST_ASSERT_TRUE(2 * PRESET_CHIP + PRESET_CHIP_GAP <= r.w - 2 * MARGIN);
}

void test_colour_names_fit() {
    const deckmenu::Rect r = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_TILE0);
    TEST_ASSERT_EQUAL_INT(75, r.w);
    TEST_ASSERT_TRUE(TILE_CHIP_X + TILE_CHIP_W + MARGIN <= TILE_TEXT_X);
    TEST_ASSERT_TRUE(TILE_CHIP_H <= r.h - 2 * MARGIN);
    TEST_ASSERT_TRUE(TILE_CHIP_X >= MARGIN && TILE_CHIP_W == 16 && TILE_CHIP_H == 48);
    for (int k = 0; k < ledlook::N_GRID; k++) {  // the grid's colours (light blue is a preset's only)
        const ledlook::Swatch& s = ledlook::SWATCHES[k];
        char name[16];
        size_t i = 0;
        for (; s.name[i]; i++) name[i] = static_cast<char>(toupper(static_cast<unsigned char>(s.name[i])));
        name[i] = 0;
        fitsLeft(Face::BODY, name, TILE_TEXT_X, r.w);  // one face with the preset names
    }
}

void test_chrome_fits() {
    fitsCentred(Face::BODY, "< BACK", deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_BACK).w);
    // the four tabs are SMALL: at 56 px, BODY's "PRESETS" (55 px) would touch the edges
    const int tab = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_TAB0).w;
    TEST_ASSERT_EQUAL_INT(56, tab);
    TEST_ASSERT_EQUAL_INT(deckmenu::N_TABS, N_TAB_LABELS);
    TEST_ASSERT_EQUAL_STRING("PRESETS", TAB_LABELS[deckmenu::TAB_PRESETS]);
    TEST_ASSERT_TRUE(termfont::width(Face::BODY, "PRESETS") > tab - 2 * MARGIN);
    for (const char* s : TAB_LABELS) fitsCentred(Face::SMALL, s, tab);
    const int pm = deckmenu::rect(deckmenu::Screen::Lights, deckmenu::L_MINUS).w;
    for (const char* s : {"[-]", "[+]"}) fitsCentred(Face::TITLE, s, pm);
    for (const char* s : {"100%", "MIX", "OFF"}) fitsCentred(Face::TITLE, s, deckmenu::LEVEL.w);
}

// The boot's typed lines: on screen, none over the next.
void test_boot_lines_fit() {
    for (const char* s : BOOT_LINES) fitsLeft(Face::SMALL, s, BOOT_X, 320);
    // the build's line under them: this one, and a long dev build's
    fitsLeft(Face::SMALL, FW_LABEL, BOOT_X, 320);
    fitsLeft(Face::SMALL, "FW 10.10.10+1234.gabcdef0.dirty", BOOT_X, 320);
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
    for (const char* s : {"--\xB0" "C", "-10\xB0" "C", "32\xB0" "C", "40\xB0" "C"}) fitsRight(Face::BIG, s, DASH_RIGHT, DASH_SPLIT);
    for (int code : {0, 1, 3, 45, 51, 61, 71, 80, 85, 95, 999}) fitsRight(Face::SMALL, weather::words(code), DASH_RIGHT, DASH_SPLIT);
    for (const char* s : {"PARTLY CLOUDY", "THUNDERSTORM", "SNOW SHOWERS"}) fitsRight(Face::SMALL, s, DASH_RIGHT, DASH_SPLIT);
    // rows: the clock over the date, both over the band's last row (HI / LO), all
    // between the header rule and the band's
    const Rows clock = fromCapTop(Face::BIG, CLOCK_Y), date = fromCapTop(Face::SMALL, DATE_Y);
    const Rows words = fromCapTop(Face::SMALL, WORDS_Y), row = fromBase(Face::SMALL, HILO_BASE);
    between(HEADER_RULE_Y, clock, CLOCK_RULE_Y, "clock");
    above(clock, date, "clock over the date");
    above(clock, words, "temperature over the sky");
    above(date, row, "date over the HI / LO row");
    above(words, row, "sky over the HI / LO row");
    between(HEADER_RULE_Y, row, CLOCK_RULE_Y, "HI / LO row");
    // the band's rule is the first row of the trip band under it (each band starts on the rule above)
    TEST_ASSERT_EQUAL_INT(deckmenu::TRIP_BAND.y, CLOCK_RULE_Y);
    // the caps spaced evenly: rule, clock, date, row, rule (within a row of each other)
    const int gaps[] = {CLOCK_Y - (HEADER_RULE_Y + 1), DATE_Y - (CLOCK_Y + termfont::capHeight(Face::BIG)),
                        (HILO_BASE - termfont::capHeight(Face::SMALL)) - (DATE_Y + termfont::capHeight(Face::SMALL)),
                        CLOCK_RULE_Y - HILO_BASE};
    for (int g : gaps) {
        for (int h : gaps) TEST_ASSERT_TRUE(g - h <= 1 && h - g <= 1);
    }
    // the split clears: the left column's clear ends before the right column's ink starts
    TEST_ASSERT_TRUE(DASH_SPLIT < DASH_RIGHT - termfont::width(Face::BIG, "-10\xB0" "C"));
    // the HI / LO row's clear reaches up to an accented capital in the HI / LO city ("SÃO PAULO") and
    // still starts under the date's and the sky's ink (capitals: nothing below their baseline)
    TEST_ASSERT_TRUE(HILO_BASE - termfont::accentAscent(Face::SMALL) > DATE_Y + termfont::capHeight(Face::SMALL) - 1);
    TEST_ASSERT_TRUE(HILO_BASE - termfont::accentAscent(Face::SMALL) > WORDS_Y + termfont::capHeight(Face::SMALL) - 1);
}

// The HI / LO row (the clock band's last): right-aligned, alone on its row, so even the longest
// city a reading holds (23 characters) fits with the coldest day's temperatures; hiLoFits still
// guards it (ledpanel drops the city when it says no).
void test_dash_hilo_row_fits() {
    char s[48];
    dashtext::hiLo(true, -10, -20, "ABCDEFGHIJKLMNOPQRSTUVW", s, sizeof(s));
    TEST_ASSERT_EQUAL_STRING("HI -10 LO -20  ABCDEFGHIJKLMNOPQRSTUVW", s);
    TEST_ASSERT_TRUE(hiLoFits(s));
    fitsRight(Face::SMALL, s, DASH_RIGHT, DASH_X - MARGIN);
    dashtext::hiLo(true, -10, -20, nullptr, s, sizeof(s));
    TEST_ASSERT_TRUE(hiLoFits(s));
    for (const char* w : {"HI -- LO --", ">OFFLINE"}) TEST_ASSERT_TRUE_MESSAGE(hiLoFits(w), w);
    TEST_ASSERT_FALSE(hiLoFits("HI -10 LO -20  ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGH"));  // what it guards
}

// The trip band: "TRIP" (SMALL), the time (BIG) after it, the distance (BODY) right-aligned, all on
// TRIP_BASE, the caps centred between the clock band's rule and the band's own.
void test_dash_trip_band_fits() {
    tripmeter::Trip t;
    t.seconds = 99 * 3600 + 59 * 60;
    t.meters = 999900;
    t.hasDistance = true;
    char hm[16], dist[16];
    tripmeter::elapsed(t, hm, sizeof(hm));
    tripmeter::distance(t, dist, sizeof(dist));
    TEST_ASSERT_EQUAL_STRING("99:59", hm);
    TEST_ASSERT_EQUAL_STRING("999.9 KM", dist);
    TEST_ASSERT_TRUE(tripTimeX() >= DASH_X + termfont::width(Face::SMALL, TRIP_LABEL) + MARGIN);
    // the longest time (hours aren't capped: 999:59 too) clears the longest distance and "-- KM"
    for (const char* tm : {static_cast<const char*>(hm), "999:59"}) {
        for (const char* d : {static_cast<const char*>(dist), "-- KM", "0.0 KM"}) {
            fitsRight(Face::BODY, d, DASH_RIGHT, tripTimeX() + termfont::width(Face::BIG, tm));
        }
    }
    // rows: BIG is the tallest face on the baseline; the band starts on the clock band's rule, its
    // own rule starts the lights band
    const Rows big = fromBase(Face::BIG, TRIP_BASE);
    TEST_ASSERT_TRUE(fromBase(Face::BODY, TRIP_BASE).top >= big.top && fromBase(Face::SMALL, TRIP_BASE).top >= big.top);
    between(CLOCK_RULE_Y, big, TRIP_RULE_Y, "trip");
    TEST_ASSERT_EQUAL_INT(deckmenu::TRIP_BAND.y, CLOCK_RULE_Y);
    TEST_ASSERT_EQUAL_INT(deckmenu::LIGHTS_BAND.y, TRIP_RULE_Y);
    const int gapTop = (TRIP_BASE - termfont::capHeight(Face::BIG)) - (CLOCK_RULE_Y + 1);
    const int gapBottom = TRIP_RULE_Y - TRIP_BASE;
    TEST_ASSERT_TRUE(gapTop - gapBottom <= 1 && gapBottom - gapTop <= 1);
}

// The lights row: IN left, OUT right of the vertical rule at LIGHTS_SPLIT, each its label (BODY), a
// chip of its colour on BODY's caps, then the colour and the level ("YELLOW 100%"), OFF or -- NO
// LINK (BODY), all on one baseline.
void test_dash_lights_fit() {
    TEST_ASSERT_EQUAL_INT(2, N_LIGHT_LABELS);
    TEST_ASSERT_EQUAL_STRING("IN", LIGHT_LABELS[0]);
    TEST_ASSERT_EQUAL_STRING("OUT", LIGHT_LABELS[1]);
    TEST_ASSERT_EQUAL_INT(termfont::capHeight(Face::BODY), LIGHT_CHIP_H);
    TEST_ASSERT_TRUE(LIGHT_GAP >= MARGIN);
    // each half starts the dashboard's margin in from its left (the screen's edge, the rule)
    TEST_ASSERT_EQUAL_INT(DASH_X, lightX(0));
    TEST_ASSERT_EQUAL_INT(LIGHTS_SPLIT + DASH_X, lightX(1));
    for (int i = 0; i < N_LIGHT_LABELS; i++) {
        TEST_ASSERT_TRUE(lightChipX(i) >= lightX(i) + termfont::width(Face::BODY, LIGHT_LABELS[i]) + MARGIN);
        TEST_ASSERT_EQUAL_INT(lightChipX(i) + LIGHT_CHIP_W + LIGHT_GAP, lightTextX(i));
        const int right = i == 0 ? LIGHTS_SPLIT : DASH_RIGHT + MARGIN;  // the rule; the rules' end
        for (int n = 0; n < ledlook::N_SWATCH; n++) {
            const ledlook::Swatch& sw = ledlook::SWATCHES[n];
            char name[16], s[24];
            int k = 0;
            for (; sw.name[k]; k++) name[k] = static_cast<char>(toupper(static_cast<unsigned char>(sw.name[k])));
            name[k] = 0;
            if (n == ledlook::LIGHT_BLUE) snprintf(name, sizeof(name), "%s", LIGHT_BLUE_SHORT);  // CYAN
            snprintf(s, sizeof(s), "%s 100%%", name);
            fitsLeft(Face::BODY, s, lightTextX(i), right);
        }
        for (const char* s : {"OFF", "-- NO LINK"}) fitsLeft(Face::BODY, s, lightTextX(i), right);
    }
    TEST_ASSERT_EQUAL_STRING("CYAN", LIGHT_BLUE_SHORT);  // what voice takes for it too ("cyan")
    // the longest case, pinned: "OUT [chip] YELLOW 100%" is about 136 px, in its half's 145
    const int outRow = lightTextX(1) + termfont::width(Face::BODY, "YELLOW 100%") - lightX(1);
    TEST_ASSERT_TRUE(outRow >= 130 && outRow <= 140);
    TEST_ASSERT_TRUE(lightX(1) + outRow <= DASH_RIGHT);
    // the row between the trip band's rule and its own, caps centred; the clear reaches an accented
    // capital's top; the band's rule starts the prompt line's band
    const Rows row = fromBase(Face::BODY, LIGHT_ROW_BASE);
    between(TRIP_RULE_Y, row, LIGHTS_RULE_Y, "lights");
    TEST_ASSERT_TRUE(LIGHT_ROW_BASE - termfont::accentAscent(Face::BODY) > TRIP_RULE_Y);
    const int gapTop = (LIGHT_ROW_BASE - termfont::capHeight(Face::BODY)) - (TRIP_RULE_Y + 1);
    const int gapBottom = LIGHTS_RULE_Y - LIGHT_ROW_BASE;
    TEST_ASSERT_TRUE(gapTop - gapBottom <= 1 && gapBottom - gapTop <= 1);
    TEST_ASSERT_EQUAL_INT(deckmenu::LIGHTS_BAND.y, TRIP_RULE_Y);
    TEST_ASSERT_EQUAL_INT(deckmenu::PROMPT_BAND.y, LIGHTS_RULE_Y);
}

// The prompt line (SMALL): ">" idle, every message, the block cursor after it, inside its band
// (accented capitals included: a message can carry a colour's name), the caps centred between the
// lights band's rule and the items.
void test_dash_prompt_fits() {
    tripmeter::Trip t;
    t.seconds = 99 * 3600 + 59 * 60;
    t.meters = 999900;
    t.hasDistance = true;
    char last[40];
    tripmeter::last(t, last, sizeof(last));
    TEST_ASSERT_EQUAL_STRING(">LAST TRIP 999.9 KM 99:59", last);
    for (const char* m : {static_cast<const char*>(last), PROMPT_IDLE, ">NO SYNCED LYRICS", ">INSIDE: NO LINK", ">OUTSIDE: LINKED",
                          ">PURPURA APPLIED", ">LIGHTS: NO LINK", ">WRONG KEY", ">LYRICS: NO SIGNAL", ">NOTHING PLAYING",
                          ">MUSIC ON", ">MUSIC OFF", MUSIC_WAITING, MUSIC_NO_SIGNAL}) {
        TEST_ASSERT_TRUE_MESSAGE(DASH_X + termfont::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) <= DASH_RIGHT, m);
    }
    const deckmenu::Rect& band = deckmenu::PROMPT_BAND;
    TEST_ASSERT_TRUE(PROMPT_BASE - termfont::accentAscent(Face::SMALL) > LIGHTS_RULE_Y);
    TEST_ASSERT_TRUE(PROMPT_BASE + termfont::descent(Face::SMALL) <= band.y + band.h);
    const int items = deckmenu::rect(deckmenu::Screen::Menu, deckmenu::M_ITEM0).y;
    const int gapTop = (PROMPT_BASE - termfont::capHeight(Face::SMALL)) - (LIGHTS_RULE_Y + 1);
    const int gapBottom = items - PROMPT_BASE;
    TEST_ASSERT_TRUE(gapTop - gapBottom <= 1 && gapBottom - gapTop <= 1);
}

// Music mode's line on the prompt line: MUSIC_PROMPT, the swatch's chip (as tall as SMALL's caps, on
// the line's baseline, inside the prompt band) and its name, the longest one's cursor still by
// DASH_RIGHT; WAITING and NO SIGNAL alone. The texts are the owner's (pinned), NO SIGNAL apart from
// LYRICS's.
void test_song_colours_fit() {
    TEST_ASSERT_EQUAL_STRING(">MUSIC", MUSIC_PROMPT);
    TEST_ASSERT_EQUAL_STRING(">MUSIC: WAITING", MUSIC_WAITING);
    TEST_ASSERT_EQUAL_STRING(">MUSIC: NO SIGNAL", MUSIC_NO_SIGNAL);
    TEST_ASSERT_TRUE(songChipX() >= DASH_X + termfont::advance(Face::SMALL, MUSIC_PROMPT) + 2);
    TEST_ASSERT_TRUE(songNameX() >= songChipX() + MUSIC_CHIP + 2);
    for (int k = 0; k < ledlook::N_GRID; k++) {  // a song's colour is always one of the grid's
        const ledlook::Swatch& sw = ledlook::SWATCHES[k];
        char name[16];
        size_t i = 0;
        for (; sw.name[i] && i + 1 < sizeof name; i++) name[i] = static_cast<char>(toupper(static_cast<unsigned char>(sw.name[i])));
        name[i] = 0;
        TEST_ASSERT_TRUE_MESSAGE(songNameX() + termfont::advance(Face::SMALL, name) + termfont::cell(Face::SMALL) + MARGIN <= DASH_RIGHT,
                                 name);
    }
    for (const char* m : {MUSIC_WAITING, MUSIC_NO_SIGNAL}) {
        TEST_ASSERT_TRUE_MESSAGE(DASH_X + termfont::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) <= DASH_RIGHT, m);
    }
    TEST_ASSERT_EQUAL_INT(termfont::capHeight(Face::SMALL), MUSIC_CHIP);
    // the chip's rows (PROMPT_BASE - MUSIC_CHIP .. PROMPT_BASE - 1) inside the band, clear of its rule
    const deckmenu::Rect& band = deckmenu::PROMPT_BAND;
    TEST_ASSERT_TRUE(PROMPT_BASE - MUSIC_CHIP > band.y);
    TEST_ASSERT_TRUE(PROMPT_BASE <= band.y + band.h);
}

void test_status_fits() {
    // the Lights status line sits on STATUS's bottom (12 rows between the tab rule and the colours):
    // its ink, '/' included, stays inside, and nothing goes below the baseline (the clear is the rect)
    const termfont::Font& f = termfont::font(Face::SMALL);
    for (const char* s : {"LINK 2/2", "OUTSIDE: NO LINK", "INSIDE: NO LINK", "NO LINK", ">PURPURA APPLIED", ">INSIDE: NO LINK",
                          ">OUTSIDE: LINKED", ">LIGHTS: NO LINK", ">NO SYNCED LYRICS", ">WRONG KEY", ">NOTHING PLAYING",
                          ">LYRICS: NO SIGNAL"}) {
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
    fitsCentred(Face::BIG, ">NOTHING PLAYING", KARAOKE_W + 2 * MARGIN);  // the state words, on one line
    fitsCentred(Face::BIG, ">LYRICS: NO SIGNAL", KARAOKE_W + 2 * MARGIN);
    // ledpanel clears a lyric area from as high as its face's accented capitals rise over its first
    // line: the big area (BIG or TITLE, from y 56) into the 12 rows under the body's top (y 44), the
    // next line's (BODY, from y 156) into the 10 rows under the big area (to y 146)
    auto rise = [](Face f) { return termfont::accentAscent(f) - termfont::ascent(f); };
    TEST_ASSERT_TRUE(rise(Face::TITLE) <= rise(Face::BIG) && rise(Face::BIG) <= 12);
    TEST_ASSERT_TRUE(rise(Face::BODY) <= 10);
    // the song row (BODY, upper case: "SÃO JOÃO - ..."), its caps centred on y 20 (ML_DATUM, so the
    // baseline at 20 + cap / 2), inside its clear (rows 2..37) with the accents and the descenders
    const int songBase = 20 + termfont::capHeight(Face::BODY) / 2;
    TEST_ASSERT_TRUE(songBase - termfont::accentAscent(Face::BODY) >= 2);
    TEST_ASSERT_TRUE(songBase + termfont::descent(Face::BODY) - 1 <= 2 + 36 - 1);
}

// Japanese in the lyric areas (ui/lyricrun's glyphs, 32 rows in BIG, 16 elsewhere, centred on the
// caps): every block the big area draws (BIG, BIG holding Japanese, TITLE; 1 line up to the most)
// stays inside its clear with its accents and its glyphs, and that clear inside the body (y 44) and
// above the next line's; the next line's and the song row's glyphs stay inside their clears.
void test_karaoke_japanese_fits() {
    struct Block { Face f; int dy, most; };
    for (const Block b : {Block{Face::BIG, KARAOKE_BIG_DY, 3}, Block{Face::BIG, KARAOKE_WIDE_DY, 3},
                          Block{Face::TITLE, KARAOKE_FIT_DY, 4}}) {
        const int glyph = lyricrun::GLYPH_H * lyricrun::scale(b.f);
        for (int n = 1; n <= b.most; n++) {
            const int y = karaokeBlockY(n, b.dy);
            const int base0 = y + termfont::ascent(b.f), baseN = base0 + (n - 1) * b.dy;
            TEST_ASSERT_TRUE(y - (termfont::accentAscent(b.f) - termfont::ascent(b.f)) >= KARAOKE_BIG_CLEAR_Y);
            TEST_ASSERT_TRUE(lyricrun::glyphTop(b.f, base0) >= KARAOKE_BIG_CLEAR_Y);
            TEST_ASSERT_TRUE(baseN + termfont::descent(b.f) <= KARAOKE_BIG_CLEAR_END);
            TEST_ASSERT_TRUE(lyricrun::glyphTop(b.f, baseN) + glyph <= KARAOKE_BIG_CLEAR_END);
        }
    }
    // a line of 2x glyphs (32 rows) fits the wide pitch, a 1x one TITLE's line box
    TEST_ASSERT_EQUAL_INT(KARAOKE_WIDE_DY, 2 * lyricrun::GLYPH_H);
    TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::TITLE, 100) >= 100 - termfont::ascent(Face::TITLE));
    TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::TITLE, 100) + lyricrun::GLYPH_H <= 100 + termfont::descent(Face::TITLE));
    TEST_ASSERT_TRUE(KARAOKE_BIG_CLEAR_Y >= deckmenu::KARAOKE_BODY.y);
    TEST_ASSERT_TRUE(KARAOKE_BIG_CLEAR_END <= KARAOKE_NEXT_Y - (termfont::accentAscent(Face::BODY) - termfont::ascent(Face::BODY)));
    // the next line (BODY, 1x): inside its area
    for (int i = 0; i < 2; i++) {
        const int base = KARAOKE_NEXT_Y + i * KARAOKE_NEXT_DY + termfont::ascent(Face::BODY);
        TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::BODY, base) >= KARAOKE_NEXT_Y);
        TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::BODY, base) + lyricrun::GLYPH_H <= KARAOKE_NEXT_Y + KARAOKE_NEXT_H);
    }
    // the song row (BODY, 1x, caps centred on y 20) inside its clear, rows 2..37
    const int songBase = 20 + termfont::capHeight(Face::BODY) / 2;
    TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::BODY, songBase) >= 2);
    TEST_ASSERT_TRUE(lyricrun::glyphTop(Face::BODY, songBase) + lyricrun::GLYPH_H <= 2 + 36);
}

// lyricrun::wrap measures with the face it draws in.
void test_wrap_with_a_face() {
    lyricrun::Item items[64];
    const size_t n = lyricrun::decode("I walked along the empty road under a sky of stars", items, 64, nullptr, nullptr);
    lyricrun::Span lines[3];
    bool cut = true;
    const int k = lyricrun::wrap(items, n, Face::BIG, KARAOKE_W, 3, lines, &cut);
    TEST_ASSERT_TRUE(k >= 2 && k <= 3);
    TEST_ASSERT_FALSE(cut);
    for (int i = 0; i < k; i++) {
        char s[64];
        size_t m = 0;
        for (size_t j = lines[i].start; j < lines[i].end; j++) s[m++] = static_cast<char>(items[j].cp);
        s[m] = 0;
        TEST_ASSERT_EQUAL_INT(termfont::width(Face::BIG, s), lyricrun::width(Face::BIG, items + lines[i].start, m));
        TEST_ASSERT_TRUE(termfont::width(Face::BIG, s) <= KARAOKE_W);
    }
}

// The update page (an update over WiFi): under the dashboard's header, the title (BIG, centred) over
// the 10-cell bar over the % (TITLE, centred), all on the 320x240 screen.
void test_update_page_fits() {
    for (otaview::Phase p : {otaview::Phase::Receiving, otaview::Phase::Done, otaview::Phase::Failed}) {
        fitsCentred(Face::BIG, otapage::title(p), 320);
    }
    for (const char* s : {"0%", "57%", "100%"}) fitsCentred(Face::TITLE, s, 320);
    between(HEADER_RULE_Y, fromCapTop(Face::BIG, UPDATE_TITLE_Y), UPDATE_BAR_Y, "update title");
    TEST_ASSERT_TRUE(UPDATE_BAR_W == otapage::CELLS * (UPDATE_CELL + UPDATE_GAP) - UPDATE_GAP);
    TEST_ASSERT_TRUE(UPDATE_BAR_W <= 320 - 2 * DASH_X);
    between(UPDATE_BAR_Y + UPDATE_BAR_H - 1, fromCapTop(Face::TITLE, UPDATE_PCT_Y), 240, "update %");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_width_follows_tft_espi);
    RUN_TEST(test_metrics);
    RUN_TEST(test_latin1);
    RUN_TEST(test_bitmap_packing);
    RUN_TEST(test_menu_items_fit);
    RUN_TEST(test_preset_names_fit);
    RUN_TEST(test_colour_names_fit);
    RUN_TEST(test_chrome_fits);
    RUN_TEST(test_boot_lines_fit);
    RUN_TEST(test_dash_header_fits);
    RUN_TEST(test_dash_clock_and_weather_fit);
    RUN_TEST(test_dash_hilo_row_fits);
    RUN_TEST(test_dash_trip_band_fits);
    RUN_TEST(test_dash_lights_fit);
    RUN_TEST(test_dash_prompt_fits);
    RUN_TEST(test_song_colours_fit);
    RUN_TEST(test_status_fits);
    RUN_TEST(test_karaoke_pitches);
    RUN_TEST(test_karaoke_japanese_fits);
    RUN_TEST(test_wrap_with_a_face);
    RUN_TEST(test_update_page_fits);
    return UNITY_END();
}
