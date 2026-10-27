#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/black/carbin.h"

void setUp() {}
void tearDown() {}

// A 3x2 car: pixel (1,0) is HEAD (4) red 0xF800, (2,1) is LOWER (2) 0x07E0, the rest clear.
static std::vector<uint8_t> blob() {
    std::vector<uint8_t> b = {'C', 'A', 'R', '1'};
    const uint16_t head[8] = {3, 2, 0, 2, 1, 3, 0, 1};   // w h rearX frontX wheelY headX headY tailY
    for (uint16_t v : head) { b.push_back(v & 255); b.push_back(v >> 8); }
    const uint8_t kinds[3] = {0x04, 0x00, 0x02};          // pixels 0,1 | 2,3 | 4,5
    b.insert(b.end(), kinds, kinds + 3);
    const uint16_t px[6] = {0, 0xF800, 0, 0, 0, 0x07E0};
    for (uint16_t v : px) { b.push_back(v & 255); b.push_back(v >> 8); }
    return b;
}

void test_parses_header_and_pixels() {
    const std::vector<uint8_t> b = blob();
    carbin::Car c;
    TEST_ASSERT_TRUE(carbin::parse(b.data(), b.size(), &c));
    TEST_ASSERT_EQUAL_INT(3, c.w);
    TEST_ASSERT_EQUAL_INT(2, c.h);
    TEST_ASSERT_EQUAL_INT(3, c.headX);
    TEST_ASSERT_EQUAL_INT(1, c.tailY);
    uint16_t col = 0;
    uint8_t kind = 0;
    TEST_ASSERT_TRUE(carbin::pixel(c, 1, 0, &col, &kind));
    TEST_ASSERT_EQUAL_HEX16(0xF800, col);
    TEST_ASSERT_EQUAL_UINT8(4, kind);
    TEST_ASSERT_TRUE(carbin::pixel(c, 2, 1, &col, &kind));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, col);
    TEST_ASSERT_FALSE(carbin::pixel(c, 0, 0, &col, &kind));
    TEST_ASSERT_EQUAL_UINT8(0, kind);
    TEST_ASSERT_FALSE(carbin::pixel(c, 3, 0, &col, &kind));   // outside
    TEST_ASSERT_FALSE(carbin::pixel(c, -1, 0, nullptr, nullptr));
}

// Review focus 4: an interrupted tool run or a stale format is refused.
void test_rejects_truncated_and_bad_magic() {
    std::vector<uint8_t> b = blob();
    carbin::Car c;
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size() - 1, &c));
    TEST_ASSERT_FALSE(carbin::parse(b.data(), 10, &c));
    TEST_ASSERT_FALSE(carbin::parse(nullptr, 0, &c));
    TEST_ASSERT_FALSE(carbin::parse(b.data(), 2, &c));   // the build's one-sample stand-in
    b[3] = '2';
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size(), &c));
}

void test_rejects_zero_or_huge_size() {
    std::vector<uint8_t> b = blob();
    b[4] = 0; b[5] = 0;                                   // w = 0
    carbin::Car c;
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size(), &c));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_header_and_pixels);
    RUN_TEST(test_rejects_truncated_and_bad_magic);
    RUN_TEST(test_rejects_zero_or_huge_size);
    return UNITY_END();
}
