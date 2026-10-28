#include <unity.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../src/util/wire/hmac256.h"
#include "../../src/util/wire/sigreq.h"

void setUp() {}
void tearDown() {}

using sigreq::Param;
using sigreq::Verdict;
static const char* KEY = "test-key-123";
static constexpr int64_t T = 1761600000;
static constexpr int64_t UP = 1000;   // the verifier's uptime in most tests: booted long before

// The value of name= in a query that sign() wrote ("" if absent).
static std::string field(const char* q, const char* name) {
    const std::string s = std::string("&") + q, key = std::string("&") + name + "=";
    const size_t at = s.find(key);
    if (at == std::string::npos) return "";
    const size_t from = at + key.size(), end = s.find('&', from);
    return s.substr(from, end == std::string::npos ? std::string::npos : end - from);
}

struct Signed {
    std::string t, n, sig;
};
static Signed signOf(const char* path, const Param* p, int count, int64_t t, uint64_t nonce) {
    char q[sigreq::AUTH_LEN];
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", path, p, count, t, nonce, q, sizeof q));
    return {field(q, "t"), field(q, "n"), field(q, "sig")};
}
static Verdict verify(sigreq::Verifier& v, const char* path, const Param* p, int count, const Signed& s, int64_t now,
                      const char* method = "GET") {
    return v.check(method, path, p, count, s.t.c_str(), s.n.c_str(), s.sig.c_str(), true, now, UP);
}

// Known answers from Python's hmac (HMAC-SHA256, first 16 bytes): the wire format is pinned, so the
// watch's Monkey C signer can be checked against the same strings.
static void test_known_answers() {
    const Param music[] = {{"on", "1"}};
    char q[sigreq::AUTH_LEN];
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", "/music", music, 1, T, 0x0123456789abcdefULL, q, sizeof q));
    TEST_ASSERT_EQUAL_STRING("t=1761600000&n=0123456789abcdef&sig=a56c573a195e878e743c20d3ae4d8ef6", q);
    const Param say[] = {{"zone", "inside"}, {"text", "light blue"}};   // signed sorted: text before zone
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", "/say", say, 2, T, 0x0123456789abcdefULL, q, sizeof q));
    TEST_ASSERT_EQUAL_STRING("t=1761600000&n=0123456789abcdef&sig=d19dcaa52a222e8b367573882f0c4aa5", q);
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", "/state", nullptr, 0, T, 0xff, q, sizeof q));
    TEST_ASSERT_EQUAL_STRING("t=1761600000&n=00000000000000ff&sig=3b9b87c49ef5a6f7874aa0cd3248882f", q);
}

static void test_a_signed_request_is_accepted_once() {
    sigreq::Verifier v(KEY);
    const Param p[] = {{"on", "1"}};
    const Signed s = signOf("/music", p, 1, T, 42);
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/music", p, 1, s, T));
    TEST_ASSERT_EQUAL(Verdict::Replay, verify(v, "/music", p, 1, s, T + 1));
}

static void test_param_order_and_reserved_names_dont_matter() {
    sigreq::Verifier v(KEY);
    const Param sent[] = {{"text", "red"}, {"zone", "inside"}};
    const Signed s = signOf("/say", sent, 2, T, 1);
    // the server sees them in another order, with t, n and sig among them
    const Param got[] = {
        {"sig", s.sig.c_str()}, {"zone", "inside"}, {"t", s.t.c_str()}, {"text", "red"}, {"n", s.n.c_str()}};
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/say", got, 5, s, T));
}

static void test_anything_changed_is_a_bad_signature() {
    sigreq::Verifier v(KEY);
    const Param p[] = {{"text", "red"}};
    const Signed s = signOf("/say", p, 1, T, 7);
    const Param other[] = {{"text", "blue"}};
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", other, 1, s, T));
    const Param more[] = {{"text", "red"}, {"zone", "outside"}};
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", more, 2, s, T));
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", nullptr, 0, s, T));
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/state", p, 1, s, T));
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", p, 1, s, T, "POST"));
    Signed t = s;
    t.t = std::to_string(T + 1);   // a later time on the same signature
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", p, 1, t, T));
    Signed n = s;
    n.n[15] = n.n[15] == '0' ? '1' : '0';   // another nonce
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", p, 1, n, T));
    Signed g = s;
    g.sig[31] = g.sig[31] == '0' ? '1' : '0';   // the tag's last bits
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", p, 1, g, T));
    sigreq::Verifier otherKey("another-key");
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(otherKey, "/say", p, 1, s, T));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/say", p, 1, s, T));   // the bad ones left no trace
}

