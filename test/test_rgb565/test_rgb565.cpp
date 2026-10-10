#include <unity.h>
#include "../../src/util/rgb565.h"

void setUp() {}
void tearDown() {}

void test_rgb_packs_the_top_bits_of_each_channel() {
    TEST_ASSERT_EQUAL_HEX16(0xF800, rgb565::rgb(0xFF, 0x00, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, rgb565::rgb(0x00, 0xFF, 0x00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, rgb565::rgb(0x00, 0x00, 0xFF));
    TEST_ASSERT_EQUAL_HEX16(0xFFDB, rgb565::rgb(0xF8, 0xF8, 0xD8));   // battle box cream
}
void test_hex_reads_0xRRGGBB() {
    TEST_ASSERT_EQUAL_HEX16(0xBE19, rgb565::hex(0xBCC3CB));
    TEST_ASSERT_EQUAL_HEX16(rgb565::rgb(0x8C, 0x46, 0xFF), rgb565::hex(0x8C46FF));
}
void test_be_swaps_bytes_for_push_image() {
    static_assert(rgb565::be(0x1234) == 0x3412, "constexpr");
    TEST_ASSERT_EQUAL_HEX16(0x3412, rgb565::be(0x1234));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rgb_packs_the_top_bits_of_each_channel);
    RUN_TEST(test_hex_reads_0xRRGGBB);
    RUN_TEST(test_be_swaps_bytes_for_push_image);
    return UNITY_END();
}
