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
    const Rect menu[] = {HEADER, CLOCK_BAND, TRIP_BAND, LIGHTS_BAND, PROMPT_BAND};
    const Rect lights[] = {STATUS, LEVEL};
    const Rect karaoke[] = {KARAOKE_BODY};
    check_screen(Screen::Menu, menu, 5);
    check_screen(Screen::Lights, lights, 2);
    check_screen(Screen::Karaoke, karaoke, 1);
    check_screen(Screen::Presets, lights, 1);  // STATUS only: no level row on the PRESETS tab
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

// The colour pages: the 2x4 tiles take the height the old preset row had (x 4..316, y 54..186),
// the level row ([-] bar [+]) right under them where it was.
void test_lights_tiles_fill_the_body() {
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 8, L_MINUS);
    for (int i = 0; i < 8; i++) {
        const Rect r = rect(Screen::Lights, L_TILE0 + i);
        TEST_ASSERT_EQUAL_INT(4 + (i % 4) * 79, r.x);
        TEST_ASSERT_EQUAL_INT(54 + (i / 4) * 68, r.y);
        TEST_ASSERT_TRUE(r.w == 75 && r.h == 64);
    }
    TEST_ASSERT_EQUAL_INT(186, rect(Screen::Lights, L_TILE0 + 7).y + rect(Screen::Lights, L_TILE0 + 7).h);
    const Rect minus = rect(Screen::Lights, L_MINUS), plus = rect(Screen::Lights, L_PLUS);
    TEST_ASSERT_TRUE(minus.x == 4 && minus.y == 190 && minus.w == 96 && minus.h == 46);
    TEST_ASSERT_TRUE(plus.x == 220 && plus.y == 190 && plus.w == 96 && plus.h == 46);
    TEST_ASSERT_TRUE(LEVEL.x == 106 && LEVEL.y == 190 && LEVEL.w == 108 && LEVEL.h == 46);
    TEST_ASSERT_EQUAL_INT(L_N, count(Screen::Lights));
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Lights, L_N).w);
}

// The PRESETS tab: the six presets, two rows of three, 100 x 60, 6 px apart side by side (as the
// Menu's items) and 10 px row to row, the block centred under the status line.
void test_presets_tab() {
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 6, P_N);
    TEST_ASSERT_EQUAL_INT(P_N, count(Screen::Presets));
    for (int i = 0; i < 6; i++) {
        const Rect r = rect(Screen::Presets, P_PRE0 + i);
        TEST_ASSERT_EQUAL_INT(4 + (i % 3) * 106, r.x);
        TEST_ASSERT_EQUAL_INT(80 + (i / 3) * 70, r.y);
        TEST_ASSERT_TRUE(r.w == 100 && r.h == 60);
        TEST_ASSERT_EQUAL_INT(P_PRE0 + i, hit(Screen::Presets, r.x + 50, r.y + 30));
    }
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Presets, P_N).w);
    // the gaps snap to the nearer preset; the status line is display only; far below the block, nothing
    TEST_ASSERT_EQUAL_INT(P_PRE0, hit(Screen::Presets, 106, 100));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 1, hit(Screen::Presets, 107, 100));
    TEST_ASSERT_EQUAL_INT(P_PRE0, hit(Screen::Presets, 50, 144));
    TEST_ASSERT_EQUAL_INT(P_PRE0 + 3, hit(Screen::Presets, 50, 145));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Presets, 50, 45));
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

// The dashboard: header, clock + weather (with the HI / LO row), the trip, the lights, the prompt
// line, then the three items at the bottom, top to bottom, each band right under the one before.
void test_menu_bands_stack() {
    const Rect bands[] = {HEADER, CLOCK_BAND, TRIP_BAND, LIGHTS_BAND, PROMPT_BAND};
    TEST_ASSERT_EQUAL_INT(0, HEADER.y);
    for (int i = 1; i < 5; i++) TEST_ASSERT_EQUAL_INT(bands[i - 1].y + bands[i - 1].h, bands[i].y);
    for (const Rect& r : bands) TEST_ASSERT_TRUE(r.x == 0 && r.w == 320 && r.h > 0);
    const Rect items = rect(Screen::Menu, M_ITEM0);
    TEST_ASSERT_EQUAL_INT(items.y - 4, PROMPT_BAND.y + PROMPT_BAND.h);  // 4 rows that snap to the items
    TEST_ASSERT_EQUAL_INT(236, items.y + items.h);  // 4 px off the screen's bottom, as off its sides
}

void test_menu_ids_and_hits() {
    TEST_ASSERT_EQUAL_INT(M_N, count(Screen::Menu));
    TEST_ASSERT_EQUAL_INT(3, N_ITEMS);
    TEST_ASSERT_EQUAL_INT(0, M_ITEM0);
    TEST_ASSERT_EQUAL_INT(3, M_N);
    // LIGHTS, KARAOKE and MUSIC side by side, finger-sized.
    const Rect lights = rect(Screen::Menu, M_ITEM0 + LIGHTS), karaoke = rect(Screen::Menu, M_ITEM0 + KARAOKE),
               music = rect(Screen::Menu, M_ITEM0 + MUSIC);
    TEST_ASSERT_TRUE(lights.x == 4 && lights.y == 192 && lights.w == 100 && lights.h == 44);
    TEST_ASSERT_TRUE(karaoke.x == 110 && karaoke.y == 192 && karaoke.w == 100 && karaoke.h == 44);
    TEST_ASSERT_TRUE(music.x == 216 && music.y == 192 && music.w == 100 && music.h == 44);
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 50, 214));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 160, 214));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 270, 214));
    // the gaps between them (x 104..109, 210..215) snap to the nearer
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 106, 214));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 107, 214));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 212, 214));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 213, 214));
    // just above the row (under the prompt line) and below it (the screen's last rows), still the item
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 50, 188));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, hit(Screen::Menu, 50, 189));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, hit(Screen::Menu, 160, 239));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, hit(Screen::Menu, 319, 239));
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

