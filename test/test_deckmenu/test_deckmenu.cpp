#include <unity.h>
#include <initializer_list>
#include "../../src/util/yellow/deckmenu.h"

using namespace deckmenu;

void setUp() {}
void tearDown() {}

static bool overlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static bool isTab(Screen s, int id) {
    return (s == Screen::Lights || s == Screen::Presets) && id >= L_TAB0 && id < L_TAB0 + N_TABS;
}

static void check_screen(Screen s, const Rect* shown, int nShown) {
    for (int i = 0; i < count(s); i++) {
        const Rect a = rect(s, i);
        if (a.w == 0) continue;
        TEST_ASSERT_TRUE_MESSAGE(a.x >= 0 && a.y >= 0 && a.x + a.w <= 320 && a.y + a.h <= 240, "inside screen");
        // the tabs are the one exception (test_tab_row): four of them and < BACK can't all be 70 wide
        TEST_ASSERT_TRUE_MESSAGE(a.w >= (isTab(s, i) ? 56 : 70) && a.h >= 36, "big enough");
        for (int k = 0; k < nShown; k++) TEST_ASSERT_FALSE_MESSAGE(overlap(a, shown[k]), "over a display rect");
        for (int j = i + 1; j < count(s); j++) {
            const Rect b = rect(s, j);
            if (b.w == 0) continue;
            TEST_ASSERT_FALSE_MESSAGE(overlap(a, b), "buttons overlap");
        }
    }
}

void test_rects_inside_screen_and_apart() {
    const Rect menu[] = {HEADER, CLOCK_BAND, PROMPT_BAND};
    const Rect lights[] = {PROMPT_BAND, LEVEL};
    check_screen(Screen::Menu, menu, 3);
    check_screen(Screen::Lights, lights, 2);
    check_screen(Screen::Presets, lights, 1);  // PROMPT_BAND only: no level row on the PRESETS tab
}

// < BACK and the four tabs (BOTH, INSIDE, OUTSIDE, PRESETS) on the top row, the same on Lights and on
// its PRESETS tab: Back keeps its 70 px, so the tabs share x 80..316 at 56 px, 4 px apart (the focus
// rings' room). Five 70 px buttons would need 5 * 70 + 4 * 4 = 366 px of the 320.
void test_tab_row() {
    TEST_ASSERT_EQUAL_INT(4, N_TABS);
    TEST_ASSERT_TRUE(5 * 70 + 4 * 4 > 320);
    for (Screen s : {Screen::Lights, Screen::Presets}) {
        const Rect back = rect(s, L_BACK);
        TEST_ASSERT_TRUE(back.x == 4 && back.y == 2 && back.w == 70 && back.h == 36);
        int x = back.x + back.w + 6;
        for (int t = 0; t < N_TABS; t++) {
            const Rect r = rect(s, L_TAB0 + t);
            TEST_ASSERT_TRUE(r.x == x && r.y == 2 && r.w == 56 && r.h == 36);
            x += r.w + 4;
        }
        TEST_ASSERT_EQUAL_INT(316 + 4, x);  // the last tab ends 4 px off the screen's right edge
        // a tap in the 4 px between two tabs snaps to the nearer one
        const Rect t1 = rect(s, L_TAB0 + 1);
        TEST_ASSERT_EQUAL_INT(L_TAB0, hit(s, t1.x - 3, 20));
        TEST_ASSERT_EQUAL_INT(L_TAB0 + 1, hit(s, t1.x - 2, 20));
        TEST_ASSERT_EQUAL_INT(L_TAB0 + TAB_PRESETS, hit(s, 319, 20));
    }
    TEST_ASSERT_EQUAL_INT(L_BACK, P_BACK);
    TEST_ASSERT_EQUAL_INT(L_TAB0, P_TAB0);
}

