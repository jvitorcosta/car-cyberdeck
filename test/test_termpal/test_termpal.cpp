#include <unity.h>
#include "../../src/util/termpal.h"

void setUp() {}
void tearDown() {}

void test_termpal() {
    using namespace termpal;
    TEST_ASSERT_EQUAL_HEX32(0x3CFF7A, rgb(TEXT, false));
    const uint32_t d = rgb(TEXT, false), n = rgb(TEXT, true);
    for (int sh = 0; sh <= 16; sh += 8) TEST_ASSERT_TRUE(((n >> sh) & 0xFF) <= ((d >> sh) & 0xFF));
    TEST_ASSERT_TRUE(n != d);
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
