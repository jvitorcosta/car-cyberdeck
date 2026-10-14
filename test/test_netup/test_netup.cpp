#include <unity.h>
#include "../../src/util/netup.h"

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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_connected_without_an_address_is_not_usable);
    RUN_TEST(test_connected_with_an_address_is_usable);
    RUN_TEST(test_not_connected_is_not_usable);
    return UNITY_END();
}
