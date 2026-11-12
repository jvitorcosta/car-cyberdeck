#include <cstring>
#include <unity.h>
#include "../../src/util/yellow/bothlook.h"

using ledlook::Look;

void setUp() {}
void tearDown() {}

static Look look(int swatch, int pct, bool lit) {
    Look l;
    l.swatch = swatch;
    l.pct = static_cast<uint8_t>(pct);
    l.lit = lit;
    return l;
}
static void assertLook(const Look& want, const Look& got) {
    TEST_ASSERT_EQUAL_INT(want.swatch, got.swatch);
    TEST_ASSERT_EQUAL_INT(want.pct, got.pct);
    TEST_ASSERT_EQUAL(want.lit, got.lit);
}
static bothlook::Change power(int on) { bothlook::Change c; c.power = on; return c; }
static bothlook::Change step(int s) { bothlook::Change c; c.step = s; return c; }
static bothlook::Change colour(int swatch) { bothlook::Change c; c.swatch = swatch; return c; }
static bothlook::Change level(int pct) { bothlook::Change c; c.level = pct; return c; }

// The spec's example favourite: pink, inside 50 %, outside 100 %.
static constexpr int PINK = 2, BLUE = 4;

void test_mixed_is_computed_from_the_two_lights() {
    TEST_ASSERT_TRUE(bothlook::mixed(look(PINK, 50, true), look(PINK, 100, true)));
    TEST_ASSERT_TRUE(bothlook::mixed(look(PINK, 50, true), look(BLUE, 50, true)));
    TEST_ASSERT_FALSE(bothlook::mixed(look(PINK, 50, true), look(PINK, 50, true)));
}
void test_seed_is_the_inside_look_lit_if_either_is() {
    assertLook(look(PINK, 50, true), bothlook::seed(look(PINK, 50, false), look(BLUE, 100, true)));
    assertLook(look(PINK, 50, false), bothlook::seed(look(PINK, 50, false), look(BLUE, 100, false)));
}
void test_mixed_power_off_then_on_keeps_each_colour_and_level() {
    Look in = look(PINK, 50, true), out = look(BLUE, 100, true);
    Look both = bothlook::seed(in, out);
    bothlook::apply(power(0), &both, &in, &out);
    assertLook(look(PINK, 50, false), in);
    assertLook(look(BLUE, 100, false), out);
    TEST_ASSERT_FALSE(both.lit);
    bothlook::apply(power(1), &both, &in, &out);
    assertLook(look(PINK, 50, true), in);
    assertLook(look(BLUE, 100, true), out);
    assertLook(look(PINK, 50, true), both);  // the page stays on the inside colour, never a third one
}
void test_mixed_level_step_steps_each_light_with_clamping() {
    Look in = look(PINK, 10, true), out = look(PINK, 100, true);
    Look both = bothlook::seed(in, out);
    bothlook::apply(step(-1), &both, &in, &out);
    assertLook(look(PINK, 10, true), in);   // already at the minimum
    assertLook(look(PINK, 90, true), out);
    bothlook::apply(step(+1), &both, &in, &out);
    bothlook::apply(step(+1), &both, &in, &out);
    assertLook(look(PINK, 30, true), in);
    assertLook(look(PINK, 100, true), out);  // capped at 100
}
void test_mixed_step_that_meets_makes_the_page_that_look() {
    Look in = look(PINK, 90, true), out = look(PINK, 100, true);
    Look both = look(BLUE, 40, true);  // stale
    bothlook::apply(step(+1), &both, &in, &out);
    assertLook(look(PINK, 100, true), in);
    assertLook(look(PINK, 100, true), out);
    assertLook(look(PINK, 100, true), both);
}
void test_mixed_colour_makes_both_lights_equal() {
    Look in = look(PINK, 50, true), out = look(BLUE, 100, false);
    Look both = bothlook::seed(in, out);
    bothlook::apply(colour(BLUE), &both, &in, &out);
    assertLook(look(BLUE, 50, true), both);  // the page's level, as a tile tap always did
    assertLook(both, in);
    assertLook(both, out);
    TEST_ASSERT_FALSE(bothlook::mixed(in, out));
}
void test_mixed_absolute_level_sets_each_pct_and_keeps_each_colour() {
    Look in = look(PINK, 50, true), out = look(BLUE, 100, true);
    Look both = bothlook::seed(in, out);
    bothlook::apply(level(70), &both, &in, &out);
    assertLook(look(PINK, 70, true), in);
    assertLook(look(BLUE, 70, true), out);
    assertLook(look(PINK, 70, true), both);
}
void test_not_mixed_copies_the_page_like_before() {
    Look in = look(PINK, 50, true), out = in;
    Look both = look(BLUE, 30, false);  // a stale BOTH page still wins, as it always did
    bothlook::apply(power(1), &both, &in, &out);
    assertLook(look(BLUE, 30, true), both);
    assertLook(both, in);
    assertLook(both, out);
    bothlook::apply(step(-1), &both, &in, &out);
    assertLook(look(BLUE, 20, true), in);
    bothlook::apply(colour(PINK), &both, &in, &out);
    assertLook(look(PINK, 20, true), out);
    bothlook::apply(level(3), &both, &in, &out);  // clamped like the old code
    assertLook(look(PINK, 10, true), in);
}
void test_edit_one_look() {
    Look l = look(PINK, 100, false);
    bothlook::Change c;
    c.swatch = BLUE;
    c.step = 1;
    bothlook::edit(&l, c);
    assertLook(look(BLUE, 100, true), l);
    c = {};
    c.swatch = PINK;
    c.power = 0;   // power after colour: "pink off" leaves it off
    c.level = 40;
    bothlook::edit(&l, c);
    assertLook(look(PINK, 40, false), l);
}

