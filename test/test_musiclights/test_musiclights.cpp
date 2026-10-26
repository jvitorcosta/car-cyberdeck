#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "../../src/util/ledlook.h"
#include "../../src/util/musiclights.h"

using songpalette::Rgb;
using namespace musiclights;

void setUp() {}
void tearDown() {}

// ledlook's order: red, orange, pink, purple, blue, green, white, yellow.
static int swatch(const char* word) {
    for (int i = 0; i < ledlook::N_SWATCH; i++) {
        if (strcmp(ledlook::SWATCHES[i].word, word) == 0) return i;
    }
    return -1;
}

void test_pure_colours_snap_to_their_swatch() {
    TEST_ASSERT_EQUAL_INT(0, nearestSwatch({255, 0, 0}));
    TEST_ASSERT_EQUAL_INT(4, nearestSwatch({0, 0, 255}));
    TEST_ASSERT_EQUAL_INT(swatch("green"), nearestSwatch({0, 255, 0}));
    TEST_ASSERT_EQUAL_INT(swatch("yellow"), nearestSwatch({255, 220, 0}));
}

void test_whites_and_washed_out_colours_are_white() {
    TEST_ASSERT_EQUAL_INT(6, nearestSwatch({255, 214, 160}));  // songpalette's warm white
    TEST_ASSERT_EQUAL_INT(6, nearestSwatch({255, 255, 255}));
    TEST_ASSERT_EQUAL_INT(6, nearestSwatch({200, 180, 170}));  // a pale grey-pink
    TEST_ASSERT_EQUAL_INT(6, nearestSwatch({0, 0, 0}));        // no colour at all
}

void test_cover_colours_snap_by_hue() {
    // songpalette's cover picks come at full saturation and value
    TEST_ASSERT_EQUAL_INT(swatch("orange"), nearestSwatch({255, 127, 0}));   // hue 30
    TEST_ASSERT_EQUAL_INT(swatch("orange"), nearestSwatch({255, 60, 0}));    // deep orange, hue 14
    TEST_ASSERT_EQUAL_INT(swatch("purple"), nearestSwatch({160, 32, 255}));  // songpalette's purple
    TEST_ASSERT_EQUAL_INT(swatch("purple"), nearestSwatch({140, 60, 255}));  // its violet
    TEST_ASSERT_EQUAL_INT(swatch("pink"), nearestSwatch({255, 64, 160}));    // its pink
    TEST_ASSERT_EQUAL_INT(swatch("pink"), nearestSwatch({255, 0, 200}));     // its magenta
    TEST_ASSERT_EQUAL_INT(swatch("blue"), nearestSwatch({0, 220, 255}));     // cyan: blue, not green
    TEST_ASSERT_EQUAL_INT(swatch("green"), nearestSwatch({0, 200, 170}));    // teal: green
    TEST_ASSERT_EQUAL_INT(swatch("red"), nearestSwatch({255, 0, 30}));       // a crimson
}

// What the black deck sends (songpalette::forLights): the colours as by day, whatever the time, so a
// song snaps to one swatch all day and night; only the cap follows the clock.
static songpalette::Rgb hue(int h) {  // 180..299 deg at full S and V, as songpalette's cover picks
    const int f = (h % 60) * 255 / 60;
    switch (h / 60) {
        case 3: return {0, static_cast<uint8_t>(255 - f), 255};
        default: return {static_cast<uint8_t>(f), 0, 255};  // 240..299
    }
}

void test_night_keeps_a_blue_cover_blue() {
    const songpalette::Rgb cover[2] = {hue(240), hue(200)};
    for (int minute : {720, 1200, 1380}) {  // day, evening, late
        // electro: blue's family; calm: teal's (the cover pulled 15 deg off it, to 225)
        TEST_ASSERT_EQUAL_INT(swatch("blue"), nearestSwatch(songpalette::forLights(cover, 2, 9, minute).c[0]));
        TEST_ASSERT_EQUAL_INT(swatch("blue"), nearestSwatch(songpalette::forLights(cover, 2, 21, minute).c[0]));
    }
    // a 255 deg cover under calm is 240 by day; the evening shift (+15) made it 255, purple
    const songpalette::Rgb indigo[2] = {hue(255), hue(200)};
    TEST_ASSERT_EQUAL_INT(swatch("purple"), nearestSwatch(songpalette::build(indigo, 2, 21, 1200).c[0]));
    TEST_ASSERT_EQUAL_INT(swatch("blue"), nearestSwatch(songpalette::forLights(indigo, 2, 21, 1200).c[0]));
    TEST_ASSERT_EQUAL_INT(swatch("blue"), nearestSwatch(songpalette::forLights(indigo, 2, 21, 1380).c[0]));
}

