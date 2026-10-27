#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/black/backdrop.h"

void setUp() {}
void tearDown() {}

// "BKD1", u16 width, u16 height (LE), then width x height big-endian RGB565 pixels.
static std::vector<uint8_t> file(int w, int h) {
    std::vector<uint8_t> b = {'B', 'K', 'D', '1', (uint8_t)w, (uint8_t)(w >> 8), (uint8_t)h, (uint8_t)(h >> 8)};
    for (int i = 0; i < w * h; ++i) { b.push_back((uint8_t)(i >> 8)); b.push_back((uint8_t)i); }
    return b;
}

void test_valid_file() {
    const std::vector<uint8_t> b = file(4, 2);
    TEST_ASSERT_TRUE(backdrop::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(4, backdrop::width());
    TEST_ASSERT_EQUAL_INT(2, backdrop::height());
    TEST_ASSERT_EQUAL_PTR(b.data() + 8, backdrop::row(0));
    TEST_ASSERT_EQUAL_PTR(b.data() + 8 + 8, backdrop::row(1));
    TEST_ASSERT_NULL(backdrop::row(2));
    TEST_ASSERT_NULL(backdrop::row(-1));
}

// What a fresh checkout and CI have: tools/media_sizes.py's stand-in ("BKD1" + zeros, either 8
// bytes or the real size). It must read as none, so the old update screen stays.
void test_stand_in_reads_as_none() {
    std::vector<uint8_t> b = {'B', 'K', 'D', '1', 0, 0, 0, 0};
    TEST_ASSERT_FALSE(backdrop::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, backdrop::width());
    TEST_ASSERT_NULL(backdrop::row(0));
    b.resize(153608, 0);
    TEST_ASSERT_FALSE(backdrop::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, backdrop::height());
}

void test_bad_magic_or_length() {
    std::vector<uint8_t> b = file(4, 2);
    b[0] = 'X';
    TEST_ASSERT_FALSE(backdrop::parse(b.data(), b.size()));
    b = file(4, 2);
    TEST_ASSERT_FALSE(backdrop::parse(b.data(), b.size() - 1));
    TEST_ASSERT_FALSE(backdrop::parse(nullptr, 100));
}

// Nothing bigger than the 320 x 240 screen.
void test_too_big() {
    const std::vector<uint8_t> wide = file(321, 1);
    TEST_ASSERT_FALSE(backdrop::parse(wide.data(), wide.size()));
    const std::vector<uint8_t> tall = file(1, 241);
    TEST_ASSERT_FALSE(backdrop::parse(tall.data(), tall.size()));
}

// A good file after a bad one, and a bad one after a good one: the last parse wins.
void test_last_parse_wins() {
    const std::vector<uint8_t> good = file(3, 3);
    TEST_ASSERT_TRUE(backdrop::parse(good.data(), good.size()));
    TEST_ASSERT_FALSE(backdrop::parse(nullptr, 0));
    TEST_ASSERT_EQUAL_INT(0, backdrop::width());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_file);
    RUN_TEST(test_stand_in_reads_as_none);
    RUN_TEST(test_bad_magic_or_length);
    RUN_TEST(test_too_big);
    RUN_TEST(test_last_parse_wins);
    return UNITY_END();
}
