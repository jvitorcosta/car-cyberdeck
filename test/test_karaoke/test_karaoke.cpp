#include <unity.h>
#include <cstring>
#include "../../src/util/karaoke.h"

void setUp() {}
void tearDown() {}

static stateparse::Deck deck(bool playing, uint32_t progress, uint32_t duration) {
    stateparse::Deck d{};
    d.playing = playing;
    d.progressMs = progress;
    d.durationMs = duration;
    strcpy(d.track, "Highway Song");
    strcpy(d.artist, "Made-up Band");
    strcpy(d.lyricKind, "none");
    return d;
}

static stateparse::Deck synced() {
    stateparse::Deck d = deck(true, 0, 200000);
    strcpy(d.lyricKind, "synced");
    strcpy(d.line, "first made-up line");
    strcpy(d.next, "second made-up line");
    d.lineStartMs = 59000;
    d.nextStartMs = 64000;
    return d;
}

void test_progress_interpolates_while_playing() {
    const stateparse::Deck d = deck(true, 61000, 200000);
    TEST_ASSERT_EQUAL_INT(61000, karaoke::progressMs(d, 0));
    TEST_ASSERT_EQUAL_INT(62500, karaoke::progressMs(d, 1500));
}

void test_progress_capped_at_duration() {
    const stateparse::Deck d = deck(true, 199000, 200000);
    TEST_ASSERT_EQUAL_INT(200000, karaoke::progressMs(d, 5000));
    TEST_ASSERT_EQUAL_INT(200000, karaoke::progressMs(d, 0xFFFFFFFFu));  // no wrap on a huge gap
}

void test_progress_paused_stays() {
    const stateparse::Deck d = deck(false, 61000, 200000);
    TEST_ASSERT_EQUAL_INT(61000, karaoke::progressMs(d, 4000));
}

void test_progress_without_duration_is_not_capped_to_zero() {
    const stateparse::Deck d = deck(true, 1000, 0);
    TEST_ASSERT_EQUAL_INT(2000, karaoke::progressMs(d, 1000));
}

void test_cells() {
    TEST_ASSERT_EQUAL_INT(0, karaoke::cells(0, 200000, 20));
    TEST_ASSERT_EQUAL_INT(10, karaoke::cells(100000, 200000, 20));
    TEST_ASSERT_EQUAL_INT(20, karaoke::cells(200000, 200000, 20));
    TEST_ASSERT_EQUAL_INT(20, karaoke::cells(250000, 200000, 20));  // past the end: full
    TEST_ASSERT_EQUAL_INT(0, karaoke::cells(100000, 0, 20));
    TEST_ASSERT_EQUAL_INT(19, karaoke::cells(199999, 200000, 20));  // full only at the end
    TEST_ASSERT_EQUAL_INT(20, karaoke::cells(0xFFFFFFFFu, 0xFFFFFFFFu, 20));  // no overflow
    TEST_ASSERT_EQUAL_INT(156, karaoke::cells(100000, 200000, 312));  // a pixel bar
}

void test_available() {
    const stateparse::Deck d = synced();
    TEST_ASSERT_TRUE(karaoke::available(true, true, d));
    TEST_ASSERT_FALSE(karaoke::available(false, true, d));  // the black deck isn't heard
    TEST_ASSERT_FALSE(karaoke::available(true, false, d));  // paused
    stateparse::Deck e = d;
    strcpy(e.lyricKind, "plain");
    TEST_ASSERT_FALSE(karaoke::available(true, true, e));
    strcpy(e.lyricKind, "none");
    TEST_ASSERT_FALSE(karaoke::available(true, true, e));
    strcpy(e.lyricKind, "instrumental");
    TEST_ASSERT_FALSE(karaoke::available(true, true, e));
    e = d;
    e.line[0] = 0;  // before the first line: the next one is known
    TEST_ASSERT_TRUE(karaoke::available(true, true, e));
    e.nextStartMs = 0;  // synced, but no line at all
    TEST_ASSERT_FALSE(karaoke::available(true, true, e));
    e = d;
    e.nextStartMs = 0;  // the last line
    TEST_ASSERT_TRUE(karaoke::available(true, true, e));
}