// The colour pages: the 2x4 tiles right under the tabs (clear of their focus rings), the level row
// ([-] bar [+]) under them, ending 4 px above the prompt line's rule.
void test_lights_tiles_fill_the_body() {
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 8, L_MINUS);
    for (int i = 0; i < 8; i++) {
        const Rect r = rect(Screen::Lights, L_TILE0 + i);
        TEST_ASSERT_EQUAL_INT(4 + (i % 4) * 79, r.x);
        TEST_ASSERT_EQUAL_INT(42 + (i / 4) * 66, r.y);
        TEST_ASSERT_TRUE(r.w == 75 && r.h == 62);
        TEST_ASSERT_TRUE(r.y >= 40);
    }
    const Rect minus = rect(Screen::Lights, L_MINUS), plus = rect(Screen::Lights, L_PLUS);
    TEST_ASSERT_TRUE(minus.x == 4 && minus.y == 176 && minus.w == 96 && minus.h == 38);
    TEST_ASSERT_TRUE(plus.x == 220 && plus.y == 176 && plus.w == 96 && plus.h == 38);
    TEST_ASSERT_TRUE(LEVEL.x == 106 && LEVEL.y == 176 && LEVEL.w == 108 && LEVEL.h == 38);
    const Rect t7 = rect(Screen::Lights, L_TILE0 + 7);
    TEST_ASSERT_TRUE(t7.y + t7.h <= minus.y);
    TEST_ASSERT_TRUE(minus.y + minus.h <= 214 && LEVEL.y + LEVEL.h <= 214);
    TEST_ASSERT_EQUAL_INT(L_N, count(Screen::Lights));
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Lights, L_N).w);
}

// The PRESETS tab: the six presets, two rows of three, 100 x 72, 6 px apart side by side and
// 12 px row to row, between the tabs and the prompt line.
void test_presets_tab() {
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 6, P_N);
    TEST_ASSERT_EQUAL_INT(P_N, count(Screen::Presets));
    for (int i = 0; i < 6; i++) {
        const Rect r = rect(Screen::Presets, P_PRE0 + i);
        TEST_ASSERT_EQUAL_INT(4 + (i % 3) * 106, r.x);
        TEST_ASSERT_EQUAL_INT(48 + (i / 3) * 84, r.y);
        TEST_ASSERT_TRUE(r.w == 100 && r.h == 72);
        TEST_ASSERT_TRUE(r.y >= 40 && r.y + r.h <= 214 && r.h >= 60);
        TEST_ASSERT_EQUAL_INT(P_PRE0 + i, hit(Screen::Presets, r.x + 50, r.y + 30));
    }
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Presets, P_N).w);
    // the gaps snap to the nearer preset; the prompt line is display only
    TEST_ASSERT_EQUAL_INT(P_PRE0, hit(Screen::Presets, 106, 100));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 1, hit(Screen::Presets, 107, 100));
    TEST_ASSERT_EQUAL_INT(P_PRE0, hit(Screen::Presets, 50, 125));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 3, hit(Screen::Presets, 50, 126));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Presets, 160, 236));
    for (int y = 0; y < 240; y += 3)
        for (int x = 0; x < 320; x += 3) {
            const int h = hit(Screen::Presets, x, y);
            TEST_ASSERT_TRUE(h >= -1 && h < P_N);
        }
    // the D-pad: 2x3 neighbours, up from the top row to the tabs, nothing past the edges
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 1, neighbour(Screen::Presets, P_PRE0, 1, 0));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 2, neighbour(Screen::Presets, P_PRE0 + 1, 1, 0));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 2, neighbour(Screen::Presets, P_PRE0 + 2, 1, 0));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 3, neighbour(Screen::Presets, P_PRE0 + 0, 0, 1));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 5, neighbour(Screen::Presets, P_PRE0 + 2, 0, 1));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 4, neighbour(Screen::Presets, P_PRE0 + 4, 0, 1));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 1, neighbour(Screen::Presets, P_PRE0 + 4, 0, -1));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 3, neighbour(Screen::Presets, P_PRE0 + 4, -1, 0));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 3, neighbour(Screen::Presets, P_PRE0 + 3, -1, 0));
    const int up = neighbour(Screen::Presets, P_PRE0, 0, -1);
    TEST_ASSERT_TRUE(up == P_BACK || (up >= P_TAB0 && up < P_TAB0 + N_TABS));
    const int down = neighbour(Screen::Presets, P_TAB0 + 1, 0, 1);
    TEST_ASSERT_TRUE(down >= P_PRE0 && down < P_N);
}

