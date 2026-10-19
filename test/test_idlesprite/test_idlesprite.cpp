#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/util/idlesprite.h"

void setUp() {}
void tearDown() {}

// A 4-frame, 4x2 file: each frame's top-left pixel is colour index frame+1, the rest clear.
static uint8_t s_blob[8 + 32 + 4 * 4];
static size_t blob() {
    memcpy(s_blob, "IDL1", 4);
    s_blob[4] = 4; s_blob[5] = 4; s_blob[6] = 2; s_blob[7] = 0;
    for (int i = 0; i < 16; ++i) { s_blob[8 + 2 * i] = (uint8_t)(i * 3); s_blob[9 + 2 * i] = 0x80; }
    memset(s_blob + 40, 0, 16);
    for (int f = 0; f < 4; ++f) s_blob[40 + f * 4] = (uint8_t)((f + 1) << 4);
    return sizeof s_blob;
}

void test_parses_and_reads_pixels() {
    TEST_ASSERT_TRUE(idlesprite::parse(s_blob, blob()));
    TEST_ASSERT_EQUAL_INT(4, idlesprite::count());
    TEST_ASSERT_EQUAL_INT(4, idlesprite::width());
    TEST_ASSERT_EQUAL_INT(2, idlesprite::height());
    uint16_t c = 0;
    TEST_ASSERT_TRUE(idlesprite::pixel(2, 0, 0, &c));
    TEST_ASSERT_EQUAL_HEX16(0x8000 | 9, c);              // palette entry 3, little-endian
    TEST_ASSERT_FALSE(idlesprite::pixel(2, 1, 0, &c));   // index 0: clear
    TEST_ASSERT_FALSE(idlesprite::pixel(2, 4, 0, &c));   // outside the box
    TEST_ASSERT_FALSE(idlesprite::pixel(9, 0, 0, &c));   // no such frame
}

void test_bad_files_read_as_none() {
    blob();
    TEST_ASSERT_FALSE(idlesprite::parse(s_blob, sizeof s_blob - 1));   // cut off
    TEST_ASSERT_EQUAL_INT(0, idlesprite::count());
    s_blob[0] = 'X';
    TEST_ASSERT_FALSE(idlesprite::parse(s_blob, sizeof s_blob));       // wrong magic
    blob();
    s_blob[4] = 3;                                                     // not the 4 frames
    TEST_ASSERT_FALSE(idlesprite::parse(s_blob, sizeof s_blob));
    const uint8_t empty[8] = {'I', 'D', 'L', '1', 0, 0, 0, 0};          // the build's stand-in
    TEST_ASSERT_FALSE(idlesprite::parse(empty, sizeof empty));
    TEST_ASSERT_FALSE(idlesprite::parse(nullptr, 0));
    uint16_t c;
    TEST_ASSERT_FALSE(idlesprite::pixel(0, 0, 0, &c));
}

// Sonic 1's waiting animation (s1disasm _anim/Sonic.asm, SonAni_Wait): 24 ticks a frame at
// 60 Hz = 400 ms; 12 x Stand, Wait2, Wait1 x 3, then Wait2 / Wait3 for ever (the foot tap).
void test_the_games_waiting_timeline() {
    using idlesprite::STAND; using idlesprite::WAIT1; using idlesprite::WAIT2; using idlesprite::WAIT3;
    TEST_ASSERT_EQUAL_INT(STAND, idlesprite::frameAt(0));
    TEST_ASSERT_EQUAL_INT(STAND, idlesprite::frameAt(4799));
    TEST_ASSERT_EQUAL_INT(WAIT2, idlesprite::frameAt(4800));
    TEST_ASSERT_EQUAL_INT(WAIT1, idlesprite::frameAt(5200));
    TEST_ASSERT_EQUAL_INT(WAIT1, idlesprite::frameAt(6399));
    TEST_ASSERT_EQUAL_INT(WAIT2, idlesprite::frameAt(6400));
    TEST_ASSERT_EQUAL_INT(WAIT3, idlesprite::frameAt(6800));
    TEST_ASSERT_EQUAL_INT(WAIT2, idlesprite::frameAt(7200));
    TEST_ASSERT_EQUAL_INT(WAIT3, idlesprite::frameAt(7600 + 800 * 1000));   // taps for ever
    // the tick changes exactly when the frame may: every 400 ms, and never wraps backwards
    TEST_ASSERT_EQUAL_UINT32(0, idlesprite::tickAt(399));
    TEST_ASSERT_EQUAL_UINT32(1, idlesprite::tickAt(400));
    TEST_ASSERT_EQUAL_UINT32(4294967295u / 400u, idlesprite::tickAt(4294967295u));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parses_and_reads_pixels);
    RUN_TEST(test_bad_files_read_as_none);
    RUN_TEST(test_the_games_waiting_timeline);
    return UNITY_END();
}
