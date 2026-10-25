#include <unity.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/ui/termfont.h"
#include "../../src/ui/termlayout.h"
#include "../../src/util/dashtext.h"
#include "../../src/util/deckmenu.h"
#include "../../src/util/karaoke.h"
#include "../../src/util/ledlook.h"
#include "../../src/util/ledpresets.h"
#include "../../src/util/musicreadout.h"
#include "../../src/util/otapage.h"
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
        // a tag (KARAOKE's LIVE, MUSIC's ON: SMALL, an inverted mini-label, its caps TAG_PAD from its
        // edges) bottom right, under the label's ink with a row to spare, inside the margins
        for (const char* tag : {LIVE_TAG, ON_TAG}) {
            const int tagW = termfont::width(Face::SMALL, tag) + 2 * TAG_PAD;
            TEST_ASSERT_TRUE_MESSAGE(r.w - TAG_RIGHT - tagW >= MARGIN, tag);
            TEST_ASSERT_TRUE_MESSAGE(ink.bottom + 1 < r.h - TAG_BOTTOM - tagH, label);
        }
    }
    TEST_ASSERT_EQUAL_STRING("[KARAOKE]", MENU_ITEMS[deckmenu::KARAOKE]);
    TEST_ASSERT_EQUAL_STRING("[MUSIC]", MENU_ITEMS[deckmenu::MUSIC]);
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
    // the longest time (hours aren't capped: 999:59 too) clears the longest distance and NO LINK
    for (const char* tm : {static_cast<const char*>(hm), "999:59"}) {
        for (const char* d : {static_cast<const char*>(dist), "-- NO LINK", "0.0 KM"}) {
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
        for (const ledlook::Swatch& sw : ledlook::SWATCHES) {
            char name[16], s[24];
            int k = 0;
            for (; sw.name[k]; k++) name[k] = static_cast<char>(toupper(static_cast<unsigned char>(sw.name[k])));
            name[k] = 0;
            snprintf(s, sizeof(s), "%s 100%%", name);
            fitsLeft(Face::BODY, s, lightTextX(i), right);
        }
        for (const char* s : {"OFF", "-- NO LINK"}) fitsLeft(Face::BODY, s, lightTextX(i), right);
    }
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
                          ">PURPURA APPLIED", ">LIGHTS: NO LINK", ">WRONG KEY", ">NO SIGNAL", ">STANDBY", ">MUSIC ON", ">MUSIC OFF"}) {
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

// Music mode's readout on the prompt line: the longest line, its cursor and the song's three
// colour chips right-aligned at DASH_RIGHT (as tall as SMALL's caps, on the line's baseline).
void test_music_readout_fits() {
    char s[40];
    musicreadout::format(musicreadout::State::Music, 999, 255, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(">MUSIC 999 BPM  LVL 100%", s);
    const int chipsW = 3 * MUSIC_CHIP + 2 * MUSIC_CHIP_GAP;
    for (const char* m : {static_cast<const char*>(s), ">LISTENING", ">NO SIGNAL", ">MUSIC -- BPM  LVL 100%"}) {
        TEST_ASSERT_TRUE_MESSAGE(DASH_X + termfont::advance(Face::SMALL, m) + termfont::cell(Face::SMALL) + MARGIN <=
                                     DASH_RIGHT - chipsW, m);
    }
    TEST_ASSERT_EQUAL_INT(termfont::capHeight(Face::SMALL), MUSIC_CHIP);
    TEST_ASSERT_TRUE(MUSIC_CHIP_GAP >= 2);
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
    RUN_TEST(test_music_readout_fits);
    RUN_TEST(test_status_fits);
    RUN_TEST(test_karaoke_pitches);
    RUN_TEST(test_wrap_with_a_face);
    RUN_TEST(test_update_page_fits);
    return UNITY_END();
}
