#include <unity.h>
#include "../../src/util/ledvoice.h"

void setUp() {}
void tearDown() {}

static ledvoice::Command parsed(const char* text) {
    ledvoice::Command c;
    TEST_ASSERT_TRUE_MESSAGE(ledvoice::parse(text, &c), text);
    return c;
}

void test_a_colour_alone() {
    TEST_ASSERT_EQUAL_INT(3, parsed("Purple.").color);
    TEST_ASSERT_EQUAL_INT(0, parsed("red").color);
    TEST_ASSERT_EQUAL_INT(6, parsed("WHITE").color);
}
void test_a_colour_with_a_zone_in_a_sentence() {
    const ledvoice::Command c = parsed("Set the lights to blue inside, please");
    TEST_ASSERT_EQUAL_INT(4, c.color);
    TEST_ASSERT_EQUAL_INT(ledvoice::ZONE_INSIDE, c.zone);
    TEST_ASSERT_EQUAL_INT(-1, c.power);
}
void test_power() {
    TEST_ASSERT_EQUAL_INT(0, parsed("lights off").power);
    TEST_ASSERT_EQUAL_INT(1, parsed("turn the lights on").power);
}
void test_levels_and_steps() {
    TEST_ASSERT_EQUAL_INT(50, parsed("50%").level);
    TEST_ASSERT_EQUAL_INT(70, parsed("green 68 percent").level);
    TEST_ASSERT_EQUAL_INT(10, parsed("3%").level);   // clamped to the panel's minimum
    TEST_ASSERT_EQUAL_INT(100, parsed("max").level);
    TEST_ASSERT_EQUAL_INT(1, parsed("brighter").levelStep);
    TEST_ASSERT_EQUAL_INT(-1, parsed("dimmer").levelStep);
}
void test_portuguese() {
    const ledvoice::Command c = parsed("Luzes roxo fora");
    TEST_ASSERT_EQUAL_INT(3, c.color);
    TEST_ASSERT_EQUAL_INT(ledvoice::ZONE_OUTSIDE, c.zone);
    TEST_ASSERT_EQUAL_INT(0, parsed("desliga as luzes").power);
    TEST_ASSERT_EQUAL_INT(-1, parsed("mais fraco").levelStep);   // the last step word wins
    TEST_ASSERT_EQUAL_INT(100, parsed("m\xc3\xa1ximo").level);
    TEST_ASSERT_EQUAL_INT(3, parsed("lil\xc3\xa1s").color);
}
void test_yellow() {
    TEST_ASSERT_EQUAL_INT(7, parsed("Yellow").color);
    TEST_ASSERT_EQUAL_INT(7, parsed("amarelo").color);
}
void test_siri_homophones() {
    TEST_ASSERT_EQUAL_INT(0, parsed("Read").color);   // a lone "red", as Siri wrote it
    TEST_ASSERT_EQUAL_INT(4, parsed("Blew").color);
}
void test_nothing_to_do() {
    ledvoice::Command c;
    TEST_ASSERT_FALSE(ledvoice::parse("hello there", &c));
    TEST_ASSERT_FALSE(ledvoice::parse("", &c));
}

