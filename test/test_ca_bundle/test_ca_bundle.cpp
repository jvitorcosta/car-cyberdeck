#include <unity.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/net/ca_bundle.inc"

// The ESP32's bundle checker (WiFiClientSecure esp_crt_bundle.c) trusts the bytes blindly:
// u16 count, then per root u16 name length, u16 key length, subject DER, public key DER, and
// it finds a root by binary search on the subject. A malformed or unsorted bundle doesn't
// fail to build - every Spotify handshake fails on the device instead.

void setUp() {}
void tearDown() {}

struct Root { std::string subject; size_t keyLen; };

static std::vector<Root> roots() {
    std::vector<Root> out;
    const size_t n = (CA_BUNDLE[0] << 8) | CA_BUNDLE[1];
    size_t p = 2;
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(p + 4 <= sizeof CA_BUNDLE, "entry header past the end");
        const size_t name = (CA_BUNDLE[p] << 8) | CA_BUNDLE[p + 1];
        const size_t key = (CA_BUNDLE[p + 2] << 8) | CA_BUNDLE[p + 3];
        TEST_ASSERT_TRUE_MESSAGE(p + 4 + name + key <= sizeof CA_BUNDLE, "entry past the end");
        out.push_back({std::string((const char*)CA_BUNDLE + p + 4, name), key});
        p += 4 + name + key;
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof CA_BUNDLE, p, "bytes left after the last root");
    return out;
}

void test_bundle_walks_to_exactly_its_end() {
    auto r = roots();
    TEST_ASSERT_TRUE(r.size() >= 2);
    for (auto& e : r) TEST_ASSERT_TRUE(e.subject.size() > 0 && e.keyLen > 0);
}
void test_roots_are_sorted_by_subject_for_the_binary_search() {
    auto r = roots();
    for (size_t i = 1; i < r.size(); ++i) TEST_ASSERT_TRUE_MESSAGE(r[i - 1].subject < r[i].subject, r[i].subject.c_str());
}
void test_both_spotify_hosts_chain_to_a_root_in_the_bundle() {
    // api.spotify.com -> DigiCert Global Root G2; accounts.spotify.com -> Starfield Root G2 (2026-10).
    const char* need[] = {"DigiCert Global Root G2", "Starfield Root Certificate Authority - G2"};
    auto r = roots();
    for (const char* cn : need) {
        bool found = false;
        for (auto& e : r) found |= e.subject.find(cn) != std::string::npos;
        TEST_ASSERT_TRUE_MESSAGE(found, cn);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_bundle_walks_to_exactly_its_end);
    RUN_TEST(test_roots_are_sorted_by_subject_for_the_binary_search);
    RUN_TEST(test_both_spotify_hosts_chain_to_a_root_in_the_bundle);
    return UNITY_END();
}
