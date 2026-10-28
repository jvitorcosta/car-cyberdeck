#include <unity.h>
#include <initializer_list>
#include "../../src/yellow/ui/presslook.h"
#include "../../src/yellow/ui/termfont.h"
#include "../../src/yellow/ui/termlayout.h"
#include "../../src/util/yellow/deckmenu.h"

using termfont::Face;
void setUp() {}
void tearDown() {}

static bool same(presslook::Box a, presslook::Box b) { return a.bar == b.bar && a.ring == b.ring; }

// A press differs from idle and from selected whatever the button showed before it: a tap acts while
// the finger is down, so the tile it lit is already selected when it is drawn pressed.
void test_pressed_differs_from_idle_and_selected() {
    const presslook::Box idle = presslook::box(false, false), selected = presslook::box(false, true);
    TEST_ASSERT_FALSE(idle.bar);
    TEST_ASSERT_TRUE(selected.bar);
    TEST_ASSERT_FALSE(same(idle, selected));
    for (bool sel : {false, true}) {
        const presslook::Box p = presslook::box(true, sel);
        TEST_ASSERT_FALSE(same(p, idle));
        TEST_ASSERT_FALSE(same(p, selected));
        TEST_ASSERT_TRUE(p.bar && p.ring);  // the bar (unlike idle) with a ring cut in it (unlike selected)
    }
}

// The ring sits inside every button with room for a label inside it; on the Menu items it stays clear
// of the label's caps and of the tag.
void test_ring_fits_every_button() {
    constexpr int edge = presslook::INSET + presslook::RING;
    TEST_ASSERT_TRUE(presslook::INSET >= 2 && presslook::RING >= 2);  // off the bar's 1 px margin, 2 px thick
    for (deckmenu::Screen s : {deckmenu::Screen::Menu, deckmenu::Screen::Lights, deckmenu::Screen::Presets,
                               deckmenu::Screen::Karaoke}) {
        for (int id = 0; id < deckmenu::count(s); id++) {
            const deckmenu::Rect r = deckmenu::rect(s, id);
            if (r.w <= 0) continue;
            TEST_ASSERT_TRUE(r.w - 2 * edge >= termfont::capHeight(Face::SMALL));
            TEST_ASSERT_TRUE(r.h - 2 * edge >= termfont::capHeight(Face::SMALL));
        }
    }
    TEST_ASSERT_TRUE(termlayout::ROW_TEXT_Y >= edge + 1);
    TEST_ASSERT_TRUE(termlayout::TAG_BOTTOM >= edge && termlayout::TAG_RIGHT >= edge);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pressed_differs_from_idle_and_selected);
    RUN_TEST(test_ring_fits_every_button);
    return UNITY_END();
}