// Percent-encoding: a value can't pose as more params.
static void test_a_value_cant_pose_as_params() {
    sigreq::Verifier v(KEY);
    const Param one[] = {{"text", "red&zone=inside"}};
    const Signed s = signOf("/say", one, 1, T, 9);
    const Param two[] = {{"text", "red"}, {"zone", "inside"}};
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/say", two, 2, s, T));
    const Param bar[] = {{"text", "a|b"}};
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/say", bar, 1, signOf("/say", bar, 1, T, 10), T));
}

static void test_the_time_window() {
    const Param p[] = {{"on", "1"}};
    sigreq::Verifier v(KEY);
    // the first check sets the boot floor (now - uptime): start well before the times tried below
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/music", p, 1, signOf("/music", p, 1, T - 500, 1), T - 500));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/music", p, 1, signOf("/music", p, 1, T - 120, 2), T));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/music", p, 1, signOf("/music", p, 1, T + 120, 3), T));
    TEST_ASSERT_EQUAL(Verdict::Stale, verify(v, "/music", p, 1, signOf("/music", p, 1, T - 121, 4), T));
    TEST_ASSERT_EQUAL(Verdict::Stale, verify(v, "/music", p, 1, signOf("/music", p, 1, T + 121, 5), T));
}

static void test_no_clock_checks_nothing() {
    sigreq::Verifier v(KEY);
    const Signed s = signOf("/state", nullptr, 0, T, 1);
    TEST_ASSERT_EQUAL(Verdict::NoClock,
                      v.check("GET", "/state", nullptr, 0, s.t.c_str(), s.n.c_str(), s.sig.c_str(), false, 5, UP));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, s, T));
}

static Verdict raw(sigreq::Verifier& v, const char* t, const char* n, const char* sig) {
    return v.check("GET", "/state", nullptr, 0, t, n, sig, true, T, UP);
}

static void test_unsigned_and_malformed() {
    sigreq::Verifier v(KEY);
    const Signed s = signOf("/state", nullptr, 0, T, 1);
    const char *t = s.t.c_str(), *n = s.n.c_str(), *sig = s.sig.c_str();
    TEST_ASSERT_EQUAL(Verdict::Unsigned, raw(v, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL(Verdict::Unsigned, raw(v, t, n, nullptr));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, nullptr, n, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, nullptr, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, "-5", n, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, "17616x0000", n, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, "", n, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, "1234567890123", n, sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, "00000000000000f", sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, "00000000000000FF", sig));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, n, s.sig.substr(0, 30).c_str()));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, n, (s.sig + "00").c_str()));
    TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, n, "zz"));
    std::string upper = s.sig;
    for (char& c : upper) c = (char)toupper((unsigned char)c);
    if (upper != s.sig) TEST_ASSERT_EQUAL(Verdict::Malformed, raw(v, t, n, upper.c_str()));
    TEST_ASSERT_EQUAL(Verdict::Ok, raw(v, t, n, sig));
}

// A ring of 32: a request that has left it is refused by its time, so a replay never gets in
// whatever the rate.
static void test_a_request_that_left_the_ring_is_stale() {
    sigreq::Verifier v(KEY);
    const Signed first = signOf("/state", nullptr, 0, T, 1000);
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, first, T));
    for (int i = 1; i < sigreq::RING; ++i) {   // fills the ring, the first still in it
        const Signed s = signOf("/state", nullptr, 0, T + i, 1000 + (uint64_t)i);
        TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, s, T + i));
    }
    TEST_ASSERT_EQUAL(Verdict::Replay, verify(v, "/state", nullptr, 0, first, T + 40));
    // one more pushes the first out
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, signOf("/state", nullptr, 0, T + 40, 5000), T + 40));
    TEST_ASSERT_EQUAL(Verdict::Stale, verify(v, "/state", nullptr, 0, first, T + 41));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, signOf("/state", nullptr, 0, T + 41, 5001), T + 41));
}

// Out of order (two signers, clocks apart): the floor is the newest time that left the ring.
static void test_the_floor_is_the_newest_time_let_go() {
    sigreq::Verifier v(KEY);
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, signOf("/state", nullptr, 0, T, 1), T));
    const Signed ahead = signOf("/state", nullptr, 0, T + 60, 2);   // a clock 60 s ahead
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, ahead, T + 1));
    for (int i = 0; i < sigreq::RING; ++i) {
        const Signed s = signOf("/state", nullptr, 0, T + 2 + i, 100 + (uint64_t)i);
        TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, s, T + 2 + i));
    }
    // both early ones have left the ring; the floor is T + 60, so the one ahead can't come back
    TEST_ASSERT_EQUAL(Verdict::Stale, verify(v, "/state", nullptr, 0, ahead, T + 40));
}

