#include <unity.h>
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
    const Rect menu[] = {HEADER, MENU_STRIP};
    const Rect lights[] = {STATUS, LEVEL};
    check_screen(Screen::Menu, menu, 2);
    check_screen(Screen::Lights, lights, 2);
}

void test_menu_ids_and_hits() {
    TEST_ASSERT_EQUAL_INT(M_N, count(Screen::Menu));
    const Rect lights = rect(Screen::Menu, M_ITEM0), first = rect(Screen::Menu, M_ITEM0 + 1);
    TEST_ASSERT_EQUAL_INT(312, lights.w);
    TEST_ASSERT_EQUAL_INT(M_PRE0, M_ITEM0 + 1);
    TEST_ASSERT_EQUAL_INT(4, first.x);
    TEST_ASSERT_EQUAL_INT(92, first.y);
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
    // The gap between the preset columns is x 158..161: each x snaps to the nearer block.
    TEST_ASSERT_EQUAL_INT(M_PRE0, hit(Screen::Menu, 158, 100));
    TEST_ASSERT_EQUAL_INT(M_PRE0, hit(Screen::Menu, 159, 100));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 1, hit(Screen::Menu, 160, 100));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 1, hit(Screen::Menu, 161, 100));
    // Display only: no snapping to a neighbour from inside them.
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Lights, LEVEL.x + 50, LEVEL.y + 20));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Lights, STATUS.x + 50, STATUS.y + 5));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Menu, 100, 10));
    TEST_ASSERT_EQUAL_INT(-1, hit(Screen::Menu, 100, 200));
}

void test_neighbour() {
    TEST_ASSERT_EQUAL_INT(L_TILE0 + 1, neighbour(Screen::Lights, L_TILE0, 1, 0));
    TEST_ASSERT_EQUAL_INT(L_PRE0, neighbour(Screen::Lights, L_TILE0 + 4, 0, 1));
    TEST_ASSERT_EQUAL_INT(L_ZONE0, neighbour(Screen::Lights, L_ZONE0, 0, -1));
    TEST_ASSERT_EQUAL_INT(L_TILE0, neighbour(Screen::Lights, L_TILE0 + 1, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_PRE0, neighbour(Screen::Menu, M_ITEM0, 0, 1));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 1, neighbour(Screen::Menu, M_PRE0, 1, 0));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 2, neighbour(Screen::Menu, M_PRE0, 0, 1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0, neighbour(Screen::Menu, M_PRE0, 0, -1));
    TEST_ASSERT_EQUAL_INT(M_ITEM0, neighbour(Screen::Menu, M_PRE0 + 1, 0, -1));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 3, neighbour(Screen::Menu, M_PRE0 + 1, 0, 1));
    TEST_ASSERT_EQUAL_INT(M_PRE0, neighbour(Screen::Menu, M_PRE0 + 1, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 2, neighbour(Screen::Menu, M_PRE0 + 2, 0, 1));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 3, neighbour(Screen::Menu, M_PRE0 + 3, 0, 1));
    // Sideways from the full-width LIGHTS row there is no block beside it, but neighbour() is
    // centre-based and generic, so it hops down to the nearest preset on that side.
    TEST_ASSERT_EQUAL_INT(M_PRE0, neighbour(Screen::Menu, M_ITEM0, -1, 0));
    TEST_ASSERT_EQUAL_INT(M_PRE0 + 1, neighbour(Screen::Menu, M_ITEM0, 1, 0));
}

void test_preset_slots() {
    TEST_ASSERT_EQUAL_INT(3, presetSlot(Screen::Lights, L_PRE0 + 3));
    TEST_ASSERT_EQUAL_INT(0, presetSlot(Screen::Menu, M_PRE0));
    TEST_ASSERT_EQUAL_INT(3, presetSlot(Screen::Menu, M_PRE0 + 3));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Lights, L_TILE0));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Lights, L_MINUS));
    TEST_ASSERT_EQUAL_INT(-1, presetSlot(Screen::Menu, M_ITEM0));
    TEST_ASSERT_TRUE(isPreset(Screen::Menu, M_PRE0 + 3));
    TEST_ASSERT_FALSE(isPreset(Screen::Lights, L_PLUS));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rects_inside_screen_and_apart);
    RUN_TEST(test_menu_ids_and_hits);
    RUN_TEST(test_hit_snaps_and_ignores_display_rects);
    RUN_TEST(test_neighbour);
    RUN_TEST(test_preset_slots);
    return UNITY_END();
}
