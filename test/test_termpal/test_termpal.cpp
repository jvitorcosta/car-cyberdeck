#include <unity.h>
#include "../../src/util/termpal.h"

void setUp() {}
void tearDown() {}

static uint32_t luma(uint32_t c) { return (77 * ((c >> 16) & 0xFF) + 151 * ((c >> 8) & 0xFF) + 28 * (c & 0xFF)) >> 8; }

void test_termpal() {
    using namespace termpal;
    TEST_ASSERT_EQUAL_HEX32(0x96FFAA, rgb(TEXT, false));
    const uint32_t d = rgb(TEXT, false), n = rgb(TEXT, true);
    TEST_ASSERT_TRUE(n != d);
    TEST_ASSERT_EQUAL_HEX32(0xB37B00, n);
    // Night is dimmer (by luma), and amber / red family: blue never above red in any night role.
    TEST_ASSERT_TRUE(luma(n) < luma(d));
    TEST_ASSERT_TRUE(luma(rgb(MID, true)) < luma(n));
    TEST_ASSERT_TRUE(luma(rgb(DIM, true)) < luma(rgb(MID, true)));
    for (int r = BG; r <= ALERT; r++) {
        const uint32_t c = rgb(static_cast<Role>(r), true);
        TEST_ASSERT_TRUE((c & 0xFF) <= ((c >> 16) & 0xFF));
        if (r != BG) TEST_ASSERT_TRUE(luma(c) < luma(rgb(static_cast<Role>(r), false)));
    }
    // ALERT: red, deeper at night, a hue apart from TEXT in both modes (red dominates, TEXT is green).
    TEST_ASSERT_EQUAL_HEX32(0xFF3C28, rgb(ALERT, false));
    TEST_ASSERT_EQUAL_HEX32(0xB02A10, rgb(ALERT, true));
    TEST_ASSERT_TRUE(luma(rgb(ALERT, true)) < luma(rgb(ALERT, false)));
    for (int night = 0; night < 2; night++) {
        const uint32_t a = rgb(ALERT, night), t = rgb(TEXT, night);
        TEST_ASSERT_TRUE(a != t);
        TEST_ASSERT_TRUE(((a >> 16) & 0xFF) > ((a >> 8) & 0xFF));            // alert: red leads green
        TEST_ASSERT_TRUE(night ? true : ((t >> 8) & 0xFF) > ((t >> 16) & 0xFF));  // day text: green leads red
    }
    TEST_ASSERT_EQUAL_HEX32(0x6E4C00, rgb(MID, true));
    TEST_ASSERT_EQUAL_HEX32(0x372600, rgb(DIM, true));
    TEST_ASSERT_EQUAL_HEX32(0x061008, scan(false));
    TEST_ASSERT_EQUAL_HEX32(0x0A0700, scan(true));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, false));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, true));
    TEST_ASSERT_EQUAL_HEX32(0x50AA64, rgb(MID, false));
    TEST_ASSERT_EQUAL_HEX32(0x235530, rgb(DIM, false));
    TEST_ASSERT_EQUAL_HEX32(0xFF0090, tile(0xFF0090, false));
    TEST_ASSERT_EQUAL_HEX32(0xB20064, tile(0xFF0090, true));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_termpal);
    return UNITY_END();
}
