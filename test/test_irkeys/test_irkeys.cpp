#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/irkeys.h"

using irkeys::Action;
using irkeys::Key;
using irkeys::Kind;

void setUp() {}
void tearDown() {}

static Key key(uint8_t command) {
    Key k = Key::N;
    TEST_ASSERT_TRUE_MESSAGE(irkeys::lookup(irkeys::ADDRESS, command, &k), "a table code");
    return k;
}

static void assertAction(Key k, Kind kind, int value) {
    const Action a = irkeys::action(k);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(kind), static_cast<int>(a.kind));
    TEST_ASSERT_EQUAL_INT(value, a.value);
}

void test_every_key_maps() {
    const Key order[] = {Key::BrightUp, Key::BrightDown, Key::On,     Key::Off,    Key::Red,    Key::Green,  Key::Blue,
                         Key::White,    Key::Orange,     Key::Yellow, Key::Cyan,   Key::Purple, Key::Jump3,  Key::Jump7,
                         Key::Fade3,    Key::Fade7,      Key::Music1, Key::Music2, Key::Music3, Key::Music4};
    for (uint8_t c = 0; c < 20; c++) TEST_ASSERT_EQUAL_INT(static_cast<int>(order[c]), static_cast<int>(key(c)));
    assertAction(Key::BrightUp, Kind::Level, 1);
    assertAction(Key::BrightDown, Kind::Level, -1);
    assertAction(Key::On, Kind::Power, 1);
    assertAction(Key::Off, Kind::Power, 0);
    assertAction(Key::Red, Kind::Colour, ledlook::RED);
    assertAction(Key::Green, Kind::Colour, ledlook::GREEN);
    assertAction(Key::Blue, Kind::Colour, ledlook::BLUE);
    assertAction(Key::White, Kind::Colour, ledlook::WHITE);
    assertAction(Key::Orange, Kind::Colour, ledlook::ORANGE);
    assertAction(Key::Yellow, Kind::Colour, ledlook::YELLOW);
    assertAction(Key::Cyan, Kind::Colour, ledlook::CYAN);
    assertAction(Key::Purple, Kind::Colour, ledlook::PURPLE);
    assertAction(Key::Jump3, Kind::Effect, 0);
    assertAction(Key::Fade7, Kind::Effect, 3);
    assertAction(Key::Music1, Kind::Playlist, 0);
    assertAction(Key::Music2, Kind::Playlist, 1);
    assertAction(Key::Music4, Kind::Playlist, 3);
}

void test_foreign_address_is_ignored() {
    Key k;
    TEST_ASSERT_FALSE(irkeys::lookup(irkeys::ADDRESS ^ 1, 0x02, &k));
    TEST_ASSERT_FALSE(irkeys::lookup(0x0000, 0x02, &k));
}

void test_unknown_command_is_ignored() {
    Key k;
    TEST_ASSERT_FALSE(irkeys::lookup(irkeys::ADDRESS, 0x14, &k));
    TEST_ASSERT_FALSE(irkeys::lookup(irkeys::ADDRESS, 0xFF, &k));
}

void test_brightness_repeats_four_a_second() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::BrightUp, false, 0));
    TEST_ASSERT_FALSE(r.accept(Key::BrightUp, true, 110));
    TEST_ASSERT_FALSE(r.accept(Key::BrightUp, true, 220));
    TEST_ASSERT_TRUE(r.accept(Key::BrightUp, true, 330));
    TEST_ASSERT_FALSE(r.accept(Key::BrightUp, true, 440));
    TEST_ASSERT_TRUE(r.accept(Key::BrightUp, true, 580));
}

void test_other_keys_act_once_while_held() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::Off, false, 0));
    for (uint32_t t = 110; t <= 1100; t += 110) TEST_ASSERT_FALSE(r.accept(Key::Off, true, t));
}

void test_a_resent_frame_is_a_repeat() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::On, false, 0));
    TEST_ASSERT_FALSE(r.accept(Key::On, false, 110));
    TEST_ASSERT_TRUE(r.accept(Key::On, false, 400));
}

void test_playlist_presses_coalesce() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::Music1, false, 0));
    TEST_ASSERT_FALSE(r.accept(Key::Music1, false, 150));
}

void test_another_key_acts_at_once() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::On, false, 0));
    TEST_ASSERT_TRUE(r.accept(Key::Red, false, 50));
}

void test_repeat_survives_millis_wrap() {
    irkeys::Repeat r;
    TEST_ASSERT_TRUE(r.accept(Key::BrightUp, false, 0xFFFFFF00u));
    TEST_ASSERT_TRUE(r.accept(Key::BrightUp, true, 0xFFFFFF00u + 260));
}

void test_effect_modes_are_distinct() {
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(irkeys::EFFECT_MODE[i] <= 212);
        for (int j = 0; j < i; j++) TEST_ASSERT_NOT_EQUAL(irkeys::EFFECT_MODE[j], irkeys::EFFECT_MODE[i]);
    }
    TEST_ASSERT_TRUE(irkeys::EFFECT_SPEED <= 100);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_key_maps);
    RUN_TEST(test_foreign_address_is_ignored);
    RUN_TEST(test_unknown_command_is_ignored);
    RUN_TEST(test_brightness_repeats_four_a_second);
    RUN_TEST(test_other_keys_act_once_while_held);
    RUN_TEST(test_a_resent_frame_is_a_repeat);
    RUN_TEST(test_playlist_presses_coalesce);
    RUN_TEST(test_another_key_acts_at_once);
    RUN_TEST(test_repeat_survives_millis_wrap);
    RUN_TEST(test_effect_modes_are_distinct);
    return UNITY_END();
}
