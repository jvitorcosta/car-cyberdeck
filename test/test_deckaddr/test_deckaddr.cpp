#include <unity.h>
#include "../../src/util/deckaddr.h"

void setUp() {}
void tearDown() {}

// The yellow deck keeps the black deck's looked-up address through short blips (the new deck's
// background scan drops one poll a minute): it only looks the name up again after 3 misses.
void test_keeps_the_address_through_two_misses() {
    deckaddr::Keep k;
    k.found(0x0A000001);
    k.onPoll(false);
    k.onPoll(false);
    TEST_ASSERT_EQUAL_HEX32(0x0A000001, k.addr());
    k.onPoll(false);
    TEST_ASSERT_EQUAL_HEX32(0, k.addr());                  // third miss: look it up again
}
void test_a_good_poll_resets_the_misses() {
    deckaddr::Keep k;
    k.found(0x0A000001);
    k.onPoll(false);
    k.onPoll(false);
    k.onPoll(true);
    k.onPoll(false);
    k.onPoll(false);
    TEST_ASSERT_EQUAL_HEX32(0x0A000001, k.addr());
}
void test_forget() {
    deckaddr::Keep k;
    k.found(0x0A000001);
    k.forget();                                            // switched networks
    TEST_ASSERT_EQUAL_HEX32(0, k.addr());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_keeps_the_address_through_two_misses);
    RUN_TEST(test_a_good_poll_resets_the_misses);
    RUN_TEST(test_forget);
    return UNITY_END();
}
