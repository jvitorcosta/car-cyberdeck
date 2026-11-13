#include <unity.h>
#include "../../src/util/yellow/deckaddr.h"

void setUp() {}
void tearDown() {}

// Off the hotspot the yellow deck keeps the black deck's looked-up address through short blips (the new deck's
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

// On the hotspot: the fixed address first, by name after 3 polls there without a valid answer
// (another address, or a phone holding .10), the fixed address again when the name isn't found.
void test_hotspot_falls_back_to_the_name() {
    deckaddr::Keep k;
    TEST_ASSERT_TRUE(k.tryFixed());
    k.onPoll(false);
    k.onPoll(false);
    TEST_ASSERT_TRUE(k.tryFixed());
    k.onPoll(false);
    TEST_ASSERT_FALSE(k.tryFixed());                       // third miss: look the name up
    k.lookupFailed();
    TEST_ASSERT_TRUE(k.tryFixed());                        // not found: .10 again
}
void test_a_valid_answer_keeps_the_fixed_address() {
    deckaddr::Keep k;
    k.onPoll(false);
    k.onPoll(false);
    k.onPoll(true);
    k.onPoll(false);
    k.onPoll(false);
    TEST_ASSERT_TRUE(k.tryFixed());
}
void test_a_found_address_that_misses_goes_back_to_the_fixed_one() {
    deckaddr::Keep k;
    for (int i = 0; i < deckaddr::MISSES; ++i) k.onPoll(false);
    k.found(0xAC140A05);                                   // the black deck at .5
    TEST_ASSERT_EQUAL_HEX32(0xAC140A05, k.addr());
    k.onPoll(true);
    for (int i = 0; i < deckaddr::MISSES; ++i) k.onPoll(false);
    TEST_ASSERT_EQUAL_HEX32(0, k.addr());
    TEST_ASSERT_TRUE(k.tryFixed());
}

void test_the_last_address_outlives_a_forget() {
    deckaddr::Keep k;
    TEST_ASSERT_EQUAL_UINT32(0, k.last());
    k.found(0x0D64A8C0);
    for (int i = 0; i < deckaddr::MISSES; i++) k.onPoll(false);
    TEST_ASSERT_EQUAL_UINT32(0, k.addr());         // forgotten: the name is looked up again
    TEST_ASSERT_EQUAL_UINT32(0x0D64A8C0, k.last()); // but kept for when the lookup finds nothing
}

void test_a_saved_address_is_the_last_one_at_boot() {
    deckaddr::Keep k;
    k.remember(0x0D64A8C0);
    TEST_ASSERT_EQUAL_UINT32(0, k.addr());
    TEST_ASSERT_EQUAL_UINT32(0x0D64A8C0, k.last());
    k.found(0x0E64A8C0);
    TEST_ASSERT_EQUAL_UINT32(0x0E64A8C0, k.last());
}

void test_it_searches_until_an_address_is_known() {
    deckaddr::Keep k;
    TEST_ASSERT_TRUE(k.searching());               // nothing found, nothing saved: look in the background
    k.found(0x0D64A8C0);
    TEST_ASSERT_FALSE(k.searching());
    k.forget();
    TEST_ASSERT_FALSE(k.searching());              // the last address is there to ask
    deckaddr::Keep saved;
    saved.remember(0x0D64A8C0);
    TEST_ASSERT_FALSE(saved.searching());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_keeps_the_address_through_two_misses);
    RUN_TEST(test_a_good_poll_resets_the_misses);
    RUN_TEST(test_forget);
    RUN_TEST(test_hotspot_falls_back_to_the_name);
    RUN_TEST(test_a_valid_answer_keeps_the_fixed_address);
    RUN_TEST(test_a_found_address_that_misses_goes_back_to_the_fixed_one);
    RUN_TEST(test_the_last_address_outlives_a_forget);
    RUN_TEST(test_a_saved_address_is_the_last_one_at_boot);
    RUN_TEST(test_it_searches_until_an_address_is_known);
    return UNITY_END();
}