void test_state_words() {
    const stateparse::Deck d = synced();
    TEST_ASSERT_EQUAL_STRING("", karaoke::state(true, false, true, d));  // sing
    TEST_ASSERT_EQUAL_STRING(">WRONG KEY", karaoke::state(false, true, true, d));
    TEST_ASSERT_EQUAL_STRING(">WRONG KEY", karaoke::state(true, true, true, d));
    TEST_ASSERT_EQUAL_STRING(">NO SIGNAL", karaoke::state(false, false, true, d));
    TEST_ASSERT_EQUAL_STRING(">STANDBY", karaoke::state(true, false, false, d));  // paused
    stateparse::Deck e = d;
    e.track[0] = 0;
    TEST_ASSERT_EQUAL_STRING(">STANDBY", karaoke::state(true, false, true, e));  // nothing plays
    e = d;
    strcpy(e.lyricKind, "plain");
    TEST_ASSERT_EQUAL_STRING(">NO SYNCED LYRICS", karaoke::state(true, false, true, e));
}

void test_why_not() {
    stateparse::Deck d = synced();
    TEST_ASSERT_EQUAL_STRING(">NO SIGNAL", karaoke::whyNot(false, false, true, d));
    TEST_ASSERT_EQUAL_STRING(">WRONG KEY", karaoke::whyNot(false, true, true, d));
    TEST_ASSERT_EQUAL_STRING(">WRONG KEY", karaoke::whyNot(true, true, true, d));
    TEST_ASSERT_EQUAL_STRING(">STANDBY", karaoke::whyNot(true, false, false, d));  // paused
    stateparse::Deck e = d;
    e.track[0] = 0;
    TEST_ASSERT_EQUAL_STRING(">STANDBY", karaoke::whyNot(true, false, true, e));  // nothing plays
    e = d;
    strcpy(e.lyricKind, "plain");
    TEST_ASSERT_EQUAL_STRING(">NO SYNCED LYRICS", karaoke::whyNot(true, false, true, e));
}

void test_lyric_synced() {
    const stateparse::Deck d = synced();
    TEST_ASSERT_EQUAL_STRING("", karaoke::lyric(d, 58999));
    TEST_ASSERT_EQUAL_STRING("first made-up line", karaoke::lyric(d, 59000));
    TEST_ASSERT_EQUAL_STRING("first made-up line", karaoke::lyric(d, 63999));
    TEST_ASSERT_EQUAL_STRING("second made-up line", karaoke::lyric(d, 64000));
    TEST_ASSERT_EQUAL_STRING("second made-up line", karaoke::lyric(d, 90000));
}

void test_lyric_next_unknown() {
    stateparse::Deck d = synced();
    d.nextStartMs = 0;  // the last line, or not known: the current one stays
    TEST_ASSERT_EQUAL_STRING("first made-up line", karaoke::lyric(d, 90000));
}

void test_lyric_not_synced() {
    stateparse::Deck d = synced();
    strcpy(d.lyricKind, "plain");
    TEST_ASSERT_EQUAL_STRING("", karaoke::lyric(d, 60000));
    strcpy(d.lyricKind, "none");
    TEST_ASSERT_EQUAL_STRING("", karaoke::lyric(d, 60000));
}

void test_next_line() {
    stateparse::Deck d = synced();
    TEST_ASSERT_EQUAL_STRING("first made-up line", karaoke::nextLine(d, 58999));  // before it: it comes next
    TEST_ASSERT_EQUAL_STRING("second made-up line", karaoke::nextLine(d, 59000));
    TEST_ASSERT_EQUAL_STRING("second made-up line", karaoke::nextLine(d, 63999));
    TEST_ASSERT_EQUAL_STRING("", karaoke::nextLine(d, 64000));  // after the switch: not known till the next poll
    d.nextStartMs = 0;
    TEST_ASSERT_EQUAL_STRING("", karaoke::nextLine(d, 60000));  // the last line
    d = synced();
    strcpy(d.lyricKind, "plain");
    TEST_ASSERT_EQUAL_STRING("", karaoke::nextLine(d, 60000));
}

// A monospace stand-in for the TFT fonts: 10 px a character in font 2, 14 in font 4.
static int mono(const char* s, int font) { return static_cast<int>(strlen(s)) * (font == 4 ? 14 : 10); }

