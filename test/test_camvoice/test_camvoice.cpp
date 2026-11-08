#include <unity.h>
#include "../../src/util/yellow/camvoice.h"

void setUp() {}
void tearDown() {}

using camvoice::Cmd;

static void expect(Cmd want, const char* text) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(want), static_cast<int>(camvoice::parse(text)), text);
}

void test_mark() {
    expect(Cmd::Mark, "mark camera");
    expect(Cmd::Mark, "Mark camera.");
    expect(Cmd::Mark, "MARK THE CAMERA!");
    expect(Cmd::Mark, "marca radar");
    expect(Cmd::Mark, "Marcar o radar");
    expect(Cmd::Mark, "marca c\xc3\xa2mera");
}
void test_mute() {
    expect(Cmd::Mute, "cameras mute");
    expect(Cmd::Mute, "Mute cameras.");
    expect(Cmd::Mute, "mute the camera, please");
    expect(Cmd::Mute, "silenciar radar");
    expect(Cmd::Mute, "Silenciar radares!");
    expect(Cmd::Mute, "cameras off");
}
void test_unmute() {
    expect(Cmd::Unmute, "cameras on");
    expect(Cmd::Unmute, "Radar on.");
    expect(Cmd::Unmute, "unmute cameras");
    expect(Cmd::Unmute, "liga radar");
}
// The order of the two words doesn't matter: "camera mark" is as good as "mark camera".
void test_any_order() {
    expect(Cmd::Mark, "camera mark");
    expect(Cmd::Mute, "radar silenciar");
}
// Lights commands have no camera word, so they fall through to ledvoice untouched.
void test_lights_words_are_not_camera_words() {
    expect(Cmd::None, "purple inside");
    expect(Cmd::None, "lights off");
    expect(Cmd::None, "turn the lights on");
    expect(Cmd::None, "desliga as luzes");
}
void test_camera_alone_is_none() {
    expect(Cmd::None, "camera");
    expect(Cmd::None, "Radar.");
    expect(Cmd::None, "the cameras please");
}
void test_action_alone_is_none() {
    expect(Cmd::None, "mark");
    expect(Cmd::None, "mute");
}
void test_empty_and_null() {
    expect(Cmd::None, "");
    expect(Cmd::None, nullptr);
}
// A word only counts whole: "cameraman" or "radarscope" is no camera word, "marked" no mark word.
void test_whole_words_only() {
    expect(Cmd::None, "mark cameraman");
    expect(Cmd::None, "marked camera");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mark);
    RUN_TEST(test_mute);
    RUN_TEST(test_unmute);
    RUN_TEST(test_any_order);
    RUN_TEST(test_lights_words_are_not_camera_words);
    RUN_TEST(test_camera_alone_is_none);
    RUN_TEST(test_action_alone_is_none);
    RUN_TEST(test_empty_and_null);
    RUN_TEST(test_whole_words_only);
    return UNITY_END();
}
