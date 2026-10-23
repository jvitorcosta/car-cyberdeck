#include <unity.h>
#include <cstring>
#include "../../src/util/ledfavs.h"

void setUp() {}
void tearDown() {}

static ledfavs::Fav makeFav(int inSw, int inPct, bool inLit, int outSw, int outPct, bool outLit) {
    ledfavs::Fav f;
    f.set = true;
    f.in = {inSw, static_cast<uint8_t>(inPct), inLit};
    f.out = {outSw, static_cast<uint8_t>(outPct), outLit};
    return f;
}

void test_encode_decode_round_trip() {
    const ledfavs::Fav f = makeFav(2, 50, true, 2, 100, true);
    uint8_t b[ledfavs::BYTES];
    ledfavs::encode(f, b);
    ledfavs::Fav g;
    TEST_ASSERT_TRUE(ledfavs::decode(b, &g));
    TEST_ASSERT_TRUE(g.set);
    TEST_ASSERT_TRUE(f.in == g.in);
    TEST_ASSERT_TRUE(f.out == g.out);
}

void test_off_light_survives() {
    const ledfavs::Fav f = makeFav(2, 50, false, 2, 100, true);
    uint8_t b[ledfavs::BYTES];
    ledfavs::encode(f, b);
    ledfavs::Fav g;
    TEST_ASSERT_TRUE(ledfavs::decode(b, &g));
    TEST_ASSERT_FALSE(g.in.lit);
    char s[96];
    ledfavs::describe(g, 1, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Favourite 1: inside off, outside Pink 100 percent", s);
}

void test_bad_bytes_are_empty() {
    const uint8_t bad[][ledfavs::BYTES] = {
        {9, 50, 1, 0, 100, 1}, {0, 55, 1, 0, 100, 1}, {0, 50, 2, 0, 100, 1}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    for (const auto& b : bad) {
        ledfavs::Fav f = makeFav(1, 20, true, 1, 20, true);
        TEST_ASSERT_FALSE(ledfavs::decode(b, &f));
        TEST_ASSERT_FALSE(f.set);
    }
}

void test_describe_set_and_empty() {
    char s[96];
    ledfavs::describe(makeFav(2, 50, true, 2, 100, true), 1, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Favourite 1: inside Pink 50 percent, outside Pink 100 percent", s);
    ledfavs::describe(ledfavs::Fav{}, 3, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Favourite 3 is empty", s);
}

void test_wire_lines() {
    char s[64];
    ledfavs::wireLine(makeFav(2, 50, true, 2, 100, true), 1, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("1 pink 50 pink 100", s);
    ledfavs::wireLine(makeFav(2, 50, false, 4, 80, true), 2, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("2 off 0 blue 80", s);
    ledfavs::wireLine(ledfavs::Fav{}, 3, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("3 -", s);
}

void test_swatch_words() {
    const char* order[] = {"red", "orange", "pink", "purple", "blue", "green", "white", "yellow"};
    TEST_ASSERT_EQUAL_INT(8, ledlook::N_SWATCH);
    for (int i = 0; i < ledlook::N_SWATCH; i++) TEST_ASSERT_EQUAL_STRING(order[i], ledlook::SWATCHES[i].word);
    TEST_ASSERT_EQUAL_STRING("red", ledlook::SWATCHES[0].word);
    TEST_ASSERT_EQUAL_STRING("yellow", ledlook::SWATCHES[7].word);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_decode_round_trip);
    RUN_TEST(test_off_light_survives);
    RUN_TEST(test_bad_bytes_are_empty);
    RUN_TEST(test_describe_set_and_empty);
    RUN_TEST(test_wire_lines);
    RUN_TEST(test_swatch_words);
    return UNITY_END();
}
