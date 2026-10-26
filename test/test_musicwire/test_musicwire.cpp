#include <unity.h>
#include <cstring>
#include "../../src/util/musicwire.h"

void setUp() {}
void tearDown() {}

using musicwire::PalPkt;

static void test_pal_round_trip() {
    PalPkt in{0x01020304u, {{{255, 0, 10}, {1, 2, 3}, {40, 50, 60}}, 70, 45}};
    uint8_t buf[32];
    TEST_ASSERT_EQUAL(18, musicwire::encode(in, buf, sizeof buf));
    const uint8_t want[18] = {'M', 'P', 1, 4, 3, 2, 1, 255, 0, 10, 1, 2, 3, 40, 50, 60, 70, 45};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, buf, 18);
    PalPkt out{};
    TEST_ASSERT_TRUE(musicwire::decode(buf, 18, &out));
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, out.songId);
    TEST_ASSERT_EQUAL(255, out.p.c[0].r);
    TEST_ASSERT_EQUAL(10, out.p.c[0].b);
    TEST_ASSERT_EQUAL(2, out.p.c[1].g);
    TEST_ASSERT_EQUAL(60, out.p.c[2].b);
    TEST_ASSERT_EQUAL(70, out.p.cap);
    TEST_ASSERT_EQUAL(45, out.p.depth);
}

static void test_encode_needs_room() {
    uint8_t buf[17];
    TEST_ASSERT_EQUAL(0, musicwire::encode(PalPkt{}, buf, 17));
}

static void test_bad_packets_rejected() {
    uint8_t p[18];
    musicwire::encode(PalPkt{9, {}}, p, sizeof p);
    PalPkt po{};
    // Short.
    TEST_ASSERT_FALSE(musicwire::decode(p, 17, &po));
    TEST_ASSERT_FALSE(musicwire::decode(p, 0, &po));
    // Long: a datagram is one packet, extra bytes mean another format.
    uint8_t longer[19] = {};
    memcpy(longer, p, 18);
    TEST_ASSERT_FALSE(musicwire::decode(longer, 19, &po));
    // The old feature packet's magic, at a palette's length.
    uint8_t bad[18];
    memcpy(bad, p, 18);
    bad[1] = 'F';
    TEST_ASSERT_FALSE(musicwire::decode(bad, 18, &po));
    // Wrong magic.
    memcpy(bad, p, 18);
    bad[0] = 'X';
    TEST_ASSERT_FALSE(musicwire::decode(bad, 18, &po));
    // Version 2.
    memcpy(bad, p, 18);
    bad[2] = 2;
    TEST_ASSERT_FALSE(musicwire::decode(bad, 18, &po));
    // Null input.
    TEST_ASSERT_FALSE(musicwire::decode(nullptr, 18, &po));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pal_round_trip);
    RUN_TEST(test_encode_needs_room);
    RUN_TEST(test_bad_packets_rejected);
    return UNITY_END();
}