void test_hit_snaps_and_ignores_display_rects() {
    const Rect t0 = rect(Screen::Lights, L_TILE0), t1 = rect(Screen::Lights, L_TILE0 + 1);
    TEST_ASSERT_EQUAL_INT(L_TILE0, hit(Screen::Lights, t0.x + 10, t0.y + 10));
    // In the gap between tiles 0 and 1, one px off tile 1 and four off tile 0.
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 1, hit(Screen::Lights, t1.x - 1, t1.y + 10));
    // Just under the Back button, in the gap above the status line.
    const Rect back = rect(Screen::Lights, L_BACK);
    TEST_ASSERT_EQUAL_INT(L_BACK, hit(Screen::Lights, back.x + 5, back.y + back.h + 1));
    // Display only: no snapping to a neighbour from inside them.
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Lights, LEVEL.x + 50, LEVEL.y + 20));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Lights, STATUS.x + 50, STATUS.y + 5));
    // The dashboard's bands are display only, even the rows next to the items.
    for (const Rect& r : {HEADER, CLOCK_BAND, TRIP_BAND, LIGHTS_BAND, PROMPT_BAND}) {
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
    // The Menu: left / right along the three items, one step at a time; up, down and past the ends
    // (no wrap) nothing lies that way.
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + LIGHTS, 1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, 1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + KARAOKE, neighbour(Screen::Menu, M_ITEM0 + MUSIC, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, neighbour(Screen::Menu, M_ITEM0 + KARAOKE, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + LIGHTS, neighbour(Screen::Menu, M_ITEM0 + LIGHTS, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_ITEM0 + MUSIC, neighbour(Screen::Menu, M_ITEM0 + MUSIC, 1, 0));
    for (int i = 0; i < N_ITEMS; i++) {
        TEST_ASSERT_EQUAL_INT(M_ITEM0 + i, neighbour(Screen::Menu, M_ITEM0 + i, 0, 1));
        TEST_ASSERT_EQUAL_INT(M_ITEM0 + i, neighbour(Screen::Menu, M_ITEM0 + i, 0, -1));
    }
}

void test_karaoke_screen() {
    TEST_ASSERT_EQUAL_INT(K_N, count(Screen::Karaoke));
    TEST_ASSERT_EQUAL_INT(1, K_N);
    const Rect back = rect(Screen::Karaoke, K_BACK), lightsBack = rect(Screen::Lights, L_BACK);
    TEST_ASSERT_TRUE(back.x == lightsBack.x && back.y == lightsBack.y && back.w == lightsBack.w && back.h == lightsBack.h);
    TEST_ASSERT_EQUAL_INT(0, rect(Screen::Karaoke, 1).w);
    for (int y = 0; y < 240; y += 3)
        for (int x = 0; x < 320; x += 3) {
            const int h = hit(Screen::Karaoke, x, y);
            TEST_ASSERT_TRUE(h == -1 || h == K_BACK);
        }
    TEST_ASSERT_EQUAL_INT(K_BACK, hit(Screen::Karaoke, 20, 20));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Karaoke, 160, 120));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Karaoke, 200, 20));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Karaoke, 20, 50));  // the body, just under Back
    TEST_ASSERT_EQUAL_INT(K_BACK, neighbour(Screen::Karaoke, K_BACK, 1, 0));
    TEST_ASSERT_EQUAL_INT(K_BACK, neighbour(Screen::Karaoke, K_BACK, 0, 1));
    TEST_ASSERT_FALSE(isPreset(Screen::Karaoke, K_BACK));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Karaoke, 0));
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

// The pre-flight check (A/B) and trip complete (D) screens are one tap anywhere: it dismisses them.
void test_check_and_parked_are_one_full_screen_tap() {
    for (Screen s : {Screen::Check, Screen::Parked}) {
        TEST_ASSERT_EQUAL_INT(H_N, count(s));
        TEST_ASSERT_EQUAL_INT(1, H_N);
        const Rect r = rect(s, H_TAP);
        TEST_ASSERT_TRUE(r.x == 0 && r.y == 0 && r.w == 320 && r.h == 240);
        for (int id : {-1, 1, 2, 20}) {
            const Rect n = rect(s, id);
            TEST_ASSERT_TRUE(n.x == 0 && n.y == 0 && n.w == 0 && n.h == 0);
        }
        for (int y = 0; y < 240; y += 7)
            for (int x = 0; x < 320; x += 7) TEST_ASSERT_EQUAL_INT(H_TAP, hit(s, x, y));
        TEST_ASSERT_EQUAL_INT(H_TAP, hit(s, 319, 239));
        TEST_ASSERT_EQUAL_INT(H_TAP, neighbour(s, H_TAP, 1, 0));
        TEST_ASSERT_EQUAL_INT(H_TAP, neighbour(s, H_TAP, 0, -1));
        TEST_ASSERT_FALSE(isPreset(s, H_TAP));
        TEST_ASSERT_EQUAL_INT(-1, presetSlot(s, H_TAP));
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
    RUN_TEST(test_hit_snaps_and_ignores_display_rects);
    RUN_TEST(test_neighbour);
    RUN_TEST(test_karaoke_screen);
    RUN_TEST(test_preset_slots);
    RUN_TEST(test_check_and_parked_are_one_full_screen_tap);
    return UNITY_END();
}
