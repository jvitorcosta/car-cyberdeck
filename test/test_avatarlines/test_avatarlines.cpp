#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/avatarlines.h"

using avatarlines::Event;
void setUp() {}
void tearDown() {}

static avatarlines::Out out;
static avatarlines::In track(bool spoke, uint32_t since, uint32_t rnd) {
    return {Event::Track, spoke, since, rnd, "Made Up Song", "Some Band", "Pikachu"};
}

void test_fill_replaces_placeholders() {
    char b[96];
    avatarlines::fill("{A} - {T}, {P}!", "Song", "Band", "Pikachu", b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("Band - Song, PIKACHU!", b);
}
// Review Focus 3: empty values get a fallback word.
void test_fill_defaults_for_empty_values() {
    char b[96];
    avatarlines::fill("{T}|{A}|{P}", "", nullptr, "", b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("this song|this artist|POK\xC3\xA9MON", b);
}
void test_fill_keeps_unknown_braces() {
    char b[32];
    avatarlines::fill("{X} {T", "s", "a", "p", b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("{X} {T", b);
}
// Review Focus 3: long values are cut, never mid-character.
void test_fill_never_cuts_a_utf8_character() {
    char b[8];                                          // room for 7 bytes
    avatarlines::fill("{T}", "abcde\xC3\xA9", "", "", b, sizeof b);   // 5 + 2 bytes fit
    TEST_ASSERT_EQUAL_STRING("abcde\xC3\xA9", b);
    char c[7];                                          // room for 6: the 2-byte é doesn't fit
    avatarlines::fill("{T}", "abcde\xC3\xA9", "", "", c, sizeof c);
    TEST_ASSERT_EQUAL_STRING("abcde", c);
}
void test_fill_tiny_buffer() {
    char b[1] = {'x'};
    avatarlines::fill("{T}", "song", "", "", b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("", b);
}
void test_boot_always_speaks() {
    for (uint32_t r = 0; r < 6; ++r) {
        avatarlines::In in{Event::Boot, false, 0, r, "", "", ""};
        TEST_ASSERT_TRUE(avatarlines::pick(in, out));
        const avatarlines::Line& l = avatarlines::BOOT[r % avatarlines::BOOT_N];
        char want[avatarlines::TEXT_CAP];
        avatarlines::fill(l.text, "", "", "", want, sizeof want);
        TEST_ASSERT_EQUAL_STRING(want, out.text);
        TEST_ASSERT_EQUAL_UINT8(l.mood, out.mood);
    }
}
// Review Focus 2: no remark within 5 minutes of her last scene (the boot greeting included).
void test_track_too_soon_after_boot_greeting() {
    TEST_ASSERT_FALSE(avatarlines::pick(track(true, 20000, 0), out));
    TEST_ASSERT_FALSE(avatarlines::pick(track(true, avatarlines::GAP_MS - 1, 0), out));
}
void test_track_odds() {
    TEST_ASSERT_FALSE(avatarlines::pick(track(true, avatarlines::GAP_MS, 1), out));
    TEST_ASSERT_FALSE(avatarlines::pick(track(true, avatarlines::GAP_MS, 2), out));
    TEST_ASSERT_TRUE(avatarlines::pick(track(true, avatarlines::GAP_MS, 3), out));
}
void test_track_line_is_filled() {
    const uint32_t rnd = avatarlines::ODDS * 4;          // passes the odds, picks TRACK[4]
    TEST_ASSERT_TRUE(avatarlines::pick(track(false, 0, rnd), out));
    const avatarlines::Line& l = avatarlines::TRACK[4 % avatarlines::TRACK_N];
    char want[avatarlines::TEXT_CAP];
    avatarlines::fill(l.text, "Made Up Song", "Some Band", "Pikachu", want, sizeof want);
    TEST_ASSERT_EQUAL_STRING(want, out.text);
    TEST_ASSERT_EQUAL_UINT8(l.mood, out.mood);
}
void test_every_line_fits_with_long_values() {
    std::string longv(95, 'w');                            // a 95-byte title / artist
    for (int i = 0; i < avatarlines::TRACK_N; ++i) {
        char b[avatarlines::TEXT_CAP];
        avatarlines::fill(avatarlines::TRACK[i].text, longv.c_str(), longv.c_str(), "Pikachu", b, sizeof b);
        TEST_ASSERT_TRUE(strlen(b) < avatarlines::TEXT_CAP);
        TEST_ASSERT_TRUE(avatarlines::TRACK[i].mood <= 2);
    }
    for (int i = 0; i < avatarlines::BOOT_N; ++i) TEST_ASSERT_TRUE(avatarlines::BOOT[i].mood <= 2);
}

// The old deck has no Pokemon name: lines with {P} are skipped, never "A wild POKéMON!".
void test_no_pokemon_lines_without_a_pokemon() {
    for (uint32_t k = 0; k < (uint32_t)avatarlines::TRACK_N; ++k) {
        avatarlines::In in{Event::Track, false, 0, k * avatarlines::ODDS, "Song", "Band", ""};
        TEST_ASSERT_TRUE(avatarlines::pick(in, out));
        TEST_ASSERT_NULL(strstr(out.text, "POK\xC3\xA9MON"));
    }
}

// Reactions always speak: no gap, no odds.
void test_reactions_always_speak() {
    const Event evs[] = {Event::Howl, Event::Vroom, Event::TripReset, Event::Busy, Event::Unreachable};
    for (int i = 0; i < 5; ++i) {
        avatarlines::In in{evs[i], true, 0, 1, "", "", ""};   // just spoke; rnd fails the odds
        TEST_ASSERT_TRUE(avatarlines::pick(in, out));
        TEST_ASSERT_EQUAL_STRING(avatarlines::REACT[i].text, out.text);
        TEST_ASSERT_EQUAL_UINT8(avatarlines::REACT[i].mood, out.mood);
    }
    TEST_ASSERT_EQUAL_STRING("I can't reach the deck, darling.", avatarlines::REACT[4].text);
    TEST_ASSERT_EQUAL_STRING("One at a time, darling!", avatarlines::REACT[3].text);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fill_replaces_placeholders);
    RUN_TEST(test_fill_defaults_for_empty_values);
    RUN_TEST(test_fill_keeps_unknown_braces);
    RUN_TEST(test_fill_never_cuts_a_utf8_character);
    RUN_TEST(test_fill_tiny_buffer);
    RUN_TEST(test_boot_always_speaks);
    RUN_TEST(test_track_too_soon_after_boot_greeting);
    RUN_TEST(test_track_odds);
    RUN_TEST(test_track_line_is_filled);
    RUN_TEST(test_every_line_fits_with_long_values);
    RUN_TEST(test_no_pokemon_lines_without_a_pokemon);
    RUN_TEST(test_reactions_always_speak);
    return UNITY_END();
}