void test_wrap_empty_and_one_line() {
    karaoke::Line out[3];
    bool cut = true;
    TEST_ASSERT_EQUAL_INT(0, karaoke::wrap("", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(0, karaoke::wrap("   ", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("hello you", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("hello you", out[0].s);
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("exactly10!", 2, mono, 100, 3, out, &cut));  // 100 px: fits
    TEST_ASSERT_EQUAL_STRING("exactly10!", out[0].s);
}

void test_wrap_breaks_at_spaces() {
    karaoke::Line out[3];
    bool cut = true;
    // 10 characters a line
    TEST_ASSERT_EQUAL_INT(3, karaoke::wrap("we sing  along the highway", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("we sing", out[0].s);
    TEST_ASSERT_EQUAL_STRING("along the", out[1].s);
    TEST_ASSERT_EQUAL_STRING("highway", out[2].s);
    TEST_ASSERT_FALSE(cut);
    // the font is passed through: 14 px a character fits 7
    TEST_ASSERT_EQUAL_INT(2, karaoke::wrap("we sing along", 4, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("we sing", out[0].s);
    TEST_ASSERT_EQUAL_STRING("along", out[1].s);
}

void test_wrap_breaks_a_long_word() {
    karaoke::Line out[3];
    bool cut = true;
    TEST_ASSERT_EQUAL_INT(2, karaoke::wrap("abcdefghijklmno", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("abcdefghij", out[0].s);
    TEST_ASSERT_EQUAL_STRING("klmno", out[1].s);
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(3, karaoke::wrap("ab abcdefghijklmno", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("ab", out[0].s);  // the word moves down whole first
    TEST_ASSERT_EQUAL_STRING("abcdefghij", out[1].s);
    TEST_ASSERT_EQUAL_STRING("klmno", out[2].s);
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(2, karaoke::wrap("ab abcdefghijklmno", 2, mono, 100, 2, out, &cut));
    TEST_ASSERT_EQUAL_STRING("ab", out[0].s);
    TEST_ASSERT_EQUAL_STRING("abcdefgh..", out[1].s);  // the last line: cut, not broken
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("abc", 2, mono, 5, 1, out, &cut));  // nothing fits: ".."
    TEST_ASSERT_EQUAL_STRING("..", out[0].s);
    TEST_ASSERT_TRUE(cut);
}

void test_wrap_cuts_the_last_line() {
    karaoke::Line out[3];
    bool cut = false;
    TEST_ASSERT_EQUAL_INT(3, karaoke::wrap("we sing along the highway all night long", 2, mono, 100, 3, out, &cut));
    TEST_ASSERT_EQUAL_STRING("we sing", out[0].s);
    TEST_ASSERT_EQUAL_STRING("along the", out[1].s);
    TEST_ASSERT_EQUAL_STRING("highway..", out[2].s);  // the trailing space before ".." goes
    TEST_ASSERT_TRUE(cut);
    // one line: fitText's job
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("Highway Song", 2, mono, 60, 1, out, &cut));
    TEST_ASSERT_EQUAL_STRING("High..", out[0].s);
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("Highway Song", 2, mono, 120, 1, out, &cut));
    TEST_ASSERT_EQUAL_STRING("Highway Song", out[0].s);
    TEST_ASSERT_FALSE(cut);
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap("Highway Song", 2, mono, 120, 1, out));  // cut is optional
}

void test_wrap_text_longer_than_a_line_buffer() {
    char big[400];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    karaoke::Line out[1];
    bool cut = false;
    TEST_ASSERT_EQUAL_INT(1, karaoke::wrap(big, 2, mono, 100000, 1, out, &cut));  // wide enough, but no room
    TEST_ASSERT_TRUE(cut);
    const size_t n = strlen(out[0].s);
    TEST_ASSERT_EQUAL_UINT(sizeof(out[0].s) - 1, n);
    TEST_ASSERT_EQUAL_STRING("..", out[0].s + n - 2);
    karaoke::Line two[2];
    TEST_ASSERT_EQUAL_INT(2, karaoke::wrap(big, 2, mono, 100000, 2, two, &cut));
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_EQUAL_UINT(sizeof(two[0].s) - 1, strlen(two[0].s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_progress_interpolates_while_playing);
    RUN_TEST(test_progress_capped_at_duration);
    RUN_TEST(test_progress_paused_stays);
    RUN_TEST(test_progress_without_duration_is_not_capped_to_zero);
    RUN_TEST(test_cells);
    RUN_TEST(test_available);
    RUN_TEST(test_state_words);
    RUN_TEST(test_why_not);
    RUN_TEST(test_lyric_synced);
    RUN_TEST(test_lyric_next_unknown);
    RUN_TEST(test_lyric_not_synced);
    RUN_TEST(test_next_line);
    RUN_TEST(test_wrap_empty_and_one_line);
    RUN_TEST(test_wrap_breaks_at_spaces);
    RUN_TEST(test_wrap_breaks_a_long_word);
    RUN_TEST(test_wrap_cuts_the_last_line);
    RUN_TEST(test_wrap_text_longer_than_a_line_buffer);
    return UNITY_END();
}
