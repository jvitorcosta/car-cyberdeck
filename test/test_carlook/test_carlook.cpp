#include <unity.h>
#include "../../src/black/ui/carlook.h"

void setUp() {}
void tearDown() {}

static int r8(uint16_t c) { return (c >> 11) << 3; }
static int g8(uint16_t c) { return ((c >> 5) & 63) << 2; }
static int b8(uint16_t c) { return (c & 31) << 3; }
static bool red(uint16_t c) { return r8(c) >= 140 && g8(c) <= 90 && b8(c) <= 90; }

void test_honda_keeps_the_silver_rims() {
    TEST_ASSERT_EQUAL_PTR(&carlook::SILVER, carlook::honda().rims);
    TEST_ASSERT_FALSE(red(carlook::SILVER.ring[2]));
}

// McQueen's rims are red: the rim lips and the spokes, so they read red while they spin.
void test_mcqueen_rims_are_red_on_black_tyres() {
    const carlook::Rims& r = carlook::RED;
    TEST_ASSERT_TRUE(red(r.ring[2]));
    TEST_ASSERT_TRUE(red(r.ring[4]));
    TEST_ASSERT_TRUE(red(r.spoke[1]));
    TEST_ASSERT_EQUAL_HEX16(carlook::SILVER.ring[1], r.ring[1]);   // the tyre stays black
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_honda_keeps_the_silver_rims);
    RUN_TEST(test_mcqueen_rims_are_red_on_black_tyres);
    return UNITY_END();
}
