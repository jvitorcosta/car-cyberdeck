#include <unity.h>
#include <initializer_list>
#include "../../src/util/deckmenu.h"

using namespace deckmenu;

void setUp() {}
void tearDown() {}

static bool overlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void check_screen(Screen s, const Rect* shown, int nShown) {
    for (int i = 0; i < count(s); i++) {
        const Rect a = rect(s, i);
        if (a.w == 0) continue;
        TEST_ASSERT_TRUE_MESSAGE(a.x >= 0 && a.y >= 0 && a.x + a.w <= 320 && a.y + a.h <= 240, "inside screen");
        TEST_ASSERT_TRUE_MESSAGE(a.w >= 70 && a.h >= 36, "big enough");
        for (int k = 0; k < nShown; k++) TEST_ASSERT_FALSE_MESSAGE(overlap(a, shown[k]), "over a display rect");
        for (int j = i + 1; j < count(s); j++) {
            const Rect b = rect(s, j);
            if (b.w == 0) continue;
            TEST_ASSERT_FALSE_MESSAGE(overlap(a, b), "buttons overlap");
        }
    }
}

void test_rects_inside_screen_and_apart() {
    const Rect menu[] = {HEADER, CLOCK_BAND, LIGHTS_BAND};
    const Rect lights[] = {STATUS, LEVEL};
    const Rect karaoke[] = {KARAOKE_BODY};
    check_screen(Screen::Menu, menu, 3);
    check_screen(Screen::Lights, lights, 2);
    check_screen(Screen::Karaoke, karaoke, 1);
}

// The dashboard: header, clock + weather (with the trip row), lights, then the three items at the
// bottom, top to bottom.
void test_menu_bands_stack() {
    TEST_ASSERT_TRUE(HEADER.y == 0 && HEADER.y + HEADER.h == CLOCK_BAND.y);
    TEST_ASSERT_EQUAL_INT(CLOCK_BAND.y + CLOCK_BAND.h, LIGHTS_BAND.y);
    const Rect items = rect(Screen::Menu, M_ITEM0);
    TEST_ASSERT_TRUE(LIGHTS_BAND.y + LIGHTS_BAND.h <= items.y);
    TEST_ASSERT_EQUAL_INT(236, items.y + items.h);  // 4 px off the screen's bottom, as off its sides
    for (const Rect& r : {HEADER, CLOCK_BAND, LIGHTS_BAND}) TEST_ASSERT_TRUE(r.x == 0 && r.w == 320);
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
    // just above the row (under the lights band) and below it (the screen's last rows), still the item
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
    for (const Rect& r : {HEADER, CLOCK_BAND, LIGHTS_BAND}) {
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = 0; x < 320; x += 7) TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Menu, x, y));
    }
}

void test_neighbour() {
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 1, neighbour(Screen::Lights, L_TILE0, 1, 0));
    TEST_ASSERT_EQUAL_INT(L_PRE0, neighbour(Screen::Lights, L_TILE0 + 4, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_ZONE0, neighbour(Screen::Lights, L_ZONE0, 0, -1));
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

// Presets are the Lights page's only: the Menu has none.
void test_preset_slots() {
    TEST_ASSERT_EQUAL_INT(0, presetSlot(Screen::Lights, L_PRE0));
    TEST_ASSERT_EQUAL_INT(3, presetSlot(Screen::Lights, L_PRE0 + 3));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Lights, L_TILE0));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Lights, L_MINUS));
    TEST_ASSERT_FALSE(isPreset(Screen::Lights, L_PLUS));
    TEST_ASSERT_TRUE(isPreset(Screen::Lights, L_PRE0 + 3));
    for (int id = -1; id <= 6; id++) {
        TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Menu, id));
        TEST_ASSERT_FALSE(isPreset(Screen::Menu, id));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rects_inside_screen_and_apart);
    RUN_TEST(test_menu_bands_stack);
    RUN_TEST(test_menu_ids_and_hits);
    RUN_TEST(test_hit_snaps_and_ignores_display_rects);
    RUN_TEST(test_neighbour);
    RUN_TEST(test_karaoke_screen);
    RUN_TEST(test_preset_slots);
    return UNITY_END();
}
