#include <unity.h>
#include "../../src/util/avatarscene.h"

using avatarscene::Phase;
using avatarscene::at;
void setUp() {}
void tearDown() {}

static const uint32_t S = avatarscene::SLIDE_MS, C = avatarscene::CHAR_MS;

void test_slides_in_in_six_steps() {
    TEST_ASSERT_TRUE(at(0, "Hi", 1).phase == Phase::Slide);
    TEST_ASSERT_EQUAL_INT(15, at(0, "Hi", 1).shown);           // 92 * 1/6
    TEST_ASSERT_EQUAL_INT(61, at(120, "Hi", 1).shown);         // step 4
    TEST_ASSERT_EQUAL_INT(92, at(S - 1, "Hi", 1).shown);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)at(S - 1, "Hi", 1).bytes);
}
void test_types_one_character_per_step() {
    avatarscene::State st = at(S, "Hi there", 1);
    TEST_ASSERT_TRUE(st.phase == Phase::Type);
    TEST_ASSERT_EQUAL_INT(92, st.shown);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)st.bytes);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)at(S + 2 * C, "Hi there", 1).bytes);
}
void test_accented_character_types_whole() {
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)at(S + C, "\xC3\xA9!", 1).bytes);    // "é" = 2 bytes
}
void test_still_mood_never_changes_frame() {
    for (uint32_t t = 0; t < 400; t += 10) TEST_ASSERT_EQUAL_INT(0, at(S + t, "Hello world", 1).frame);
}
void test_mouth_flaps_on_letters_rests_on_spaces() {
    // "Hello world", 2 frames. t = ms after the slide; t/100 even -> open on a letter.
    TEST_ASSERT_EQUAL_INT(1, at(S + 50, "Hello world", 2).frame);    // "He", t/100 = 0
    TEST_ASSERT_EQUAL_INT(0, at(S + 125, "Hello world", 2).frame);   // "Hello", t/100 = 1 (flap)
    TEST_ASSERT_EQUAL_INT(0, at(S + 150, "Hello world", 2).frame);   // "Hello ", a space
    TEST_ASSERT_EQUAL_INT(1, at(S + 225, "Hello world", 2).frame);   // "Hello wor", t/100 = 2
    TEST_ASSERT_EQUAL_INT(0, at(S, "Hello world", 2).frame);         // nothing typed yet
}
void test_more_frames_cycle_open_shapes() {
    const int a = at(S + 50, "Hello world", 3).frame, b = at(S + 225, "Hello world", 3).frame;
    TEST_ASSERT_TRUE(a >= 1 && a <= 2);
    TEST_ASSERT_TRUE(b >= 1 && b <= 2);
    TEST_ASSERT_TRUE(a != b);                                         // t/200 = 0 then 1
}
void test_holds_then_done() {
    avatarscene::State st = at(S + 2 * C, "Hi", 2);                   // both characters typed
    TEST_ASSERT_TRUE(st.phase == Phase::Hold);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)st.bytes);
    TEST_ASSERT_EQUAL_INT(0, st.frame);
    TEST_ASSERT_TRUE(at(S + 2 * C + avatarscene::HOLD_MS - 1, "Hi", 2).phase == Phase::Hold);
    TEST_ASSERT_TRUE(at(S + 2 * C + avatarscene::HOLD_MS, "Hi", 2).phase == Phase::Done);
}
void test_empty_line() {
    TEST_ASSERT_TRUE(at(S, nullptr, 2).phase == Phase::Hold);
    TEST_ASSERT_TRUE(at(S + avatarscene::HOLD_MS, "", 2).phase == Phase::Done);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slides_in_in_six_steps);
    RUN_TEST(test_types_one_character_per_step);
    RUN_TEST(test_accented_character_types_whole);
    RUN_TEST(test_still_mood_never_changes_frame);
    RUN_TEST(test_mouth_flaps_on_letters_rests_on_spaces);
    RUN_TEST(test_more_frames_cycle_open_shapes);
    RUN_TEST(test_holds_then_done);
    RUN_TEST(test_empty_line);
    return UNITY_END();
}
