#include <unity.h>
#include <cstdint>
#include "../../src/util/yellow/padedges.h"

using padedges::Latch;

void setUp() {}
void tearDown() {}

static constexpr uint32_t A = 1u << 14, START = 1u << 3, L1 = 1u << 10;

void test_a_press_is_reported_once() {
    Latch l;
    TEST_ASSERT_EQUAL_UINT32(A, l.feed(A));
    TEST_ASSERT_EQUAL_UINT32(0, l.feed(A));  // held on: no new edge
    TEST_ASSERT_EQUAL_UINT32(0, l.feed(0));
    TEST_ASSERT_EQUAL_UINT32(A, l.feed(A));  // pressed again
}

void test_a_press_waits_until_taken() {
    Latch l;
    l.feed(START | A);
    l.take(START);  // the shortcut acted and the pass ended
    TEST_ASSERT_EQUAL_UINT32(A, l.waiting());
    l.feed(START | A);  // next pass: still held, no new edge, but A still waits
    TEST_ASSERT_EQUAL_UINT32(A, l.waiting());
    l.take(A);
    TEST_ASSERT_EQUAL_UINT32(0, l.waiting());
}

void test_a_tap_let_go_before_the_next_pass_still_waits() {
    Latch l;
    l.feed(START | A);
    l.take(START);
    l.feed(0);  // A already released: the press is not lost
    TEST_ASSERT_EQUAL_UINT32(A, l.waiting());
}

void test_a_screen_change_drops_the_waiting_presses() {
    Latch l;
    l.feed(A | L1);
    l.take(A);
    l.drop();
    TEST_ASSERT_EQUAL_UINT32(0, l.waiting());
    TEST_ASSERT_EQUAL_UINT32(0, l.feed(L1));  // still held across the change: no new press
    TEST_ASSERT_EQUAL_UINT32(0, l.waiting());
}

void test_reset_forgets_everything() {
    Latch l;
    l.feed(A);
    l.reset();
    TEST_ASSERT_EQUAL_UINT32(0, l.waiting());
    TEST_ASSERT_EQUAL_UINT32(A, l.feed(A));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_press_is_reported_once);
    RUN_TEST(test_a_press_waits_until_taken);
    RUN_TEST(test_a_tap_let_go_before_the_next_pass_still_waits);
    RUN_TEST(test_a_screen_change_drops_the_waiting_presses);
    RUN_TEST(test_reset_forgets_everything);
    return UNITY_END();
}
