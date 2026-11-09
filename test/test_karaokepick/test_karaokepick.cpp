#include <unity.h>
#include <cstring>
#include "../../src/util/black/karaokepick.h"

static lyricbuf::Lyrics L;   // ~5.6 KB: keep it off the stack
void setUp() {}
void tearDown() {}

static const char* text(int i) { return lyricbuf::lineText(L, i); }

void test_picks_prev_cur_next() {
    lyricbuf::parse("[00:01.00]A\n[00:04.00]B\n[00:07.00]C\n", L);
    const karaokepick::Pick p = karaokepick::pick(L, 5000, 0);
    TEST_ASSERT_EQUAL_STRING("A", text(p.prev));
    TEST_ASSERT_EQUAL_STRING("B", text(p.cur));
    TEST_ASSERT_EQUAL_STRING("C", text(p.next));
    TEST_ASSERT_EQUAL_UINT32(4000, p.startMs);
    TEST_ASSERT_EQUAL_UINT32(7000, p.endMs);
}

void test_intro_has_no_current() {
    lyricbuf::parse("[00:01.00]A\n[00:04.00]B\n", L);
    const karaokepick::Pick p = karaokepick::pick(L, 500, 0);
    TEST_ASSERT_EQUAL_INT(-1, p.cur);
    TEST_ASSERT_EQUAL_INT(-1, p.prev);
    TEST_ASSERT_EQUAL_STRING("A", text(p.next));
}

void test_blank_line_keeps_the_last_sung() {
    lyricbuf::parse("[00:01.00]A\n[00:04.00]\n[00:09.00]B\n", L);
    const karaokepick::Pick p = karaokepick::pick(L, 6000, 0);
    TEST_ASSERT_EQUAL_STRING("A", text(p.cur));
    TEST_ASSERT_EQUAL_UINT32(4000, p.endMs);  // the break ends the sweep: the line stays full
    TEST_ASSERT_EQUAL_STRING("B", text(p.next));
    TEST_ASSERT_EQUAL_INT(-1, p.prev);
}

void test_blank_is_never_prev() {
    lyricbuf::parse("[00:01.00]A\n[00:04.00]  \n[00:09.00]B\n", L);
    const karaokepick::Pick p = karaokepick::pick(L, 9500, 0);
    TEST_ASSERT_EQUAL_STRING("A", text(p.prev));
    TEST_ASSERT_EQUAL_STRING("B", text(p.cur));
}

void test_last_line_ends_at_song_or_cap() {
    lyricbuf::parse("[00:01.00]A\n", L);
    TEST_ASSERT_EQUAL_UINT32(3000, karaokepick::pick(L, 2000, 3000).endMs);
    TEST_ASSERT_EQUAL_UINT32(1000 + karaokepick::LAST_LINE_MS, karaokepick::pick(L, 2000, 0).endMs);
}

void test_sweep_is_proportional() { TEST_ASSERT_EQUAL_INT(150, karaokepick::sweepPx(5500, 4000, 7000, 300)); }

void test_sweep_clamps_before_start() { TEST_ASSERT_EQUAL_INT(0, karaokepick::sweepPx(3000, 4000, 7000, 300)); }

void test_sweep_clamps_after_end() { TEST_ASSERT_EQUAL_INT(300, karaokepick::sweepPx(9000, 4000, 7000, 300)); }

void test_zero_length_line_is_full() { TEST_ASSERT_EQUAL_INT(300, karaokepick::sweepPx(4000, 4000, 4000, 300)); }

void test_hop_goes_up_and_lands() {
    karaokepick::Hop h = karaokepick::hopAt(0, 250, 10);
    TEST_ASSERT_EQUAL_INT(250, h.x);
    TEST_ASSERT_EQUAL_INT(0, h.lift);
    TEST_ASSERT_FALSE(h.done);
    h = karaokepick::hopAt(karaokepick::HOP_MS / 2, 250, 10);
    TEST_ASSERT_EQUAL_INT(130, h.x);
    TEST_ASSERT_EQUAL_INT(karaokepick::HOP_PX, h.lift);
    h = karaokepick::hopAt(karaokepick::HOP_MS, 250, 10);
    TEST_ASSERT_EQUAL_INT(10, h.x);
    TEST_ASSERT_EQUAL_INT(0, h.lift);
    TEST_ASSERT_TRUE(h.done);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_picks_prev_cur_next);
    RUN_TEST(test_intro_has_no_current);
    RUN_TEST(test_blank_line_keeps_the_last_sung);
    RUN_TEST(test_blank_is_never_prev);
    RUN_TEST(test_last_line_ends_at_song_or_cap);
    RUN_TEST(test_sweep_is_proportional);
    RUN_TEST(test_sweep_clamps_before_start);
    RUN_TEST(test_sweep_clamps_after_end);
    RUN_TEST(test_zero_length_line_is_full);
    RUN_TEST(test_hop_goes_up_and_lands);
    return UNITY_END();
}