void test_night_keeps_white_white() {
    // calm's family (no cover) has its warm white second; by night build() turns it amber (yellow)
    TEST_ASSERT_EQUAL_INT(swatch("yellow"), nearestSwatch(songpalette::build(nullptr, 0, 21, 1380).c[1]));
    for (int minute : {720, 1200, 1380}) {
        TEST_ASSERT_EQUAL_INT(swatch("white"), nearestSwatch(songpalette::forLights(nullptr, 0, 21, minute).c[1]));
    }
}

void test_level_is_the_owners_under_the_cap_with_a_floor() {
    TEST_ASSERT_EQUAL_INT(45, level(100, 45));
    TEST_ASSERT_EQUAL_INT(40, level(100, 30));  // the floor
    TEST_ASSERT_EQUAL_INT(40, level(40, 45));
    TEST_ASSERT_EQUAL_INT(30, level(30, 70));   // already under the floor: kept
    TEST_ASSERT_EQUAL_INT(56, level(80, 70));
    TEST_ASSERT_EQUAL_INT(100, level(100, 100));
    TEST_ASSERT_EQUAL_INT(60, level(60, 100));
    TEST_ASSERT_EQUAL_INT(10, level(10, 45));
    TEST_ASSERT_EQUAL_INT(70, level(70, 255));  // a cap past 100 never raises the level
    TEST_ASSERT_EQUAL_INT(40, level(90, 0));
}

void test_blend_goes_from_one_colour_to_the_other() {
    const Rgb a{0, 0, 0}, b{255, 100, 10};
    Rgb c = blend(a, b, 0, 3);
    TEST_ASSERT_EQUAL_UINT8(0, c.r);
    c = blend(a, b, 3, 3);
    TEST_ASSERT_EQUAL_UINT8(255, c.r);
    TEST_ASSERT_EQUAL_UINT8(100, c.g);
    TEST_ASSERT_EQUAL_UINT8(10, c.b);
    c = blend(a, b, 1, 3);
    TEST_ASSERT_EQUAL_UINT8(85, c.r);
    TEST_ASSERT_EQUAL_UINT8(33, c.g);
    TEST_ASSERT_EQUAL_UINT8(3, c.b);
    c = blend(b, a, 2, 6);  // downwards too
    TEST_ASSERT_EQUAL_UINT8(170, c.r);
    TEST_ASSERT_EQUAL_UINT8(67, c.g);
    TEST_ASSERT_EQUAL_UINT8(7, c.b);
    c = blend(a, b, 9, 3);  // past the end: the target
    TEST_ASSERT_EQUAL_UINT8(255, c.r);
    c = blend(a, b, 1, 0);  // no steps: the target at once
    TEST_ASSERT_EQUAL_UINT8(255, c.r);
}

void test_state_waits_shows_the_colour_then_loses_the_signal() {
    TEST_ASSERT_EQUAL(State::Waiting, state(false, 1000, 1000));
    TEST_ASSERT_EQUAL(State::Waiting, state(false, 1000, 1000 + SIGNAL_MS - 1));
    TEST_ASSERT_EQUAL(State::NoSignal, state(false, 1000, 1000 + SIGNAL_MS));
    TEST_ASSERT_EQUAL(State::Colour, state(true, 5000, 5000));
    TEST_ASSERT_EQUAL(State::Colour, state(true, 5000, 5000 + SIGNAL_MS - 1));
    TEST_ASSERT_EQUAL(State::NoSignal, state(true, 5000, 5000 + SIGNAL_MS));
    TEST_ASSERT_EQUAL(State::Colour, state(true, 0xFFFFFF00u, 0x10u));  // across millis()' wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pure_colours_snap_to_their_swatch);
    RUN_TEST(test_whites_and_washed_out_colours_are_white);
    RUN_TEST(test_cover_colours_snap_by_hue);
    RUN_TEST(test_night_keeps_a_blue_cover_blue);
    RUN_TEST(test_night_keeps_white_white);
    RUN_TEST(test_level_is_the_owners_under_the_cap_with_a_floor);
    RUN_TEST(test_blend_goes_from_one_colour_to_the_other);
    RUN_TEST(test_state_waits_shows_the_colour_then_loses_the_signal);
    return UNITY_END();
}
