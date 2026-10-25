#include <unity.h>
#include <cstring>
#include "../../src/util/musicwire.h"

void setUp() {}
void tearDown() {}

using musicwire::FeatPkt;
using musicwire::PalPkt;

static void test_feat_round_trip() {
    FeatPkt in{0xBEEF, {true, 200, 128, 7, true}};
    uint8_t buf[32];
    TEST_ASSERT_EQUAL(9, musicwire::encode(in, buf, sizeof buf));
    const uint8_t want[9] = {'M', 'F', 1, 0xEF, 0xBE, 0x03, 200, 128, 7};   // seq LE, flags beat|present
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, buf, 9);
    FeatPkt out{};
    TEST_ASSERT_TRUE(musicwire::decode(buf, 9, &out));
    TEST_ASSERT_EQUAL(0xBEEF, out.seq);
    TEST_ASSERT_TRUE(out.f.beat);
    TEST_ASSERT_TRUE(out.f.present);
    TEST_ASSERT_EQUAL(200, out.f.strength);
    TEST_ASSERT_EQUAL(128, out.f.energy);
    TEST_ASSERT_EQUAL(7, out.f.brightness);

    FeatPkt quiet{1, {false, 0, 3, 4, false}};
    TEST_ASSERT_EQUAL(9, musicwire::encode(quiet, buf, sizeof buf));
    TEST_ASSERT_EQUAL_HEX8(0, buf[5]);
    TEST_ASSERT_TRUE(musicwire::decode(buf, 9, &out));
    TEST_ASSERT_FALSE(out.f.beat);
    TEST_ASSERT_FALSE(out.f.present);
    TEST_ASSERT_EQUAL(1, out.seq);
}

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
    TEST_ASSERT_EQUAL(0, musicwire::encode(FeatPkt{}, buf, 8));
    TEST_ASSERT_EQUAL(0, musicwire::encode(PalPkt{}, buf, 17));
}

static void test_bad_packets_rejected() {
    uint8_t f[9], p[18];
    musicwire::encode(FeatPkt{5, {true, 1, 2, 3, true}}, f, sizeof f);
    musicwire::encode(PalPkt{9, {}}, p, sizeof p);
    FeatPkt fo{};
    PalPkt po{};
    // Short.
    TEST_ASSERT_FALSE(musicwire::decode(f, 8, &fo));
    TEST_ASSERT_FALSE(musicwire::decode(p, 17, &po));
    TEST_ASSERT_FALSE(musicwire::decode(f, 0, &fo));
    // Long: a datagram is one packet, extra bytes mean another format.
    uint8_t longer[19] = {};
    memcpy(longer, p, 18);
    TEST_ASSERT_FALSE(musicwire::decode(longer, 19, &po));
    // The other packet's magic.
    TEST_ASSERT_FALSE(musicwire::decode(p, 18, &fo));
    TEST_ASSERT_FALSE(musicwire::decode(f, 9, &po));
    // Wrong magic.
    uint8_t bad[18];
    memcpy(bad, f, 9);
    bad[0] = 'X';
    TEST_ASSERT_FALSE(musicwire::decode(bad, 9, &fo));
    memcpy(bad, f, 9);
    bad[1] = 'P';
    TEST_ASSERT_FALSE(musicwire::decode(bad, 9, &fo));
    // Version 2.
    memcpy(bad, f, 9);
    bad[2] = 2;
    TEST_ASSERT_FALSE(musicwire::decode(bad, 9, &fo));
    memcpy(bad, p, 18);
    bad[2] = 2;
    TEST_ASSERT_FALSE(musicwire::decode(bad, 18, &po));
    // Null input.
    TEST_ASSERT_FALSE(musicwire::decode(nullptr, 9, &fo));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_feat_round_trip);
    RUN_TEST(test_pal_round_trip);
    RUN_TEST(test_encode_needs_room);
    RUN_TEST(test_bad_packets_rejected);
    return UNITY_END();
}
