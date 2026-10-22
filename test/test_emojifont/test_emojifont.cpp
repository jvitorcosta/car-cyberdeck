#include <unity.h>
#include <vector>
#include "../../src/ui/emojifont.h"

void setUp() {}
void tearDown() {}

// Synthetic blob: keys 0x2764 and 0x1F525. Glyph g: palette[i] = 0x1000 * g + i, pixels 0x12 0x34...
static std::vector<uint8_t> makeBlob() {
    std::vector<uint8_t> b = {'E', 'M', 'J', '1', 2, 0};
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    u32(0x2764); u32(0x1F525);
    for (int g = 0; g < 2; ++g) {
        for (int i = 0; i < 16; ++i) {
            uint16_t c = (uint16_t)(0x1000 * (g + 1) + i);
            b.push_back(c & 0xFF); b.push_back(c >> 8);
        }
        for (int i = 0; i < 128; ++i) b.push_back((uint8_t)(i == 0 ? 0x12 : g == 0 ? 0x00 : 0x34));
    }
    return b;
}

void test_open_and_lookup() {
    std::vector<uint8_t> b = makeBlob();
    emojifont::Font f;
    TEST_ASSERT_TRUE(f.open(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(2, f.count());
    const uint8_t* g = f.glyph(0x2764);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_PTR(b.data() + 6 + 8, g);
    TEST_ASSERT_EQUAL_HEX16(0x1000, emojifont::paletteAt(g, 0));
    TEST_ASSERT_EQUAL_HEX16(0x100F, emojifont::paletteAt(g, 15));
    TEST_ASSERT_EQUAL_INT(1, emojifont::pixelAt(g, 0, 0));    // high nibble = left pixel
    TEST_ASSERT_EQUAL_INT(2, emojifont::pixelAt(g, 1, 0));
    TEST_ASSERT_EQUAL_INT(0, emojifont::pixelAt(g, 2, 0));
    g = f.glyph(0x1F525);
    TEST_ASSERT_EQUAL_PTR(b.data() + 6 + 8 + 160, g);
    TEST_ASSERT_EQUAL_HEX16(0x2003, emojifont::paletteAt(g, 3));
    TEST_ASSERT_EQUAL_INT(3, emojifont::pixelAt(g, 14, 15));   // last byte 0x34
    TEST_ASSERT_EQUAL_INT(4, emojifont::pixelAt(g, 15, 15));
}

void test_missing_keys() {
    std::vector<uint8_t> b = makeBlob();
    emojifont::Font f;
    f.open(b.data(), b.size());
    TEST_ASSERT_NULL(f.glyph(0x41));          // below the first key
    TEST_ASSERT_NULL(f.glyph(0x3000));        // between keys
    TEST_ASSERT_NULL(f.glyph(0x110000));      // above the last key
}

void test_rejects_bad_blobs() {
    std::vector<uint8_t> b = makeBlob();
    emojifont::Font f;
    std::vector<uint8_t> bad = b; bad[0] = 'X';
    TEST_ASSERT_FALSE(f.open(bad.data(), bad.size()));                 // magic
    TEST_ASSERT_FALSE(f.open(b.data(), b.size() - 1));                 // truncated glyphs
    TEST_ASSERT_FALSE(f.open(b.data(), 5));                            // truncated header
    TEST_ASSERT_FALSE(f.open(b.data(), 10));                           // truncated keys
    std::vector<uint8_t> unsorted = b;
    unsorted[10] = 0x64; unsorted[11] = 0x27; unsorted[12] = 0; unsorted[13] = 0;   // key 1 = key 0
    TEST_ASSERT_FALSE(f.open(unsorted.data(), unsorted.size()));
    std::vector<uint8_t> huge = b; huge[4] = 0xFF; huge[5] = 0xFF;    // count 65535
    TEST_ASSERT_FALSE(f.open(huge.data(), huge.size()));
    TEST_ASSERT_FALSE(f.open(nullptr, 0));
    TEST_ASSERT_EQUAL_INT(0, f.count());                               // closed font: no glyphs
    TEST_ASSERT_NULL(f.glyph(0x2764));
}

// EMJ2: keys 0x1F408, SEQ_BASE, SEQ_BASE + 1; sequences [1F3F3 1F308] and [1F408 2B1B] (sorted).
static std::vector<uint8_t> makeBlob2() {
    std::vector<uint8_t> b = {'E', 'M', 'J', '2', 3, 0, 2, 0};
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    u32(0x1F408); u32(emojifont::SEQ_BASE); u32(emojifont::SEQ_BASE + 1);
    u32(0x1F3F3); u32(0x1F308); u32(0); u32(0);
    u32(0x1F408); u32(0x2B1B); u32(0); u32(0);
    for (int g = 0; g < 3; ++g)
        for (size_t i = 0; i < emojifont::GLYPH_BYTES; ++i) b.push_back((uint8_t)(0x10 * g));
    return b;
}

void test_sequences() {
    std::vector<uint8_t> b = makeBlob2();
    emojifont::Font f;
    TEST_ASSERT_TRUE(f.open(b.data(), b.size()));
    TEST_ASSERT_EQUAL_INT(3, f.count());
    const uint32_t cat[] = {0x1F408, 0x2B1B}, flag[] = {0x1F3F3, 0x1F308};
    TEST_ASSERT_EQUAL_HEX32(emojifont::SEQ_BASE + 1, f.sequence(cat, 2));
    TEST_ASSERT_EQUAL_HEX32(emojifont::SEQ_BASE, f.sequence(flag, 2));
    TEST_ASSERT_EQUAL_HEX32(0, f.sequence(cat, 1));                   // a prefix is no match
    const uint32_t longer[] = {0x1F408, 0x2B1B, 0x1F308};
    TEST_ASSERT_EQUAL_HEX32(0, f.sequence(longer, 3));
    TEST_ASSERT_EQUAL_HEX32(0, f.sequence(longer, 5));                // too long for the table
    const uint8_t* g = f.glyph(emojifont::SEQ_BASE + 1);
    TEST_ASSERT_EQUAL_PTR(b.data() + 8 + 12 + 32 + 2 * emojifont::GLYPH_BYTES, g);
    TEST_ASSERT_EQUAL_HEX8(0x20, g[0]);
}

void test_rejects_bad_sequence_tables() {
    emojifont::Font f;
    std::vector<uint8_t> b = makeBlob2();
    TEST_ASSERT_FALSE(f.open(b.data(), b.size() - 1));                 // truncated glyphs
    std::vector<uint8_t> unsorted = b;                                  // swap the two sequences
    for (int i = 0; i < 16; ++i) std::swap(unsorted[20 + i], unsorted[36 + i]);
    TEST_ASSERT_FALSE(f.open(unsorted.data(), unsorted.size()));
    // an EMJ1 blob still opens, without sequences
    std::vector<uint8_t> b1 = makeBlob();
    TEST_ASSERT_TRUE(f.open(b1.data(), b1.size()));
    const uint32_t cat[] = {0x1F408, 0x2B1B};
    TEST_ASSERT_EQUAL_HEX32(0, f.sequence(cat, 2));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_open_and_lookup);
    RUN_TEST(test_missing_keys);
    RUN_TEST(test_rejects_bad_blobs);
    RUN_TEST(test_sequences);
    RUN_TEST(test_rejects_bad_sequence_tables);
    return UNITY_END();
}
