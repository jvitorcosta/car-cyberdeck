#include <unity.h>
#include "../../src/util/elkcmd.h"

void setUp() {}
void tearDown() {}

static void expectFrame(const uint8_t (&want)[9], const elkcmd::Frame& got) {
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, got.b, 9);
}

void test_color_frames_in_both_variants() {
    const uint8_t v0[9] = {0x7E, 0x00, 0x05, 0x03, 0xFF, 0x10, 0x80, 0x00, 0xEF};
    const uint8_t v1[9] = {0x7E, 0x07, 0x05, 0x03, 0xFF, 0x10, 0x80, 0x10, 0xEF};
    expectFrame(v0, elkcmd::color(elkcmd::V0, 0xFF, 0x10, 0x80));
    expectFrame(v1, elkcmd::color(elkcmd::V1, 0xFF, 0x10, 0x80));
}
void test_power_frames_match_what_worked_in_the_car() {
    const uint8_t on0[9] = {0x7E, 0x00, 0x04, 0xF0, 0x00, 0x01, 0xFF, 0x00, 0xEF};
    const uint8_t off1[9] = {0x7E, 0x04, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEF};
    expectFrame(on0, elkcmd::power(elkcmd::V0, true));
    expectFrame(off1, elkcmd::power(elkcmd::V1, false));
}
void test_dim_follows_a_perceptual_curve() {
    TEST_ASSERT_EQUAL_UINT8(255, elkcmd::dim(255, 100));
    TEST_ASSERT_EQUAL_UINT8(64, elkcmd::dim(255, 50));   // half the look, a quarter of the PWM
    TEST_ASSERT_EQUAL_UINT8(3, elkcmd::dim(255, 10));
    TEST_ASSERT_EQUAL_UINT8(0, elkcmd::dim(255, 0));
}
void test_dim_keeps_a_lit_channel_lit() {
    TEST_ASSERT_EQUAL_UINT8(1, elkcmd::dim(20, 1));   // 0.2 would round to 0 and shift the hue
    TEST_ASSERT_EQUAL_UINT8(0, elkcmd::dim(0, 50));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_color_frames_in_both_variants);
    RUN_TEST(test_power_frames_match_what_worked_in_the_car);
    RUN_TEST(test_dim_follows_a_perceptual_curve);
    RUN_TEST(test_dim_keeps_a_lit_channel_lit);
    return UNITY_END();
}
