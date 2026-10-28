#include <unity.h>
#include <cstring>
#include "../../src/util/wire/musicwire.h"

void setUp() {}
void tearDown() {}

using musicwire::PalPkt;
using musicwire::Seq;
static const char* KEY = "test-key-123";

static void test_pal_round_trip() {
    PalPkt in{0x01020304u, {{{255, 0, 10}, {1, 2, 3}, {40, 50, 60}}, 70, 45}};
    uint8_t buf[40];
    TEST_ASSERT_EQUAL(34, musicwire::encode(in, Seq{0xA1B2C3D4u, 7}, KEY, buf, sizeof buf));
    // the tag: Python's hmac.new(key, the 26 bytes, sha256), first 8 bytes
    const uint8_t want[34] = {'M', 'P', 2, 0xD4, 0xC3, 0xB2, 0xA1, 7, 0, 0, 0, 4, 3, 2, 1, 255, 0, 10, 1, 2, 3,
                              40, 50, 60, 70, 45, 0x31, 0x99, 0x14, 0x18, 0x65, 0x16, 0x9b, 0x8b};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, buf, 34);
    PalPkt out{};
    Seq s{};
    TEST_ASSERT_TRUE(musicwire::decode(buf, 34, KEY, &out, &s));
    TEST_ASSERT_EQUAL_HEX32(0xA1B2C3D4u, s.boot);
    TEST_ASSERT_EQUAL_UINT32(7, s.counter);
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, out.songId);
    TEST_ASSERT_EQUAL(255, out.p.c[0].r);
    TEST_ASSERT_EQUAL(10, out.p.c[0].b);
    TEST_ASSERT_EQUAL(2, out.p.c[1].g);
    TEST_ASSERT_EQUAL(60, out.p.c[2].b);
    TEST_ASSERT_EQUAL(70, out.p.cap);
    TEST_ASSERT_EQUAL(45, out.p.depth);
}

static void test_encode_needs_room_and_a_key() {
    uint8_t buf[40];
    TEST_ASSERT_EQUAL(0, musicwire::encode(PalPkt{}, Seq{1, 1}, KEY, buf, 33));
    TEST_ASSERT_EQUAL(0, musicwire::encode(PalPkt{}, Seq{1, 1}, "", buf, sizeof buf));
    TEST_ASSERT_EQUAL(0, musicwire::encode(PalPkt{}, Seq{1, 1}, nullptr, buf, sizeof buf));
}

// Any bit changed past the header, or another key: the tag doesn't match.
static void test_a_bad_tag_is_dropped() {
    uint8_t p[34];
    musicwire::encode(PalPkt{9, {{{255, 0, 0}, {}, {}}, 100, 60}}, Seq{5, 6}, KEY, p, sizeof p);
    PalPkt po{};
    Seq so{};
    for (size_t i = 3; i < sizeof p; ++i) {
        uint8_t bad[34];
        memcpy(bad, p, sizeof p);
        bad[i] ^= 0x01;
        TEST_ASSERT_FALSE_MESSAGE(musicwire::decode(bad, 34, KEY, &po, &so), "a flipped bit got through");
    }
    TEST_ASSERT_FALSE(musicwire::decode(p, 34, "another-key", &po, &so));
    TEST_ASSERT_FALSE(musicwire::decode(p, 34, "", &po, &so));
    TEST_ASSERT_TRUE(musicwire::decode(p, 34, KEY, &po, &so));
}

static void test_bad_packets_rejected() {
    uint8_t p[34];
    musicwire::encode(PalPkt{9, {}}, Seq{1, 2}, KEY, p, sizeof p);
    PalPkt po{};
    Seq so{};
    TEST_ASSERT_FALSE(musicwire::decode(p, 33, KEY, &po, &so));   // short
    TEST_ASSERT_FALSE(musicwire::decode(p, 0, KEY, &po, &so));
    uint8_t longer[35] = {};   // long: a datagram is one packet, extra bytes mean another format
    memcpy(longer, p, 34);
    TEST_ASSERT_FALSE(musicwire::decode(longer, 35, KEY, &po, &so));
    uint8_t bad[34];
    memcpy(bad, p, 34);
    bad[1] = 'F';   // the old feature packet's magic
    TEST_ASSERT_FALSE(musicwire::decode(bad, 34, KEY, &po, &so));
    memcpy(bad, p, 34);
    bad[0] = 'X';
    TEST_ASSERT_FALSE(musicwire::decode(bad, 34, KEY, &po, &so));
    memcpy(bad, p, 34);
    bad[2] = 3;
    TEST_ASSERT_FALSE(musicwire::decode(bad, 34, KEY, &po, &so));
    TEST_ASSERT_FALSE(musicwire::decode(nullptr, 34, KEY, &po, &so));
}

// Version 1 (18 bytes, unsigned): a black deck from before this, or someone forging one, is refused.
static void test_an_old_version_is_refused() {
    const uint8_t v1[18] = {'M', 'P', 1, 4, 3, 2, 1, 255, 0, 10, 1, 2, 3, 40, 50, 60, 70, 45};
    PalPkt po{};
    Seq so{};
    TEST_ASSERT_FALSE(musicwire::decode(v1, sizeof v1, KEY, &po, &so));
    uint8_t padded[34] = {};   // at the new length too
    memcpy(padded, v1, sizeof v1);
    TEST_ASSERT_FALSE(musicwire::decode(padded, 34, KEY, &po, &so));
}

static void test_a_replay_is_dropped() {
    musicwire::Fresh f;
    TEST_ASSERT_TRUE(f.accept(Seq{10, 100}));
    TEST_ASSERT_FALSE(f.accept(Seq{10, 100}));   // the same packet again
    TEST_ASSERT_FALSE(f.accept(Seq{10, 99}));    // an older one
    TEST_ASSERT_TRUE(f.accept(Seq{10, 101}));
    TEST_ASSERT_TRUE(f.accept(Seq{10, 150}));    // lost packets in between are fine
    TEST_ASSERT_FALSE(f.accept(Seq{10, 120}));
}

static void test_a_new_boot_starts_over() {
    musicwire::Fresh f;
    TEST_ASSERT_TRUE(f.accept(Seq{10, 5000}));
    TEST_ASSERT_TRUE(f.accept(Seq{11, 3}));   // the black deck rebooted: a new random start
    TEST_ASSERT_FALSE(f.accept(Seq{11, 3}));
    TEST_ASSERT_TRUE(f.accept(Seq{11, 4}));
}

static void test_reset_forgets_the_sender() {
    musicwire::Fresh f;
    TEST_ASSERT_TRUE(f.accept(Seq{10, 100}));
    f.reset();
    TEST_ASSERT_TRUE(f.accept(Seq{10, 100}));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pal_round_trip);
    RUN_TEST(test_encode_needs_room_and_a_key);
    RUN_TEST(test_a_bad_tag_is_dropped);
    RUN_TEST(test_bad_packets_rejected);
    RUN_TEST(test_an_old_version_is_refused);
    RUN_TEST(test_a_replay_is_dropped);
    RUN_TEST(test_a_new_boot_starts_over);
    RUN_TEST(test_reset_forgets_the_sender);
    return UNITY_END();
}