// The dashboard (layout B): header, clock + the trip's time, the lights panel (a button), the two
// switches side by side, then the prompt line at the bottom, top to bottom without overlap.
void test_menu_bands_stack() {
    const Rect lights = rect(Screen::Menu, M_ITEM0 + LIGHTS), karaoke = rect(Screen::Menu, M_ITEM0 + KARAOKE),
               music = rect(Screen::Menu, M_ITEM0 + MUSIC);
    TEST_ASSERT_EQUAL_INT(0, HEADER.y);
    TEST_ASSERT_EQUAL_INT(HEADER.y + HEADER.h, CLOCK_BAND.y);
    TEST_ASSERT_TRUE(CLOCK_BAND.y + CLOCK_BAND.h <= lights.y);
    TEST_ASSERT_TRUE(lights.y + lights.h <= karaoke.y);
    TEST_ASSERT_EQUAL_INT(karaoke.y, music.y);
    TEST_ASSERT_TRUE(karaoke.y + karaoke.h <= 214 && music.y + music.h <= 214);  // 4 px above the rule
    TEST_ASSERT_TRUE(PROMPT_BAND.x == 0 && PROMPT_BAND.y == 219 && PROMPT_BAND.w == 320 && PROMPT_BAND.h == 21);
    for (const Rect& r : {HEADER, CLOCK_BAND, PROMPT_BAND}) TEST_ASSERT_TRUE(r.x == 0 && r.w == 320 && r.h > 0);
}

void test_menu_ids_and_hits() {
    TEST_ASSERT_EQUAL_INT(M_N, count(Screen::Menu));
    TEST_ASSERT_EQUAL_INT(3, N_ITEMS);
    TEST_ASSERT_EQUAL_INT(0, M_ITEM0);
    TEST_ASSERT_EQUAL_INT(3, M_N);
    const Rect lights = rect(Screen::Menu, M_ITEM0 + LIGHTS), karaoke = rect(Screen::Menu, M_ITEM0 + KARAOKE),
               music = rect(Screen::Menu, M_ITEM0 + MUSIC);
    TEST_ASSERT_TRUE(lights.x == 4 && lights.y == 58 && lights.w == 312 && lights.h == 86);
    TEST_ASSERT_TRUE(karaoke.x == 4 && karaoke.y == 150 && karaoke.w == 154 && karaoke.h == 64);
    TEST_ASSERT_TRUE(music.x == 162 && music.y == 150 && music.w == 154 && music.h == 64);
    TEST_ASSERT_TRUE(karaoke.w >= 150 && karaoke.h >= 60 && music.w >= 150 && music.h >= 60);
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 160, 100));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 80, 180));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 240, 180));
    // the gap between the switches (x 158..161) snaps to the nearer
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 159, 180));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 160, 180));
    // Out-of-range ids have no rect, and any tap is -1 or a valid id.
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Menu, M_N).w);
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Menu, 99).w);
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Lights, -1).w);
    for (int y = 0; y < 240; y += 3)
        for (int x = 0; x < 320; x += 3) {
            const int h = hit(Screen::Menu, x, y);
            TEST_ASSERT_TRUE(h >= -1 && h < M_N);
        }
}

// The gap between the lights panel (last row 143) and the switches (first row 150) snaps to the
// nearer one.
void test_menu_gap_snaps() {
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 80, 147));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 80, 146));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 240, 148));
}

// The prompt line at the bottom is display only on every screen.
void test_prompt_band_is_no_hit_everywhere() {
    for (Screen s : {Screen::Menu, Screen::Lights, Screen::Presets}) {
        TEST_ASSERT_EQUAL_INT(-1, hit(s, 50, 230));
        for (int y = PROMPT_BAND.y; y < PROMPT_BAND.y + PROMPT_BAND.h; y++)
            for (int x = 0; x < 320; x += 7) TEST_ASSERT_EQUAL_INT(-1, hit(s, x, y));
    }
}

void test_hit_snaps_and_ignores_display_rects() {
    const Rect t0 = rect(Screen::Lights, L_TILE0), t1 = rect(Screen::Lights, L_TILE0 + 1);
    TEST_ASSERT_EQUAL_INT(L_TILE0, hit(Screen::Lights, t0.x + 10, t0.y + 10));
    // In the gap between tiles 0 and 1, one px off tile 1 and four off tile 0.
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 1, hit(Screen::Lights, t1.x - 1, t1.y + 10));
    // Just under the Back button, in the gap above the tiles.
    const Rect back = rect(Screen::Lights, L_BACK);
    TEST_ASSERT_EQUAL_INT(L_BACK, hit(Screen::Lights, back.x + 5, back.y + back.h + 1));
    // Display only: no snapping to a neighbour from inside them.
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Lights, LEVEL.x + 50, LEVEL.y + 20));
    // The dashboard's bands are display only.
    for (const Rect& r : {HEADER, CLOCK_BAND, PROMPT_BAND}) {
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = 0; x < 320; x += 7) TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Menu, x, y));
    }
}

