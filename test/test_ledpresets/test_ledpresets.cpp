#include <unity.h>
#include <cstring>
#include "../../src/util/ledpresets.h"

void setUp() {}
void tearDown() {}

static void expectPreset(int i, const char* name, const char* word, int sw, int inPct, int outPct) {
    const ledpresets::Preset& p = ledpresets::PRESETS[i];
    TEST_ASSERT_EQUAL_STRING(name, p.name);
    TEST_ASSERT_EQUAL_STRING(word, p.word);
    TEST_ASSERT_TRUE(p.in == (ledlook::Look{sw, static_cast<uint8_t>(inPct), true}));
    TEST_ASSERT_TRUE(p.out == (ledlook::Look{sw, static_cast<uint8_t>(outPct), true}));
}

// Six, all 40 % inside and 100 % outside (the owner's picks, 2026-10-26: GOJIRA took CALM's
// place, FOREST became CIVIC).
void test_table() {
    TEST_ASSERT_EQUAL_INT(6, ledpresets::N);
    expectPreset(0, "ROSE", "rose", 2, 40, 100);
    expectPreset(1, "FIRE", "fire", 0, 40, 100);
    expectPreset(2, "GOJIRA", "gojira", ledlook::LIGHT_BLUE, 40, 100);
    expectPreset(3, "PURPURA", "purpura", 3, 40, 100);
    expectPreset(4, "CIVIC", "civic", 5, 40, 100);
    expectPreset(5, "OCEAN", "ocean", 4, 40, 100);
}
// What the deck's screen draws for each: the name, but ゴジラ (gojiraname's token) for GOJIRA.
void test_shown_names() {
    TEST_ASSERT_EQUAL_STRING("\x01", ledpresets::shown(ledpresets::PRESETS[2]));
    TEST_ASSERT_EQUAL_STRING("ROSE", ledpresets::shown(ledpresets::PRESETS[0]));
    TEST_ASSERT_EQUAL_STRING("CIVIC", ledpresets::shown(ledpresets::PRESETS[4]));
    for (int i = 0; i < ledpresets::N; i++) {
        if (i != 2) TEST_ASSERT_EQUAL_STRING(ledpresets::PRESETS[i].name, ledpresets::shown(ledpresets::PRESETS[i]));
    }
}
void test_describe() {
    char buf[112];
    ledpresets::describe(1, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Rose: inside Pink 40 percent, outside Pink 100 percent", buf);
    ledpresets::describe(4, buf, sizeof buf);
    TEST_ASSERT_EQUAL_INT(0, strncmp(buf, "Purpura: ", 9));
    ledpresets::describe(6, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Ocean: inside Blue 40 percent, outside Blue 100 percent", buf);
    ledpresets::describe(3, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Gojira: inside Light blue 40 percent, outside Light blue 100 percent", buf);
}
void test_describe_fits() {
    char buf[112];
    for (int s = 1; s <= ledpresets::N; s++) {
        ledpresets::describe(s, buf, sizeof buf);
        TEST_ASSERT_TRUE(strlen(buf) < sizeof buf - 1);
    }
}
void test_wire_line() {
    char buf[64];
    ledpresets::wireLine(3, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("3 GOJIRA light blue 40 light blue 100", buf);
    ledpresets::wireLine(5, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("5 CIVIC green 40 green 100", buf);
    ledpresets::wireLine(1, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("1 ROSE pink 40 pink 100", buf);
}

void test_wire_line_clamps_slot() {
    char buf[64];
    ledpresets::wireLine(9, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("6 OCEAN blue 40 blue 100", buf);
    ledpresets::wireLine(0, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("1 ROSE pink 40 pink 100", buf);
}

// The swatch words in this order are the /presets wire contract (the watch maps them back). The
// first eight are the Lights grid; light blue, the ninth, is only a preset's.
void test_swatch_words() {
    const char* order[] = {"red", "orange", "pink", "purple", "blue", "green", "white", "yellow", "light blue"};
    TEST_ASSERT_EQUAL_INT(9, ledlook::N_SWATCH);
    TEST_ASSERT_EQUAL_INT(8, ledlook::N_GRID);
    TEST_ASSERT_EQUAL_INT(8, ledlook::LIGHT_BLUE);
    for (int i = 0; i < ledlook::N_SWATCH; i++) TEST_ASSERT_EQUAL_STRING(order[i], ledlook::SWATCHES[i].word);
    const ledlook::Swatch& lb = ledlook::SWATCHES[ledlook::LIGHT_BLUE];
    TEST_ASSERT_EQUAL_STRING("Light blue", lb.name);
    TEST_ASSERT_EQUAL_HEX32(0x3CD2FF, lb.screen);
    TEST_ASSERT_TRUE(lb.r == 0 && lb.g == 130 && lb.b == 255);  // candidate B until the car test (CYAN_TEST)
}
// The default look is yellow (the grid's last), not the preset-only light blue.
void test_default_look() {
    TEST_ASSERT_EQUAL_INT(ledlook::YELLOW, ledlook::Look{}.swatch);
    TEST_ASSERT_EQUAL_STRING("yellow", ledlook::SWATCHES[ledlook::Look{}.swatch].word);
}
// The grid colour standing for a swatch (the Lights focus, the theme): itself, blue for light blue.
void test_grid_swatch() {
    for (int i = 0; i < ledlook::N_GRID; i++) TEST_ASSERT_EQUAL_INT(i, ledlook::gridSwatch(i));
    TEST_ASSERT_EQUAL_INT(ledlook::BLUE, ledlook::gridSwatch(ledlook::LIGHT_BLUE));
    TEST_ASSERT_EQUAL_STRING("blue", ledlook::SWATCHES[ledlook::BLUE].word);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_table);
    RUN_TEST(test_describe);
    RUN_TEST(test_describe_fits);
    RUN_TEST(test_wire_line);
    RUN_TEST(test_wire_line_clamps_slot);
    RUN_TEST(test_swatch_words);
    RUN_TEST(test_shown_names);
    RUN_TEST(test_default_look);
    RUN_TEST(test_grid_swatch);
    return UNITY_END();
}
