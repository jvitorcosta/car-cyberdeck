#include <unity.h>
#include "../../src/util/termpal.h"

void setUp() {}
void tearDown() {}

void test_termpal() {
    using namespace termpal;
    TEST_ASSERT_EQUAL_HEX32(0x3CFF7A, rgb(TEXT, false));
    const uint32_t d = rgb(TEXT, false), n = rgb(TEXT, true);
    TEST_ASSERT_TRUE(n != d);
    TEST_ASSERT_EQUAL_HEX32(0xB37B00, n);
    // Amber: no blue in any night role, and none brighter than day's brightest channel (the
    // brief's "day's red channel" cannot hold: day red is 0x3C, below amber's 0xB3).
    const uint32_t dayMax = (d >> 8) & 0xFF;
    for (int r = BG; r <= DIM; r++) {
        const uint32_t c = rgb(static_cast<Role>(r), true);
        TEST_ASSERT_EQUAL_HEX32(0, c & 0xFF);
        TEST_ASSERT_TRUE(((c >> 16) & 0xFF) <= dayMax);
        TEST_ASSERT_TRUE(((c >> 8) & 0xFF) <= dayMax);
    }
    TEST_ASSERT_EQUAL_HEX32(0x6E4C00, rgb(MID, true));
    TEST_ASSERT_EQUAL_HEX32(0x372600, rgb(DIM, true));
    TEST_ASSERT_EQUAL_HEX32(0x031008, scan(false));
    TEST_ASSERT_EQUAL_HEX32(0x0A0700, scan(true));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, false));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, true));
    TEST_ASSERT_EQUAL_HEX32(0x1F9E48, rgb(MID, false));
    TEST_ASSERT_EQUAL_HEX32(0x0F5024, rgb(DIM, false));
    TEST_ASSERT_EQUAL_HEX32(0xFF0090, tile(0xFF0090, false));
    TEST_ASSERT_EQUAL_HEX32(0xB20064, tile(0xFF0090, true));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_termpal);
    return UNITY_END();
}
