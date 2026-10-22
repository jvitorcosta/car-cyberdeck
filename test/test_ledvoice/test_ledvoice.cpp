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
void test_siri_homophones() {
    TEST_ASSERT_EQUAL_INT(0, parsed("Read").color);   // a lone "red", as Siri wrote it
    TEST_ASSERT_EQUAL_INT(4, parsed("Blew").color);
}
void test_nothing_to_do() {
    ledvoice::Command c;
    TEST_ASSERT_FALSE(ledvoice::parse("hello there", &c));
    TEST_ASSERT_FALSE(ledvoice::parse("", &c));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_colour_alone);
    RUN_TEST(test_a_colour_with_a_zone_in_a_sentence);
    RUN_TEST(test_power);
    RUN_TEST(test_levels_and_steps);
    RUN_TEST(test_portuguese);
    RUN_TEST(test_siri_homophones);
    RUN_TEST(test_nothing_to_do);
    return UNITY_END();
}
