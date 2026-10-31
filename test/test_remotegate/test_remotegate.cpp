#include <unity.h>
#include <cstdint>
#include "../../src/util/common/remotegate.h"

using remotegate::Gate;
using remotegate::Result;

void setUp() {}
void tearDown() {}

void test_a_request_is_judged_key_then_busy_then_rate() {
    struct { const char* key; bool playing; Result want; const char* why; } rows[] = {
        {"s3cret", false, Result::Play,     "right key, idle"},
        {"wrong",  false, Result::BadKey,   "wrong key"},
        {"",       false, Result::BadKey,   "no key"},
        {nullptr,  false, Result::BadKey,   "missing key"},
        {"s3cre",  false, Result::BadKey,   "prefix of the key"},
        {"s3cret!",false, Result::BadKey,   "key plus a character"},
        {"wrong",  true,  Result::BadKey,   "a stranger learns nothing about the deck's state"},
        {"s3cret", true,  Result::Busy,     "a clip is playing"},
    };
    for (auto& r : rows) {
        Gate g("s3cret");
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)r.want, (int)g.request(r.key, r.playing, 1000), r.why);
    }
}
void test_no_token_configured_turns_the_remote_off() {
    Gate g("");
    TEST_ASSERT_FALSE(g.enabled());
    TEST_ASSERT_EQUAL_INT((int)Result::BadKey, (int)g.request("", false, 1000));
    Gate n(nullptr);
    TEST_ASSERT_EQUAL_INT((int)Result::BadKey, (int)n.request(nullptr, false, 1000));
}
void test_one_play_per_three_seconds() {
    Gate g("k");
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, 10000));
    TEST_ASSERT_EQUAL_INT((int)Result::TooSoon, (int)g.request("k", false, 10000 + remotegate::MIN_GAP_MS - 1));
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, 10000 + remotegate::MIN_GAP_MS));
}
void test_rejected_requests_dont_restart_the_wait() {
    Gate g("k");
    g.request("k", false, 10000);
    g.request("k", false, 11000);                       // too soon: not a play
    g.request("x", false, 12000);                       // bad key
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, 13000));
}
void test_rate_limit_survives_the_millis_wrap() {
    Gate g("k");
    const uint32_t t = 0xFFFFFFFFu - 1000;
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, t));
    TEST_ASSERT_EQUAL_INT((int)Result::TooSoon, (int)g.request("k", false, t + 1500));   // wrapped
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, t + remotegate::MIN_GAP_MS));
}
void test_the_first_request_after_boot_plays() {
    Gate g("k");
    TEST_ASSERT_EQUAL_INT((int)Result::Play, (int)g.request("k", false, 5));   // millis() near 0
}

void test_key_check_alone_never_uses_up_a_play() {
    // GET /state checks the key every 5 s while the page is open; it must not count as a play.
    remotegate::Gate g("secret");
    TEST_ASSERT_TRUE(g.keyOk("secret"));
    TEST_ASSERT_FALSE(g.keyOk("secreT"));
    TEST_ASSERT_FALSE(g.keyOk(""));
    TEST_ASSERT_FALSE(g.keyOk(nullptr));
    TEST_ASSERT_EQUAL_INT((int)remotegate::Result::Play, (int)g.request("secret", false, 100));
    remotegate::Gate off("");
    TEST_ASSERT_FALSE(off.keyOk(""));               // remote off: nothing is accepted
}

void test_signed_only_gate_refuses_the_bearer_key() {
    // Between decks (the OBD deck's /car and /fill) only a signature passes: the key never travels
    // in a URL. The gate stays enabled, so reqauth::pass still checks signatures against it.
    remotegate::Gate g("secret", remotegate::Accept::SignedOnly);
    TEST_ASSERT_TRUE(g.enabled());
    TEST_ASSERT_FALSE(g.keyOk("secret"));
    TEST_ASSERT_EQUAL_INT((int)remotegate::Result::BadKey, (int)g.request("secret", false, 100));
    remotegate::Gate def("secret");                 // the default still takes the key (Siri's /say)
    TEST_ASSERT_TRUE(def.keyOk("secret"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_signed_only_gate_refuses_the_bearer_key);
    RUN_TEST(test_a_request_is_judged_key_then_busy_then_rate);
    RUN_TEST(test_no_token_configured_turns_the_remote_off);
    RUN_TEST(test_key_check_alone_never_uses_up_a_play);
    RUN_TEST(test_one_play_per_three_seconds);
    RUN_TEST(test_rejected_requests_dont_restart_the_wait);
    RUN_TEST(test_rate_limit_survives_the_millis_wrap);
    RUN_TEST(test_the_first_request_after_boot_plays);
    return UNITY_END();
}
