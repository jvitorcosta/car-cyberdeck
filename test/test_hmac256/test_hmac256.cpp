#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/wire/hmac256.h"

void setUp() {}
void tearDown() {}

// RFC 4231 section 4: the HMAC-SHA-256 test vectors.
static std::string hexOf(const uint8_t* b, size_t n) {
    static const char H[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s += H[b[i] >> 4]; s += H[b[i] & 15]; }
    return s;
}

static std::string macHex(const std::string& key, const std::string& msg) {
    uint8_t out[hmac256::LEN];
    hmac256::mac(reinterpret_cast<const uint8_t*>(key.data()), key.size(),
                 reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), out);
    return hexOf(out, sizeof out);
}

static void test_case_1() {
    TEST_ASSERT_EQUAL_STRING("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
                             macHex(std::string(20, '\x0b'), "Hi There").c_str());
}
static void test_case_2_key_shorter_than_the_output() {
    TEST_ASSERT_EQUAL_STRING("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
                             macHex("Jefe", "what do ya want for nothing?").c_str());
}
static void test_case_3() {
    TEST_ASSERT_EQUAL_STRING("773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
                             macHex(std::string(20, '\xaa'), std::string(50, '\xdd')).c_str());
}
static void test_case_4() {
    std::string key;
    for (int i = 1; i <= 25; ++i) key += (char)i;
    TEST_ASSERT_EQUAL_STRING("82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
                             macHex(key, std::string(50, '\xcd')).c_str());
}
static void test_case_5_truncated_to_128_bits() {
    TEST_ASSERT_EQUAL_STRING("a3b6167473100ee06e0c796c2955552b",
                             macHex(std::string(20, '\x0c'), "Test With Truncation").substr(0, 32).c_str());
}
static void test_case_6_key_longer_than_a_block() {
    TEST_ASSERT_EQUAL_STRING("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
                             macHex(std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First").c_str());
}
static void test_case_7_key_and_data_longer_than_a_block() {
    TEST_ASSERT_EQUAL_STRING("9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
                             macHex(std::string(131, '\xaa'),
                                    "This is a test using a larger than block-size key and a larger than block-size data. "
                                    "The key needs to be hashed before being used by the HMAC algorithm.").c_str());
}

// Messages around SHA-256's padding edge (55, 56 and 64 bytes after the 64-byte inner pad), from Python's hmac.
static void test_padding_edges() {
    TEST_ASSERT_EQUAL_STRING("2cc44ce77061ae61e565e2ba633d3fb0c3f0a40d8183dc37eb47a18abda4baf2",
                             macHex("k", std::string(55, 'a')).c_str());
    TEST_ASSERT_EQUAL_STRING("bc446082d08be8bf111c25f6b4d227bfbcc94721f7388c69fc93a6699de8ab3b",
                             macHex("k", std::string(56, 'a')).c_str());
    TEST_ASSERT_EQUAL_STRING("51dc6773c877547b2f5658b10ed96a5a72709ba469f445538da56af1b8ffb1cd",
                             macHex("k", std::string(64, 'a')).c_str());
}

static void test_self_test_passes() { TEST_ASSERT_TRUE(hmac256::selfTest()); }

static void test_same_compares_every_byte() {
    const uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5}, c[4] = {9, 2, 3, 4};
    TEST_ASSERT_TRUE(hmac256::same(a, a, 4));
    TEST_ASSERT_FALSE(hmac256::same(a, b, 4));
    TEST_ASSERT_FALSE(hmac256::same(a, c, 4));
    TEST_ASSERT_TRUE(hmac256::same(a, b, 3));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_case_1);
    RUN_TEST(test_case_2_key_shorter_than_the_output);
    RUN_TEST(test_case_3);
    RUN_TEST(test_case_4);
    RUN_TEST(test_case_5_truncated_to_128_bits);
    RUN_TEST(test_case_6_key_longer_than_a_block);
    RUN_TEST(test_case_7_key_and_data_longer_than_a_block);
    RUN_TEST(test_padding_edges);
    RUN_TEST(test_self_test_passes);
    RUN_TEST(test_same_compares_every_byte);
    return UNITY_END();
}
