#include <unity.h>
#include <cstring>
#include "../../src/util/menunav.h"

using namespace menunav;
void setUp() {}
void tearDown() {}

static const Screen ALL[] = {Screen::Home, Screen::Lyrics, Screen::Sounds, Screen::Trip};

void test_buttons_fit_above_the_bar_and_do_not_overlap() {
    for (Screen s : ALL) {
        for (int v = 0; v < 2; ++v) {
            Btn b[MAX_BTNS];
            const int n = buttons(s, v, b);
            TEST_ASSERT_TRUE(n >= 0 && n <= MAX_BTNS);
            for (int i = 0; i < n; ++i) {
                TEST_ASSERT_TRUE(b[i].r.x >= 0 && b[i].r.y >= 0);
                TEST_ASSERT_TRUE(b[i].r.x + b[i].r.w <= 320);
                TEST_ASSERT_TRUE(b[i].r.y + b[i].r.h <= BAR.y);
                for (int j = 0; j < i; ++j) {
                    const bool apart = b[i].r.x >= b[j].r.x + b[j].r.w || b[j].r.x >= b[i].r.x + b[i].r.w ||
                                       b[i].r.y >= b[j].r.y + b[j].r.h || b[j].r.y >= b[i].r.y + b[i].r.h;
                    TEST_ASSERT_TRUE(apart);
                }
            }
        }
    }
}
// Home is just her (owner, 2026-10-14: "just keep the avatar there"); the phone opens screens.
void test_home_has_no_buttons() {
    Btn b[MAX_BTNS];
    TEST_ASSERT_EQUAL_INT(0, buttons(Screen::Home, false, b));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Home, false, 250, 40));   // where the menu was
}
void test_hit_edges() {
    Btn b[MAX_BTNS];
    buttons(Screen::Sounds, false, b);
    const Rect r = b[0].r;                                 // HOWL
    TEST_ASSERT_EQUAL_INT(Howl, hit(Screen::Sounds, false, r.x, r.y));
    TEST_ASSERT_EQUAL_INT(Howl, hit(Screen::Sounds, false, r.x + r.w - 1, r.y + r.h - 1));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Sounds, false, r.x + r.w, r.y));
}
// Review Focus 4
void test_vroom_only_when_offered() {
    Btn b[MAX_BTNS];
    TEST_ASSERT_EQUAL_INT(2, buttons(Screen::Sounds, false, b));     // HOWL, BACK
    TEST_ASSERT_EQUAL_INT(3, buttons(Screen::Sounds, true, b));      // HOWL, VROOM, BACK
    TEST_ASSERT_EQUAL_INT(Vroom, b[1].id);
    const Rect r = b[1].r;
    TEST_ASSERT_EQUAL_INT(Vroom, hit(Screen::Sounds, true, r.x + 5, r.y + 5));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Sounds, false, r.x + 5, r.y + 5));
}
void test_navigation() {
    TEST_ASSERT_TRUE(next(Screen::Home, Lyrics) == Screen::Lyrics);
    TEST_ASSERT_TRUE(next(Screen::Home, Sounds) == Screen::Sounds);
    TEST_ASSERT_TRUE(next(Screen::Home, Trip) == Screen::Trip);
    for (Screen s : ALL) TEST_ASSERT_TRUE(next(s, Back) == Screen::Home);
    TEST_ASSERT_TRUE(next(Screen::Sounds, Howl) == Screen::Sounds);   // actions stay put
    TEST_ASSERT_TRUE(next(Screen::Trip, Reset) == Screen::Trip);
}
void test_every_screen_but_home_has_back() {
    for (Screen s : ALL) {
        if (s == Screen::Home) continue;
        Btn b[MAX_BTNS];
        const int n = buttons(s, true, b);
        bool back = false;
        for (int i = 0; i < n; ++i) back = back || b[i].id == Back;
        TEST_ASSERT_TRUE(back);
    }
}
void test_only_reset_is_a_hold_button() {
    TEST_ASSERT_TRUE(isHold(Reset));
    TEST_ASSERT_FALSE(isHold(Howl));
    TEST_ASSERT_FALSE(isHold(Back));
    TEST_ASSERT_FALSE(isHold(None));
}
void test_idle_text() {
    TEST_ASSERT_EQUAL_STRING("What shall we do, darling?", idleText(Screen::Home, true, false));
    TEST_ASSERT_EQUAL_STRING("", idleText(Screen::Lyrics, true, false));
    TEST_ASSERT_EQUAL_STRING("Pick a sound, darling!", idleText(Screen::Sounds, true, false));
    TEST_ASSERT_EQUAL_STRING("Hold RESET to start a new trip.", idleText(Screen::Trip, true, false));
    TEST_ASSERT_EQUAL_STRING("Looking for the deck...", idleText(Screen::Home, false, false));
    TEST_ASSERT_EQUAL_STRING("Wrong key: REMOTE_TOKEN must match", idleText(Screen::Trip, false, true));
}

// Final review I2: the phone page always shows VROOM; when the deck doesn't offer it, pressing
// it must do nothing (it used to reach the deck, get 404 and say "I can't reach the deck").
void test_vroom_is_only_offered_when_the_deck_has_it() {
    TEST_ASSERT_FALSE(offered(Vroom, false));
    TEST_ASSERT_TRUE(offered(Vroom, true));
    TEST_ASSERT_TRUE(offered(Howl, false));
    TEST_ASSERT_TRUE(offered(Reset, false));
    TEST_ASSERT_TRUE(offered(Lyrics, false));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_buttons_fit_above_the_bar_and_do_not_overlap);
    RUN_TEST(test_home_has_no_buttons);
    RUN_TEST(test_hit_edges);
    RUN_TEST(test_vroom_only_when_offered);
    RUN_TEST(test_navigation);
    RUN_TEST(test_every_screen_but_home_has_back);
    RUN_TEST(test_only_reset_is_a_hold_button);
    RUN_TEST(test_idle_text);
    RUN_TEST(test_vroom_is_only_offered_when_the_deck_has_it);
    return UNITY_END();
}
