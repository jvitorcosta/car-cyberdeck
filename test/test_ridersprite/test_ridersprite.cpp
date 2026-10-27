#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/black/ridersprite.h"

void setUp() {}
void tearDown() {}

// "SHR1", u8 run frames, u8 w, u8 h, u8 0, 16 x u16 palette (LE), then run + 1 frames of w x h
// 4-bit indices (high nibble left; rows padded to whole bytes).
static std::vector<uint8_t> file(int run, int w, int h) {
    std::vector<uint8_t> b = {'S', 'H', 'R', '1', (uint8_t)run, (uint8_t)w, (uint8_t)h, 0};
    for (int i = 0; i < 16; ++i) { b.push_back((uint8_t)(0x10 * i)); b.push_back((uint8_t)i); }
    const int row = (w + 1) / 2;
    for (int f = 0; f <= run; ++f)
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < row; ++x) b.push_back(x == 0 && y == 0 ? (uint8_t)((f + 1) << 4) : 0);
    return b;
}

// Frame f's top-left pixel is palette entry f + 1; the rest is clear.
void test_valid_file() {
    const std::vector<uint8_t> b = file(2, 3, 2);
    TEST_ASSERT_TRUE(ridersprite::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(2, ridersprite::runFrames());
    TEST_ASSERT_EQUAL_INT(3, ridersprite::width());
    TEST_ASSERT_EQUAL_INT(2, ridersprite::height());
    uint16_t c = 0;
    TEST_ASSERT_TRUE(ridersprite::pixel(0, 0, 0, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0110, c);                       // entry 1: bytes 0x10, 0x01 LE
    TEST_ASSERT_TRUE(ridersprite::pixel(ridersprite::stand(), 0, 0, &c));
    TEST_ASSERT_EQUAL_HEX16(0x0330, c);                       // the standing frame: entry 3
    TEST_ASSERT_FALSE(ridersprite::pixel(0, 1, 0, &c));
    TEST_ASSERT_FALSE(ridersprite::pixel(0, 3, 0, &c));       // outside the box
    TEST_ASSERT_FALSE(ridersprite::pixel(3, 0, 0, &c));       // no such frame
}

// tools/media_sizes.py's stand-in ("SHR1" + zeros): none, so the rail keeps its jet glows.
void test_stand_in_reads_as_none() {
    std::vector<uint8_t> b = {'S', 'H', 'R', '1', 0, 0, 0, 0};
    b.resize(7168, 0);
    TEST_ASSERT_FALSE(ridersprite::parse(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(0, ridersprite::runFrames());
    uint16_t c;
    TEST_ASSERT_FALSE(ridersprite::pixel(0, 0, 0, &c));
}

void test_bad_files() {
    std::vector<uint8_t> b = file(2, 3, 2);
    TEST_ASSERT_FALSE(ridersprite::parse(b.data(), b.size() - 1));
    b[0] = 'X';
    TEST_ASSERT_FALSE(ridersprite::parse(b.data(), b.size()));
    TEST_ASSERT_FALSE(ridersprite::parse(nullptr, 64));
    const std::vector<uint8_t> big = file(1, 65, 2);          // wider than the screen's rider box
    TEST_ASSERT_FALSE(ridersprite::parse(big.data(), big.size()));
}

// The run loop: FRAME_MS per frame, round and round.
void test_frame_at() {
    const std::vector<uint8_t> b = file(8, 2, 2);
    ridersprite::parse(b.data(), b.size());
    TEST_ASSERT_EQUAL_INT(0, ridersprite::frameAt(0));
    TEST_ASSERT_EQUAL_INT(0, ridersprite::frameAt(ridersprite::FRAME_MS - 1));
    TEST_ASSERT_EQUAL_INT(1, ridersprite::frameAt(ridersprite::FRAME_MS));
    TEST_ASSERT_EQUAL_INT(0, ridersprite::frameAt(8 * ridersprite::FRAME_MS));
    TEST_ASSERT_TRUE(ridersprite::frameAt(0xFFFFFFFFu) < 8);   // in range at the wrap
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_file);
    RUN_TEST(test_stand_in_reads_as_none);
    RUN_TEST(test_bad_files);
    RUN_TEST(test_frame_at);
    return UNITY_END();
}
