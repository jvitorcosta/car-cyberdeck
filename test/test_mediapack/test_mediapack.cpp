#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/util/mediapack.h"

void setUp() {}
void tearDown() {}

static void put16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

// Two files the way tools/pack_media.py lays them out: "a.pcm" = "hello", "b.bin" = 4 bytes.
static uint8_t s_pack[16 + 2 * 32 + 8 + 4];
static size_t pack() {
    memset(s_pack, 0, sizeof s_pack);
    memcpy(s_pack, "CDMEDIA1", 8);
    put16(s_pack + 8, 1);
    put16(s_pack + 10, 2);
    put32(s_pack + 12, sizeof s_pack);
    uint8_t* e = s_pack + 16;
    strcpy((char*)e, "a.pcm");
    put32(e + 20, 80); put32(e + 24, 5); put32(e + 28, 0x3610A686);   // zlib.crc32(b"hello")
    e += 32;
    strcpy((char*)e, "b.bin");
    put32(e + 20, 88); put32(e + 24, 4); put32(e + 28, 0);
    memcpy(s_pack + 80, "hello", 5);
    memcpy(s_pack + 88, "\x01\x02\x03\x04", 4);
    return sizeof s_pack;
}

void test_finds_files() {
    const size_t len = pack();
    TEST_ASSERT_EQUAL_UINT32(len, mediapack::total(s_pack, mediapack::HEAD));
    TEST_ASSERT_EQUAL_INT(2, mediapack::count(s_pack, len));
    const int i = mediapack::find(s_pack, len, "a.pcm");
    TEST_ASSERT_EQUAL_INT(0, i);
    mediapack::File f{};
    TEST_ASSERT_TRUE(mediapack::at(s_pack, len, i, &f));
    TEST_ASSERT_EQUAL_UINT(5, f.size);
    TEST_ASSERT_EQUAL_MEMORY("hello", f.data, 5);
    TEST_ASSERT_EQUAL_HEX32(f.crc, mediapack::crc32(f.data, f.size));
    TEST_ASSERT_EQUAL_INT(1, mediapack::find(s_pack, len, "b.bin"));
    TEST_ASSERT_EQUAL_INT(-1, mediapack::find(s_pack, len, "c.bin"));
    TEST_ASSERT_EQUAL_INT(-1, mediapack::find(s_pack, len, "a.pc"));    // whole names only
    TEST_ASSERT_EQUAL_INT(-1, mediapack::find(s_pack, len, "a_name_far_too_long.bin"));
}

void test_bad_packs_read_as_empty() {
    size_t len = pack();
    TEST_ASSERT_EQUAL_INT(0, mediapack::count(s_pack, len - 1));        // cut off
    s_pack[0] = 'X';
    TEST_ASSERT_EQUAL_UINT32(0, mediapack::total(s_pack, len));         // wrong magic
    len = pack();
    put16(s_pack + 8, 2);
    TEST_ASSERT_EQUAL_INT(0, mediapack::count(s_pack, len));            // a newer format
    len = pack();
    put32(s_pack + 12, 40);
    TEST_ASSERT_EQUAL_INT(0, mediapack::count(s_pack, len));            // table bigger than the pack
    uint8_t erased[16];
    memset(erased, 0xFF, sizeof erased);                                // a blank partition
    TEST_ASSERT_EQUAL_UINT32(0, mediapack::total(erased, sizeof erased));
    TEST_ASSERT_EQUAL_UINT32(0, mediapack::total(nullptr, 0));
}

void test_entry_outside_the_pack_is_refused() {
    const size_t len = pack();
    put32(s_pack + 16 + 32 + 24, 5);                                    // b.bin runs past the end
    mediapack::File f{};
    TEST_ASSERT_FALSE(mediapack::at(s_pack, len, 1, &f));
    put32(s_pack + 16 + 32 + 20, 0xFFFFFFF0u);                          // offset far outside
    put32(s_pack + 16 + 32 + 24, 0x20);
    TEST_ASSERT_FALSE(mediapack::at(s_pack, len, 1, &f));
    TEST_ASSERT_FALSE(mediapack::at(s_pack, len, 2, &f));               // no such entry
}

void test_crc32_matches_zlib() {
    TEST_ASSERT_EQUAL_HEX32(0x00000000, mediapack::crc32((const uint8_t*)"", 0));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, mediapack::crc32((const uint8_t*)"123456789", 9));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_finds_files);
    RUN_TEST(test_bad_packs_read_as_empty);
    RUN_TEST(test_entry_outside_the_pack_is_refused);
    RUN_TEST(test_crc32_matches_zlib);
    return UNITY_END();
}