void test_neighbour() {
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 1, neighbour(Screen::Lights, L_TILE0, 1, 0));
    TEST_ASSERT_EQUAL_INT(L_MINUS, neighbour(Screen::Lights, L_TILE0 + 4, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 4, neighbour(Screen::Lights, L_TILE0, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_TAB0, neighbour(Screen::Lights, L_TAB0, 0, -1));
    TEST_ASSERT_EQUAL_INT(L_TAB0 + TAB_PRESETS, neighbour(Screen::Lights, L_TAB0 + TAB_OUTSIDE, 1, 0));
    // right from the last column: nothing lies that way (the PRESETS tab is up, not right)
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 3, neighbour(Screen::Lights, L_TILE0 + 3, 1, 0));
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 7, neighbour(Screen::Lights, L_TILE0 + 7, 1, 0));
    TEST_ASSERT_EQUAL_INT(L_PLUS, neighbour(Screen::Lights, L_TILE0 + 7, 0, 1));
    // down from every bottom tile reaches - or +, and up from them lands back on the tiles
    TEST_ASSERT_EQUAL_INT(L_MINUS, neighbour(Screen::Lights, L_TILE0 + 4, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_MINUS, neighbour(Screen::Lights, L_TILE0 + 5, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_PLUS, neighbour(Screen::Lights, L_TILE0 + 6, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 4, neighbour(Screen::Lights, L_MINUS, 0, -1));
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 7, neighbour(Screen::Lights, L_PLUS, 0, -1));
    TEST_ASSERT_EQUAL_INT(L_TILE0, neighbour(Screen::Lights, L_BACK, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_TILE0, neighbour(Screen::Lights, L_TILE0 + 1, -1, 0));
    // The Menu: the lights panel above, the two switches under it. Down from the panel the first
    // switch (KARAOKE); up from either switch the panel; left / right between the switches; nothing
    // past the edges.
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + LIGHTS, 0, 1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, 0, -1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, 1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, neighbour(Screen::Menu, M_ITEM0 + MUSIC, 0, -1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + MUSIC, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, neighbour(Screen::Menu, M_ITEM0 + MUSIC, 1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, neighbour(Screen::Menu, M_ITEM0 + LIGHTS, 0, -1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, 0, 1));
}

// Presets are the PRESETS tab's only: the colour pages and the Menu have none.
void test_preset_slots() {
    TEST_ASSERT_EQUAL_INT(0, presetSlot(Screen::Presets, P_PRE0));
    TEST_ASSERT_EQUAL_INT(5, presetSlot(Screen::Presets, P_PRE0 + 5));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Presets, P_N));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Presets, P_BACK));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Presets, P_TAB0 + TAB_PRESETS));
    TEST_ASSERT_TRUE(isPreset(Screen::Presets, P_PRE0 + 3));
    TEST_ASSERT_FALSE(isPreset(Screen::Presets, P_TAB0));
    for (int id = -1; id <= 20; id++) {
        TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Lights, id));
        TEST_ASSERT_FALSE(isPreset(Screen::Lights, id));
        TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Menu, id));
        TEST_ASSERT_FALSE(isPreset(Screen::Menu, id));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rects_inside_screen_and_apart);
    RUN_TEST(test_tab_row);
    RUN_TEST(test_lights_tiles_fill_the_body);
    RUN_TEST(test_presets_tab);
    RUN_TEST(test_menu_bands_stack);
    RUN_TEST(test_menu_ids_and_hits);
    RUN_TEST(test_menu_gap_snaps);
    RUN_TEST(test_prompt_band_is_no_hit_everywhere);
    RUN_TEST(test_hit_snaps_and_ignores_display_rects);
    RUN_TEST(test_neighbour);
    RUN_TEST(test_preset_slots);
    return UNITY_END();
}
