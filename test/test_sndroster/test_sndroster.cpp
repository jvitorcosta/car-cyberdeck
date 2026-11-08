#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/util/black/sndroster.h"

using namespace sndroster;

void setUp() {}
void tearDown() {}

static void put16(std::vector<uint8_t>& b, size_t at, uint16_t v) { b[at] = v & 0xFF; b[at + 1] = v >> 8; }
static void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int k = 0; k < 4; ++k) b[at + k] = (uint8_t)(v >> (8 * k));
}

// A roster as tools/gen_rosters.py writes it; clip i's bytes are i+1 repeated.
static std::vector<uint8_t> make(const std::vector<std::string>& names, const std::vector<size_t>& sizes) {
    const size_t n = names.size();
    std::vector<uint8_t> b(HEAD + ENTRY * n, 0);
    memcpy(b.data(), "SNDR", 4);
    put16(b, 4, VERSION);
    put16(b, 6, (uint16_t)n);
    for (size_t i = 0; i < n; ++i) {
        if (b.size() % 2) b.push_back(0);
        const size_t e = HEAD + ENTRY * i;
        put32(b, e, (uint32_t)b.size());
        put32(b, e + 4, (uint32_t)sizes[i]);
        memcpy(&b[e + 8], names[i].c_str(), names[i].size());
        b.insert(b.end(), sizes[i], (uint8_t)(i + 1));
    }
    return b;
}

void test_reads_clips() {
    const std::vector<uint8_t> b = make({"bem", "okaeri"}, {4, 6});
    TEST_ASSERT_EQUAL_INT(2, count(b.data(), b.size()));
    Clip c;
    TEST_ASSERT_TRUE(at(b.data(), b.size(), 1, &c));
    TEST_ASSERT_EQUAL_UINT(6, c.bytes);
    TEST_ASSERT_EQUAL_STRING("okaeri", c.name);
    for (size_t k = 0; k < 6; ++k) TEST_ASSERT_EQUAL_HEX8(2, c.data[k]);
    TEST_ASSERT_TRUE(at(b.data(), b.size(), 0, &c));
    TEST_ASSERT_EQUAL_STRING("bem", c.name);
    TEST_ASSERT_EQUAL_UINT(4, c.bytes);
}

void test_empty_and_garbage() {
    TEST_ASSERT_EQUAL_INT(0, count(nullptr, 0));
    const uint8_t standIn[2] = {0, 0};                  // tools/greeting_clip.py's stand-in
    TEST_ASSERT_EQUAL_INT(0, count(standIn, sizeof standIn));
    std::vector<uint8_t> b = make({"a"}, {2});
    b[3] = 'X';
    TEST_ASSERT_EQUAL_INT(0, count(b.data(), b.size()));
    b = make({"a"}, {2});
    put16(b, 4, 2);
    TEST_ASSERT_EQUAL_INT(0, count(b.data(), b.size()));
    std::vector<std::string> names(MAX + 1, "x");
    std::vector<size_t> sizes(MAX + 1, 2);
    b = make(names, sizes);
    TEST_ASSERT_EQUAL_INT(0, count(b.data(), b.size()));
    names.pop_back(); sizes.pop_back();
    b = make(names, sizes);
    TEST_ASSERT_EQUAL_INT(MAX, count(b.data(), b.size()));
}

void test_truncated() {
    std::vector<uint8_t> b = make({"a", "b"}, {4, 4});
    TEST_ASSERT_EQUAL_INT(0, count(b.data(), HEAD + ENTRY));   // the table runs past len
    put32(b, HEAD + ENTRY + 4, 100);                            // clip 1 runs past the end
    Clip c;
    TEST_ASSERT_EQUAL_INT(2, count(b.data(), b.size()));
    TEST_ASSERT_FALSE(at(b.data(), b.size(), 1, &c));
    TEST_ASSERT_TRUE(at(b.data(), b.size(), 0, &c));
    TEST_ASSERT_FALSE(at(b.data(), b.size(), -1, &c));
    TEST_ASSERT_FALSE(at(b.data(), b.size(), 2, &c));
    put32(b, HEAD + 4, 0xFFFFFFF0u);                            // offset + size wraps around
    TEST_ASSERT_FALSE(at(b.data(), b.size(), 0, &c));
}

void test_name_needs_nul() {
    std::vector<uint8_t> b = make({"a"}, {2});
    memset(&b[HEAD + 8], 'z', NAME);
    Clip c;
    TEST_ASSERT_FALSE(at(b.data(), b.size(), 0, &c));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_clips);
    RUN_TEST(test_empty_and_garbage);
    RUN_TEST(test_truncated);
    RUN_TEST(test_name_needs_nul);
    return UNITY_END();
}
