#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/car/camclip.h"

void setUp() {}
void tearDown() {}

using camclip::Slot;

static void put16(std::vector<uint8_t>& b, size_t at, uint16_t v) {
    b[at] = (uint8_t)v;
    b[at + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; i++) b[at + i] = (uint8_t)(v >> (8 * i));
}

// A table of `count` slots, all empty; slot(...) fills one.
static std::vector<uint8_t> table(uint16_t count) {
    std::vector<uint8_t> b(camclip::HEAD + camclip::ENTRY * count, 0);
    memcpy(b.data(), "CAMV", 4);
    put16(b, 4, camclip::VERSION);
    put16(b, 6, count);
    return b;
}
static void slot(std::vector<uint8_t>& b, int i, const std::vector<int16_t>& pcm) {
    const size_t off = b.size();
    for (int16_t s : pcm) { b.push_back((uint8_t)s); b.push_back((uint8_t)((uint16_t)s >> 8)); }
    put32(b, camclip::HEAD + camclip::ENTRY * i, (uint32_t)off);
    put32(b, camclip::HEAD + camclip::ENTRY * i + 4, (uint32_t)(pcm.size() * 2));
}

void test_reads_slots() {
    auto b = table(camclip::COUNT);
    slot(b, (int)Slot::Pon, {1, 2, 3});
    slot(b, (int)Slot::L80, {-7});
    camclip::Table t;
    TEST_ASSERT_TRUE(t.load(b.data(), b.size()));
    TEST_ASSERT_EQUAL_UINT(3, t.get(Slot::Pon).samples);
    TEST_ASSERT_EQUAL_UINT(1, t.get(Slot::L80).samples);
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::Torishimari).samples);    // empty slot
    TEST_ASSERT_NULL(t.get(Slot::Torishimari).pcm);
    TEST_ASSERT_EQUAL_HEX16((1u << (int)Slot::Pon) | (1u << (int)Slot::L80), t.present());
}

void test_bad_header_is_no_table() {
    auto b = table(camclip::COUNT);
    slot(b, 0, {1});
    camclip::Table t;
    auto bad = b;
    bad[0] = 'X';                                   // magic
    TEST_ASSERT_FALSE(t.load(bad.data(), bad.size()));
    TEST_ASSERT_EQUAL_HEX16(0, t.present());
    bad = b;
    put16(bad, 4, camclip::VERSION + 1);            // a format this firmware doesn't read
    TEST_ASSERT_FALSE(t.load(bad.data(), bad.size()));
    TEST_ASSERT_FALSE(t.load(b.data(), camclip::HEAD - 1));               // shorter than the header
    TEST_ASSERT_FALSE(t.load(b.data(), camclip::HEAD + camclip::ENTRY));  // the slot table cut short
    TEST_ASSERT_FALSE(t.load(nullptr, 0));
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::Pon).samples);
}

void test_slot_out_of_range_reads_empty() {
    auto b = table(camclip::COUNT);
    slot(b, (int)Slot::Pon, {1, 2});
    slot(b, (int)Slot::GpsOk, {5, 6});
    put32(b, camclip::HEAD + camclip::ENTRY * (int)Slot::GpsOk + 4, 400);   // runs past the end
    camclip::Table t;
    TEST_ASSERT_TRUE(t.load(b.data(), b.size()));
    TEST_ASSERT_EQUAL_UINT(2, t.get(Slot::Pon).samples);
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::GpsOk).samples);
    // An offset inside the header, or an odd size, is no clip either.
    put32(b, camclip::HEAD + camclip::ENTRY * (int)Slot::GpsOk, 4);
    put32(b, camclip::HEAD + camclip::ENTRY * (int)Slot::GpsOk + 4, 2);
    put32(b, camclip::HEAD + camclip::ENTRY * (int)Slot::Pon + 4, 3);
    TEST_ASSERT_TRUE(t.load(b.data(), b.size()));
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::GpsOk).samples);
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::Pon).samples);
}

