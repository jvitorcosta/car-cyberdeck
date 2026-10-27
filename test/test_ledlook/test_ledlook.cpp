#include <unity.h>
#include <cstring>
#include "../../src/util/ledlook.h"

void setUp() {}
void tearDown() {}

// The swatch names follow SWATCHES' order, so a table written with them can't drift from the grid.
void test_colour_names_follow_the_swatch_table() {
    const char* order[] = {"red", "orange", "pink", "purple", "blue", "green", "white", "yellow", "light blue"};
    const int ids[] = {ledlook::RED,   ledlook::ORANGE, ledlook::PINK,   ledlook::PURPLE,    ledlook::BLUE,
                       ledlook::GREEN, ledlook::WHITE,  ledlook::YELLOW, ledlook::LIGHT_BLUE};
    TEST_ASSERT_EQUAL_INT(9, ledlook::N_SWATCH);
    TEST_ASSERT_EQUAL_INT(8, ledlook::N_GRID);
    for (int i = 0; i < ledlook::N_SWATCH; i++) TEST_ASSERT_EQUAL_STRING(order[i], ledlook::SWATCHES[ids[i]].word);
}

static int wordsFor(int sw) {
    int n = 0;
    for (const ledlook::ColourWord& w : ledlook::COLOUR_WORDS) n += w.swatch == sw;
    return n;
}

// Every grid colour has its wire word plus at least one other (Portuguese) word; light blue has
// none (never a song colour; voice adds its own).
void test_every_grid_colour_has_words() {
    for (int sw = 0; sw < ledlook::N_GRID; sw++) {
        bool wire = false;
        for (const ledlook::ColourWord& w : ledlook::COLOUR_WORDS)
            wire = wire || (w.swatch == sw && strcmp(w.word, ledlook::SWATCHES[sw].word) == 0);
        TEST_ASSERT_TRUE_MESSAGE(wire, ledlook::SWATCHES[sw].word);
        TEST_ASSERT_TRUE_MESSAGE(wordsFor(sw) >= 2, ledlook::SWATCHES[sw].word);
    }
    TEST_ASSERT_EQUAL_INT(0, wordsFor(ledlook::LIGHT_BLUE));
}

// Each word names one colour, lowercase ASCII (the folded text songmood matches against).
void test_words_are_unique_lowercase_ascii() {
    for (int i = 0; i < ledlook::N_COLOUR_WORDS; i++) {
        const char* w = ledlook::COLOUR_WORDS[i].word;
        for (const char* p = w; *p; ++p) TEST_ASSERT_TRUE_MESSAGE(*p >= 'a' && *p <= 'z', w);
        for (int j = i + 1; j < ledlook::N_COLOUR_WORDS; j++)
            TEST_ASSERT_TRUE_MESSAGE(strcmp(w, ledlook::COLOUR_WORDS[j].word) != 0, w);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_colour_names_follow_the_swatch_table);
    RUN_TEST(test_every_grid_colour_has_words);
    RUN_TEST(test_words_are_unique_lowercase_ascii);
    return UNITY_END();
}