// After a reboot the ring is empty: nothing signed before the boot instant (now - uptime at the
// first check) gets in, whenever that first check comes.
static void test_nothing_from_before_the_boot() {
    sigreq::Verifier v(KEY);
    const Signed before = signOf("/state", nullptr, 0, T - 31, 1);   // booted at T - 30
    const Signed atBoot = signOf("/state", nullptr, 0, T - 30, 2);
    TEST_ASSERT_EQUAL(Verdict::Stale,
                      v.check("GET", "/state", nullptr, 0, before.t.c_str(), before.n.c_str(), before.sig.c_str(), true, T, 30));
    TEST_ASSERT_EQUAL(Verdict::Ok,
                      v.check("GET", "/state", nullptr, 0, atBoot.t.c_str(), atBoot.n.c_str(), atBoot.sig.c_str(), true, T, 30));
    TEST_ASSERT_EQUAL(Verdict::Stale, verify(v, "/state", nullptr, 0, before, T + 1));   // the floor stays
}

// The HMAC failing (on the decks: never, the SHA-256 is our own, but if it did) refuses: no sign, no
// accept, and a zero tag is never what is compared against.
static void test_a_failing_hmac_fails_closed() {
    sigreq::Verifier v(KEY);
    const Signed good = signOf("/state", nullptr, 0, T, 1);
    char q[sigreq::AUTH_LEN];
    hmac256::failForTest(true);
    TEST_ASSERT_FALSE(sigreq::sign(KEY, "GET", "/state", nullptr, 0, T, 2, q, sizeof q));
    TEST_ASSERT_EQUAL_STRING("", q);
    TEST_ASSERT_EQUAL(Verdict::Failed, verify(v, "/state", nullptr, 0, good, T));
    const Signed zeros{std::to_string(T), "0000000000000003", std::string(32, '0')};
    TEST_ASSERT_EQUAL(Verdict::Failed, verify(v, "/state", nullptr, 0, zeros, T));
    hmac256::failForTest(false);
    TEST_ASSERT_EQUAL(Verdict::BadSig, verify(v, "/state", nullptr, 0, zeros, T));
    TEST_ASSERT_EQUAL(Verdict::Ok, verify(v, "/state", nullptr, 0, good, T));   // the failed check left no trace
}

static void test_sign_refuses() {
    char q[sigreq::AUTH_LEN];
    TEST_ASSERT_FALSE(sigreq::sign("", "GET", "/state", nullptr, 0, T, 1, q, sizeof q));
    TEST_ASSERT_EQUAL_STRING("", q);
    TEST_ASSERT_FALSE(sigreq::sign(nullptr, "GET", "/state", nullptr, 0, T, 1, q, sizeof q));
    TEST_ASSERT_FALSE(sigreq::sign(KEY, "GET", "/state", nullptr, 0, T, 1, q, sigreq::AUTH_LEN - 1));
    TEST_ASSERT_FALSE(sigreq::sign(KEY, "GET", "/state", nullptr, 0, -1, 1, q, sizeof q));
    const std::string longText(400, 'a');
    const Param big[] = {{"text", longText.c_str()}};
    TEST_ASSERT_FALSE(sigreq::sign(KEY, "GET", "/say", big, 1, T, 1, q, sizeof q));
    Param many[sigreq::MAX_PARAMS + 1];
    for (auto& m : many) m = {"a", "b"};
    TEST_ASSERT_FALSE(sigreq::sign(KEY, "GET", "/say", many, sigreq::MAX_PARAMS + 1, T, 1, q, sizeof q));
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", "/say", many, sigreq::MAX_PARAMS, T, 1, q, sizeof q));
    TEST_ASSERT_TRUE(sigreq::sign(KEY, "GET", "/state", nullptr, 0, 999999999999LL, 1, q, sizeof q));   // the longest t fits
}

static void test_a_verifier_without_a_key_refuses() {
    sigreq::Verifier v("");
    TEST_ASSERT_EQUAL(Verdict::Malformed, verify(v, "/state", nullptr, 0, signOf("/state", nullptr, 0, T, 1), T));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_answers);
    RUN_TEST(test_a_signed_request_is_accepted_once);
    RUN_TEST(test_param_order_and_reserved_names_dont_matter);
    RUN_TEST(test_anything_changed_is_a_bad_signature);
    RUN_TEST(test_a_value_cant_pose_as_params);
    RUN_TEST(test_the_time_window);
    RUN_TEST(test_no_clock_checks_nothing);
    RUN_TEST(test_unsigned_and_malformed);
    RUN_TEST(test_a_request_that_left_the_ring_is_stale);
    RUN_TEST(test_the_floor_is_the_newest_time_let_go);
    RUN_TEST(test_nothing_from_before_the_boot);
    RUN_TEST(test_a_failing_hmac_fails_closed);
    RUN_TEST(test_sign_refuses);
    RUN_TEST(test_a_verifier_without_a_key_refuses);
    return UNITY_END();
}