void test_preset_names() {
    const char* in[] = {"Rose.", "FIRE!", "Gojira", "Purpura", "Civic", "ocean."};
    for (int i = 0; i < 6; i++) {
        const ledvoice::Command c = parsed(in[i]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(i + 1, c.preset, in[i]);
        TEST_ASSERT_EQUAL_INT(-1, c.level);
        TEST_ASSERT_EQUAL_INT(-1, c.color);
    }
    TEST_ASSERT_EQUAL_INT(3, parsed("Godzilla!").preset);
    TEST_ASSERT_EQUAL_INT(3, parsed("set the lights to godzilla").preset);
}
// CALM and FOREST are gone (2026-10-26): their words do nothing now.
void test_old_preset_names_are_gone() {
    ledvoice::Command c;
    TEST_ASSERT_FALSE(ledvoice::parse("calm", &c));
    TEST_ASSERT_FALSE(ledvoice::parse("Forest.", &c));
}
// Light blue (GOJIRA's colour, not on the grid): "light blue" (the two words next to each other),
// "azul claro" / "azul clara", "cyan" / "ciano". "light" or "claro" alone does nothing, "lights
// blue" and "light, then blue" are plain blue, and only blue turns light.
void test_light_blue() {
    TEST_ASSERT_EQUAL_INT(8, parsed("light blue").color);
    TEST_ASSERT_EQUAL_INT(8, parsed("Light Blue inside").color);
    TEST_ASSERT_EQUAL_INT(ledvoice::ZONE_INSIDE, parsed("Light Blue inside").zone);
    TEST_ASSERT_EQUAL_INT(8, parsed("light blew").color);
    TEST_ASSERT_EQUAL_INT(8, parsed("azul claro").color);
    TEST_ASSERT_EQUAL_INT(8, parsed("Azul-clara 50%").color);
    TEST_ASSERT_EQUAL_INT(50, parsed("Azul-clara 50%").level);
    TEST_ASSERT_EQUAL_INT(8, parsed("cyan").color);
    TEST_ASSERT_EQUAL_INT(8, parsed("ciano").color);
    TEST_ASSERT_EQUAL_INT(4, parsed("blue").color);
    TEST_ASSERT_EQUAL_INT(4, parsed("lights blue").color);
    TEST_ASSERT_EQUAL_INT(4, parsed("light, then blue").color);
    TEST_ASSERT_EQUAL_INT(0, parsed("light off").power);
    TEST_ASSERT_EQUAL_INT(-1, parsed("light off").color);
    TEST_ASSERT_EQUAL_INT(0, parsed("red claro").color);
    ledvoice::Command c;
    TEST_ASSERT_FALSE(ledvoice::parse("light", &c));
    TEST_ASSERT_FALSE(ledvoice::parse("claro", &c));
}
// The plain rule: the parser keeps what else it heard; applying the preset ignores it (Task 3).
void test_preset_name_with_other_words() {
    const ledvoice::Command c = parsed("rose 50");
    TEST_ASSERT_EQUAL_INT(1, c.preset);
    TEST_ASSERT_EQUAL_INT(50, c.level);
}
void test_purple_is_still_the_colour() {
    const ledvoice::Command c = parsed("purple");
    TEST_ASSERT_EQUAL_INT(3, c.color);
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
    const ledvoice::Command r = parsed("rosa");  // Portuguese pink stays a colour
    TEST_ASSERT_EQUAL_INT(2, r.color);
    TEST_ASSERT_EQUAL_INT(-1, r.preset);
}
void test_preset_number_words() {
    TEST_ASSERT_EQUAL_INT(2, parsed("preset two").preset);
    TEST_ASSERT_EQUAL_INT(2, parsed("favourite number 2").preset);
}
void test_name_disarms_a_favourite_word() {
    // "two" would be slot 2 if the name had not disarmed; the name's own number must win
    TEST_ASSERT_EQUAL_INT(1, parsed("favourite rose two").preset);
    TEST_ASSERT_EQUAL_INT(3, parsed("favourite gojira four").preset);
}

void test_favourite_digit() {
    const ledvoice::Command c = parsed("Favourite 2.");
    TEST_ASSERT_EQUAL_INT(2, c.preset);
    TEST_ASSERT_EQUAL_INT(-1, c.level);
}
void test_favourite_words_and_mishearings() {
    TEST_ASSERT_EQUAL_INT(1, parsed("favorite one").preset);
    TEST_ASSERT_EQUAL_INT(2, parsed("favourite to").preset);
    TEST_ASSERT_EQUAL_INT(3, parsed("preset three").preset);
    TEST_ASSERT_EQUAL_INT(4, parsed("favourite for").preset);
    TEST_ASSERT_EQUAL_INT(4, parsed("Favourite four please").preset);
    TEST_ASSERT_EQUAL_INT(5, parsed("preset five").preset);
    TEST_ASSERT_EQUAL_INT(6, parsed("Preset six.").preset);
}
void test_preset_five_and_six_digits() {
    TEST_ASSERT_EQUAL_INT(5, parsed("preset 5").preset);
    ledvoice::Command c = parsed("favourite 6");
    TEST_ASSERT_EQUAL_INT(6, c.preset);
    TEST_ASSERT_EQUAL_INT(-1, c.level);
}
void test_filler_to_is_not_a_favourite() {
    const ledvoice::Command c = parsed("set the lights to blue");
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
    TEST_ASSERT_EQUAL_INT(4, c.color);
}
void test_favourite_without_slot() {
    ledvoice::Command c;
    TEST_ASSERT_FALSE(ledvoice::parse("favourite", &c));
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
    TEST_ASSERT_FALSE(ledvoice::parse("favourite 7", &c));
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
}
void test_level_still_works() {
    const ledvoice::Command c = parsed("blue 50%");
    TEST_ASSERT_EQUAL_INT(50, c.level);
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
}
// Filler between "favourite" and its number ("number", "no.") must not cancel the slot: only a
// command word does.
void test_favourite_with_filler_before_the_number() {
    ledvoice::Command c = parsed("Favourite number 2.");
    TEST_ASSERT_EQUAL_INT(2, c.preset);
    TEST_ASSERT_EQUAL_INT(-1, c.level);
    c = parsed("favorite no. 2");
    TEST_ASSERT_EQUAL_INT(2, c.preset);
    TEST_ASSERT_EQUAL_INT(-1, c.level);
}
void test_a_command_word_cancels_the_slot() {
    ledvoice::Command c = parsed("favourite blue 50");
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
    TEST_ASSERT_EQUAL_INT(4, c.color);
    TEST_ASSERT_EQUAL_INT(50, c.level);
    c = parsed("blue favourite 7");
    TEST_ASSERT_EQUAL_INT(4, c.color);
    TEST_ASSERT_EQUAL_INT(-1, c.preset);
    TEST_ASSERT_EQUAL_INT(-1, c.level);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_colour_alone);
    RUN_TEST(test_a_colour_with_a_zone_in_a_sentence);
    RUN_TEST(test_power);
    RUN_TEST(test_levels_and_steps);
    RUN_TEST(test_portuguese);
    RUN_TEST(test_yellow);
    RUN_TEST(test_siri_homophones);
    RUN_TEST(test_nothing_to_do);
    RUN_TEST(test_preset_names);
    RUN_TEST(test_old_preset_names_are_gone);
    RUN_TEST(test_light_blue);
    RUN_TEST(test_preset_name_with_other_words);
    RUN_TEST(test_purple_is_still_the_colour);
    RUN_TEST(test_preset_number_words);
    RUN_TEST(test_name_disarms_a_favourite_word);
    RUN_TEST(test_favourite_digit);
    RUN_TEST(test_favourite_words_and_mishearings);
    RUN_TEST(test_preset_five_and_six_digits);
    RUN_TEST(test_filler_to_is_not_a_favourite);
    RUN_TEST(test_favourite_without_slot);
    RUN_TEST(test_level_still_works);
    RUN_TEST(test_favourite_with_filler_before_the_number);
    RUN_TEST(test_a_command_word_cancels_the_slot);
    return UNITY_END();
}