void test_recolour_keeps_each_level() {
    ledlook::Look in{ledlook::PINK, 60, true}, out{ledlook::PINK, 40, true}, both = bothlook::seed(in, out);
    bothlook::recolour(ledlook::BLUE, &both, &in, &out);
    TEST_ASSERT_EQUAL(ledlook::BLUE, in.swatch);
    TEST_ASSERT_EQUAL(60, in.pct);
    TEST_ASSERT_TRUE(in.lit);
    TEST_ASSERT_EQUAL(ledlook::BLUE, out.swatch);
    TEST_ASSERT_EQUAL(40, out.pct);
    TEST_ASSERT_TRUE(out.lit);
    TEST_ASSERT_EQUAL(ledlook::BLUE, both.swatch);
}

void test_recolour_keeps_an_off_light_off() {
    ledlook::Look in{ledlook::PINK, 60, true}, out{ledlook::PINK, 80, false}, both = bothlook::seed(in, out);
    bothlook::recolour(ledlook::BLUE, &both, &in, &out);
    TEST_ASSERT_EQUAL(ledlook::BLUE, out.swatch);
    TEST_ASSERT_FALSE(out.lit);
    TEST_ASSERT_EQUAL(80, out.pct);
    TEST_ASSERT_TRUE(in.lit);
}

static const char* level(ledlook::Look in, ledlook::Look out) {
    static char buf[24];
    bothlook::levelText(in, out, buf, sizeof(buf));
    return buf;
}

void test_level_text_matching() {
    TEST_ASSERT_EQUAL_STRING("60%", level({ledlook::PINK, 60, true}, {ledlook::PINK, 60, true}));
    TEST_ASSERT_EQUAL_STRING("OFF", level({ledlook::PINK, 60, false}, {ledlook::PINK, 60, false}));
}

void test_level_text_mixed() {
    TEST_ASSERT_EQUAL_STRING("IN 60%  OUT 40%", level({ledlook::PINK, 60, true}, {ledlook::PINK, 40, true}));
    TEST_ASSERT_EQUAL_STRING("60%", level({ledlook::PINK, 60, true}, {ledlook::BLUE, 60, true}));  // only the colours differ
}

void test_level_text_one_off() {
    TEST_ASSERT_EQUAL_STRING("IN OFF  OUT 80%", level({ledlook::PINK, 60, false}, {ledlook::PINK, 80, true}));
}

void test_level_text_both_off_is_off() {
    TEST_ASSERT_EQUAL_STRING("OFF", level({ledlook::PINK, 60, false}, {ledlook::BLUE, 80, false}));
}

void test_level_text_fits() {
    TEST_ASSERT_EQUAL_STRING("IN 100% OUT 90%", level({ledlook::PINK, 100, true}, {ledlook::BLUE, 90, true}));
    TEST_ASSERT_EQUAL_STRING("IN OFF OUT 100%", level({ledlook::PINK, 100, false}, {ledlook::BLUE, 100, true}));
    char small[8];
    memset(small, 'x', sizeof(small));
    bothlook::levelText({ledlook::PINK, 100, true}, {ledlook::BLUE, 40, true}, small, sizeof(small));
    TEST_ASSERT_EQUAL_STRING("IN 100%", small);
}

void test_paint_turns_lights_on_at_their_levels() {
    ledlook::Look in{ledlook::PINK, 60, false}, out{ledlook::PINK, 40, false}, both = bothlook::seed(in, out);
    bothlook::paint(ledlook::RED, &both, &in, &out);
    TEST_ASSERT_EQUAL(ledlook::RED, in.swatch);
    TEST_ASSERT_TRUE(in.lit);
    TEST_ASSERT_EQUAL(60, in.pct);
    TEST_ASSERT_EQUAL(ledlook::RED, out.swatch);
    TEST_ASSERT_TRUE(out.lit);
    TEST_ASSERT_EQUAL(40, out.pct);
    TEST_ASSERT_EQUAL(ledlook::RED, both.swatch);
    TEST_ASSERT_TRUE(both.lit);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mixed_is_computed_from_the_two_lights);
    RUN_TEST(test_seed_is_the_inside_look_lit_if_either_is);
    RUN_TEST(test_mixed_power_off_then_on_keeps_each_colour_and_level);
    RUN_TEST(test_mixed_level_step_steps_each_light_with_clamping);
    RUN_TEST(test_mixed_step_that_meets_makes_the_page_that_look);
    RUN_TEST(test_mixed_colour_makes_both_lights_equal);
    RUN_TEST(test_mixed_absolute_level_sets_each_pct_and_keeps_each_colour);
    RUN_TEST(test_not_mixed_copies_the_page_like_before);
    RUN_TEST(test_edit_one_look);
    RUN_TEST(test_recolour_keeps_each_level);
    RUN_TEST(test_recolour_keeps_an_off_light_off);
    RUN_TEST(test_level_text_matching);
    RUN_TEST(test_level_text_mixed);
    RUN_TEST(test_level_text_one_off);
    RUN_TEST(test_level_text_both_off_is_off);
    RUN_TEST(test_level_text_fits);
    RUN_TEST(test_paint_turns_lights_on_at_their_levels);
    return UNITY_END();
}
