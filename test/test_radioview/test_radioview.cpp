#include <unity.h>
#include <cstring>
#include "../../src/util/radioview.h"

void setUp() {}
void tearDown() {}

static stateparse::Deck deck(bool playing, uint32_t progress, uint32_t duration) {
    stateparse::Deck d{};
    d.playing = playing;
    d.progressMs = progress;
    d.durationMs = duration;
    strcpy(d.track, "Highway Song");
    strcpy(d.lyricKind, "none");
    return d;
}

void test_progress_interpolates_while_playing() {
    const stateparse::Deck d = deck(true, 61000, 200000);
    TEST_ASSERT_EQUAL_INT(61000, radioview::progressMs(d, 0));
    TEST_ASSERT_EQUAL_INT(62500, radioview::progressMs(d, 1500));
}

void test_progress_capped_at_duration() {
    const stateparse::Deck d = deck(true, 199000, 200000);
    TEST_ASSERT_EQUAL_INT(200000, radioview::progressMs(d, 5000));
    TEST_ASSERT_EQUAL_INT(200000, radioview::progressMs(d, 0xFFFFFFFFu));  // no wrap on a huge gap
}

void test_progress_paused_stays() {
    const stateparse::Deck d = deck(false, 61000, 200000);
    TEST_ASSERT_EQUAL_INT(61000, radioview::progressMs(d, 4000));
}

void test_progress_without_duration_is_not_capped_to_zero() {
    const stateparse::Deck d = deck(true, 1000, 0);
    TEST_ASSERT_EQUAL_INT(2000, radioview::progressMs(d, 1000));
}

void test_clock_formats() {
    char b[16];
    radioview::clock(0, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0:00", b);
    radioview::clock(187000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("3:07", b);
    radioview::clock(187999, b, sizeof(b));  // whole seconds, rounded down
    TEST_ASSERT_EQUAL_STRING("3:07", b);
    radioview::clock(3599000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("59:59", b);
    radioview::clock(3723000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("1:02:03", b);
    char tiny[3];
    radioview::clock(187000, tiny, sizeof(tiny));  // cut, still terminated
    TEST_ASSERT_EQUAL_STRING("3:", tiny);
}

void test_cells() {
    TEST_ASSERT_EQUAL_INT(0, radioview::cells(0, 200000, 20));
    TEST_ASSERT_EQUAL_INT(10, radioview::cells(100000, 200000, 20));
    TEST_ASSERT_EQUAL_INT(20, radioview::cells(200000, 200000, 20));
    TEST_ASSERT_EQUAL_INT(20, radioview::cells(250000, 200000, 20));  // past the end: full
    TEST_ASSERT_EQUAL_INT(0, radioview::cells(100000, 0, 20));
    TEST_ASSERT_EQUAL_INT(19, radioview::cells(199999, 200000, 20));  // full only at the end
    TEST_ASSERT_EQUAL_INT(20, radioview::cells(0xFFFFFFFFu, 0xFFFFFFFFu, 20));  // no overflow
}

void test_state_words() {
    TEST_ASSERT_EQUAL_STRING(">NO SIGNAL", radioview::state(false, false, false, false));
    TEST_ASSERT_EQUAL_STRING(">NO SIGNAL", radioview::state(false, false, true, true));
    TEST_ASSERT_EQUAL_STRING(">WRONG KEY", radioview::state(false, true, false, false));  // a 403: no state came
    TEST_ASSERT_EQUAL_STRING(">STANDBY", radioview::state(true, false, false, true));
    TEST_ASSERT_EQUAL_STRING(">STANDBY", radioview::state(true, false, true, false));
    TEST_ASSERT_EQUAL_STRING("ON AIR", radioview::state(true, false, true, true));
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

void test_lyric_synced() {
    const stateparse::Deck d = synced();
    TEST_ASSERT_EQUAL_STRING("", radioview::lyric(d, 58999));
    TEST_ASSERT_EQUAL_STRING("first made-up line", radioview::lyric(d, 59000));
    TEST_ASSERT_EQUAL_STRING("first made-up line", radioview::lyric(d, 63999));
    TEST_ASSERT_EQUAL_STRING("second made-up line", radioview::lyric(d, 64000));
    TEST_ASSERT_EQUAL_STRING("second made-up line", radioview::lyric(d, 90000));
}

void test_lyric_next_unknown() {
    stateparse::Deck d = synced();
    d.nextStartMs = 0;  // the last line, or not known: the current one stays
    TEST_ASSERT_EQUAL_STRING("first made-up line", radioview::lyric(d, 90000));
}

void test_lyric_not_synced() {
    stateparse::Deck d = synced();
    strcpy(d.lyricKind, "plain");
    TEST_ASSERT_EQUAL_STRING("", radioview::lyric(d, 60000));
    strcpy(d.lyricKind, "none");
    TEST_ASSERT_EQUAL_STRING("", radioview::lyric(d, 60000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_progress_interpolates_while_playing);
    RUN_TEST(test_progress_capped_at_duration);
    RUN_TEST(test_progress_paused_stays);
    RUN_TEST(test_progress_without_duration_is_not_capped_to_zero);
    RUN_TEST(test_clock_formats);
    RUN_TEST(test_cells);
    RUN_TEST(test_state_words);
    RUN_TEST(test_lyric_synced);
    RUN_TEST(test_lyric_next_unknown);
    RUN_TEST(test_lyric_not_synced);
    return UNITY_END();
}
