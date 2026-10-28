#include <unity.h>
#include "../../src/util/common/netup.h"

void setUp() {}
void tearDown() {}

// The WiFi driver can report "connected" before (or after a reconnect, without) an address: the
// S3 sat 40 s at 0.0.0.0 with every DNS lookup failing (2026-10-14).
void test_connected_without_an_address_is_not_usable() {
    TEST_ASSERT_FALSE(netup::usable(true, 0));
}
void test_connected_with_an_address_is_usable() {
    TEST_ASSERT_TRUE(netup::usable(true, 0xAC140A0A));   // 172.20.10.10
}
void test_not_connected_is_not_usable() {
    TEST_ASSERT_FALSE(netup::usable(false, 0xAC140A0A));
}

// Final review M3: one "connected, 0.0.0.0" reading can be a transient; reconnect only when the
// next check (5 s later) still says so. A real disconnect reconnects at once.
void test_reconnect_on_a_real_drop_at_once() {
    netup::Watch w;
    TEST_ASSERT_TRUE(w.reconnect(false, 0));
}
void test_no_address_needs_two_checks_in_a_row() {
    netup::Watch w;
    TEST_ASSERT_FALSE(w.reconnect(true, 0));
    TEST_ASSERT_TRUE(w.reconnect(true, 0));
}
void test_a_good_check_in_between_resets() {
    netup::Watch w;
    w.reconnect(true, 0);
    TEST_ASSERT_FALSE(w.reconnect(true, 0xAC140A0A));
    TEST_ASSERT_FALSE(w.reconnect(true, 0));
}

// The yellow deck's reconnects (net::loop): each failed one waits longer before the next, 5 s,
// 15 s, 30 s, then every 60 s, so joins (scans, the radio away from BLE) don't run back to back.
void test_rejoin_waits_5_15_30_then_60_s() {
    netup::Rejoin r;
    TEST_ASSERT_TRUE(r.due(0));                       // nothing failed yet
    uint32_t t = 1000;
    const uint32_t want[] = {5000, 15000, 30000, 60000, 60000};
    for (uint32_t w : want) {
        r.onResult(false, t);
        TEST_ASSERT_EQUAL_UINT32(w, r.waitMs());
        TEST_ASSERT_FALSE(r.due(t + w - 1));
        TEST_ASSERT_TRUE(r.due(t + w));
        t += w + 24000;                               // the next try itself takes ~24 s
    }
}
void test_rejoin_online_resets_the_wait() {
    netup::Rejoin r;
    r.onResult(false, 0);
    r.onResult(false, 5000);
    r.onResult(true, 30000);
    TEST_ASSERT_EQUAL_UINT32(0, r.waitMs());
    TEST_ASSERT_TRUE(r.due(30000));
    r.onResult(false, 40000);                         // the next drop starts at 5 s again
    TEST_ASSERT_EQUAL_UINT32(5000, r.waitMs());
}
void test_rejoin_due_survives_millis_wrap() {
    netup::Rejoin r;
    r.onResult(false, 0xFFFFF000u);
    TEST_ASSERT_FALSE(r.due(0xFFFFF000u + 4999u));
    TEST_ASSERT_TRUE(r.due(0xFFFFF000u + 5000u));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rejoin_waits_5_15_30_then_60_s);
    RUN_TEST(test_rejoin_online_resets_the_wait);
    RUN_TEST(test_rejoin_due_survives_millis_wrap);
    RUN_TEST(test_connected_without_an_address_is_not_usable);
    RUN_TEST(test_connected_with_an_address_is_usable);
    RUN_TEST(test_not_connected_is_not_usable);
    RUN_TEST(test_reconnect_on_a_real_drop_at_once);
    RUN_TEST(test_no_address_needs_two_checks_in_a_row);
    RUN_TEST(test_a_good_check_in_between_resets);
    return UNITY_END();
}