void test_fewer_or_more_slots() {
    auto few = table(2);                            // an older, shorter table: the rest are empty
    slot(few, 1, {9});
    camclip::Table t;
    TEST_ASSERT_TRUE(t.load(few.data(), few.size()));
    TEST_ASSERT_EQUAL_UINT(1, t.get(Slot::GpsFirst).samples);
    TEST_ASSERT_EQUAL_UINT(0, t.get(Slot::L80).samples);
    auto more = table(camclip::COUNT + 2);          // a newer one: the slots this firmware knows
    slot(more, (int)Slot::L80, {1, 1});
    slot(more, camclip::COUNT + 1, {2});
    TEST_ASSERT_TRUE(t.load(more.data(), more.size()));
    TEST_ASSERT_EQUAL_UINT(2, t.get(Slot::L80).samples);
}

void test_limit_slot() {
    TEST_ASSERT_EQUAL_INT((int)Slot::L20, (int)camclip::limitSlot(20));
    TEST_ASSERT_EQUAL_INT((int)Slot::L60, (int)camclip::limitSlot(60));
    TEST_ASSERT_EQUAL_INT((int)Slot::L80, (int)camclip::limitSlot(80));
    TEST_ASSERT_EQUAL_INT((int)Slot::COUNT, (int)camclip::limitSlot(70));   // no line
    TEST_ASSERT_EQUAL_INT((int)Slot::COUNT, (int)camclip::limitSlot(0));
    TEST_ASSERT_EQUAL_INT((int)Slot::COUNT, (int)camclip::limitSlot(100));
}

void test_reader_at_clip_rate_copies() {
    const int16_t pcm[] = {100, -200, 300, 32767, -32768};
    camclip::Reader r({(const uint8_t*)pcm, 5}, camclip::RATE, 100);
    int16_t out[8];
    TEST_ASSERT_EQUAL_UINT(3, r.fill(out, 3));
    TEST_ASSERT_EQUAL_INT16(100, out[0]);
    TEST_ASSERT_EQUAL_INT16(-200, out[1]);
    TEST_ASSERT_EQUAL_UINT(2, r.fill(out, 8));
    TEST_ASSERT_EQUAL_INT16(32767, out[0]);
    TEST_ASSERT_EQUAL_INT16(-32768, out[1]);
    TEST_ASSERT_EQUAL_UINT(0, r.fill(out, 8));
    TEST_ASSERT_TRUE(r.done());
}

void test_reader_upsamples_by_interpolation() {
    const int16_t pcm[] = {0, 300, 600};
    camclip::Reader r({(const uint8_t*)pcm, 3}, 48000, 100);
    int16_t out[16];
    TEST_ASSERT_EQUAL_UINT(9, r.fill(out, 16));     // three output samples per input one
    TEST_ASSERT_EQUAL_INT16(0, out[0]);
    TEST_ASSERT_EQUAL_INT16(100, out[1]);
    TEST_ASSERT_EQUAL_INT16(200, out[2]);
    TEST_ASSERT_EQUAL_INT16(300, out[3]);
    TEST_ASSERT_EQUAL_INT16(600, out[6]);
    TEST_ASSERT_EQUAL_INT16(600, out[8]);           // the last sample held, never read past the end
}

void test_reader_volume_and_unaligned() {
    uint8_t raw[1 + 4] = {0};
    const int16_t pcm[] = {1000, -1000};
    memcpy(raw + 1, pcm, 4);                        // the media pack gives no alignment
    camclip::Reader r({raw + 1, 2}, camclip::RATE, 50);
    int16_t out[2];
    TEST_ASSERT_EQUAL_UINT(2, r.fill(out, 2));
    TEST_ASSERT_EQUAL_INT16(500, out[0]);
    TEST_ASSERT_EQUAL_INT16(-500, out[1]);
}

void test_reader_length_ms() {
    TEST_ASSERT_EQUAL_UINT32(1000, camclip::lengthMs({nullptr, 16000}));
    TEST_ASSERT_EQUAL_UINT32(0, camclip::lengthMs({nullptr, 0}));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reads_slots);
    RUN_TEST(test_bad_header_is_no_table);
    RUN_TEST(test_slot_out_of_range_reads_empty);
    RUN_TEST(test_fewer_or_more_slots);
    RUN_TEST(test_limit_slot);
    RUN_TEST(test_reader_at_clip_rate_copies);
    RUN_TEST(test_reader_upsamples_by_interpolation);
    RUN_TEST(test_reader_volume_and_unaligned);
    RUN_TEST(test_reader_length_ms);
    return UNITY_END();
}
