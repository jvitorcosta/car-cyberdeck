#include <unity.h>
#include "../../src/util/yellow/heapguard.h"

using heapguard::Gate;
using heapguard::MIN_BLOCK;
using heapguard::MIN_FREE;
using heapguard::RESUME_MARGIN;

void setUp() {}
void tearDown() {}

// A desk heap (37 KB free, 16 KB block) asks.
void test_room_on_a_normal_heap() {
    Gate g;
    TEST_ASSERT_TRUE(g.room(37000, 16000));
    TEST_ASSERT_FALSE(g.low);
}

// The crash log's runs: 15.6 KB free, and 19.7 KB free with a 4 KB block, both held back.
void test_holds_back_at_the_crash_heaps() {
    Gate a;
    TEST_ASSERT_FALSE(a.room(15660, 124));
    Gate b;
    TEST_ASSERT_FALSE(b.room(19768, 4084));
}

// Back above MIN_FREE alone isn't enough: it waits for RESUME_MARGIN more.
void test_resumes_only_past_the_margin() {
    Gate g;
    TEST_ASSERT_FALSE(g.room(MIN_FREE - 1, 16000));
    TEST_ASSERT_FALSE(g.room(MIN_FREE + 1, 16000));
    TEST_ASSERT_FALSE(g.room(MIN_FREE + RESUME_MARGIN - 1, 16000));
    TEST_ASSERT_TRUE(g.room(MIN_FREE + RESUME_MARGIN, 16000));
}

// Plenty free but no block big enough: held back, and it stays so until the block is back.
void test_block_counts_both_ways() {
    Gate g;
    TEST_ASSERT_FALSE(g.room(30000, MIN_BLOCK - 1));
    TEST_ASSERT_FALSE(g.room(30000, MIN_BLOCK - 1));
    TEST_ASSERT_TRUE(g.room(30000, MIN_BLOCK));
}

// The edge itself still asks: MIN_FREE and MIN_BLOCK exactly.
void test_edge_asks() {
    Gate g;
    TEST_ASSERT_TRUE(g.room(MIN_FREE, MIN_BLOCK));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_room_on_a_normal_heap);
    RUN_TEST(test_holds_back_at_the_crash_heaps);
    RUN_TEST(test_resumes_only_past_the_margin);
    RUN_TEST(test_block_counts_both_ways);
    RUN_TEST(test_edge_asks);
    return UNITY_END();
}
