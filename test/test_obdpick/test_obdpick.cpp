#include <unity.h>
#include "../../src/util/car/obdpick.h"

using obdpick::Candidate;
using obdpick::Recent;

void setUp() {}
void tearDown() {}

void test_name_match_beats_an_earlier_serial_gadget() {
    // A neighbour's HM-10 (FFE0, its own name) comes first in the scan; the Vgate is later.
    const Candidate c[] = {
        {0x111111, "HMSoft", -40, true, false},
        {0x222222, "", -45, true, false},
        {0x333333, "vLinker MC-IOS", -70, true, false},
    };
    Recent r;
    TEST_ASSERT_EQUAL_INT(2, obdpick::pick(c, 3, r, 1000));
}

void test_best_rssi_among_name_matches() {
    const Candidate c[] = {
        {0x1, "OBDII", -80, false, false},
        {0x2, "V-LINK", -50, true, false},
        {0x3, "ELM327", -65, false, false},
    };
    Recent r;
    TEST_ASSERT_EQUAL_INT(1, obdpick::pick(c, 3, r, 1000));
}

void test_nameless_serial_fallback_only_without_a_name_match() {
    const Candidate gadgets[] = {
        {0x1, "HMSoft", -40, true, false},   // named, not an adapter: never
        {0x2, "", -70, true, false},
        {0x3, "", -55, true, false},         // the stronger nameless one
        {0x4, "", -30, false, false},        // nameless, no serial service: never
    };
    Recent r;
    TEST_ASSERT_EQUAL_INT(2, obdpick::pick(gadgets, 4, r, 1000));
    const Candidate withName[] = {
        {0x2, "", -30, true, false},
        {0x5, "OBDII", -90, false, false},
    };
    TEST_ASSERT_EQUAL_INT(1, obdpick::pick(withName, 2, r, 1000));
}

void test_a_recently_failed_address_is_skipped_for_ten_minutes() {
    const Candidate c[] = {
        {0xAA, "OBDII", -40, true, false},
        {0xBB, "", -50, true, false},
    };
    Recent r;
    r.failed(0xAA, 1000);
    TEST_ASSERT_EQUAL_INT(1, obdpick::pick(c, 2, r, 1000 + obdpick::SKIP_MS - 1));
    TEST_ASSERT_EQUAL_INT(0, obdpick::pick(c, 2, r, 1000 + obdpick::SKIP_MS));
    r.failed(0xBB, 2000);
    r.failed(0xAA, 2000);
    TEST_ASSERT_EQUAL_INT(-1, obdpick::pick(c, 2, r, 3000));
}

void test_the_ring_forgets_the_oldest_failure() {
    Recent r;
    for (uint64_t a = 1; a <= (uint64_t)obdpick::RING + 1; ++a) r.failed(a, 100);
    TEST_ASSERT_FALSE(r.skip(1, 200));   // pushed out by the fifth
    TEST_ASSERT_TRUE(r.skip(2, 200));
    TEST_ASSERT_TRUE(r.skip((uint64_t)obdpick::RING + 1, 200));
    r.failed(3, 300);                     // again: refreshed, not a second slot
    TEST_ASSERT_TRUE(r.skip(2, 200));
}

void test_lights_and_elk_names_are_never_picked() {
    const Candidate c[] = {
        {0x1, "OBDII", -40, true, true},     // a light's MAC (the caller's flag), even with an OBD name
        {0x2, "ELK-BLEDOM", -40, true, false},
        {0x3, "MELK-OA10", -40, true, false},
        {0x4, "elk-bledom", -40, true, false},
    };
    Recent r;
    TEST_ASSERT_EQUAL_INT(-1, obdpick::pick(c, 4, r, 1000));
    TEST_ASSERT_TRUE(obdpick::isLight("ELK-BLE"));
    TEST_ASSERT_FALSE(obdpick::isLight("OBDII"));
    TEST_ASSERT_FALSE(obdpick::isLight(nullptr));
}

void test_nothing_to_pick() {
    Recent r;
    TEST_ASSERT_EQUAL_INT(-1, obdpick::pick(nullptr, 0, r, 0));
    const Candidate c[] = {{0x1, nullptr, -40, false, false}};
    TEST_ASSERT_EQUAL_INT(-1, obdpick::pick(c, 1, r, 0));
}

void test_an_adapter_name_is_skipped_only_after_a_failed_setup() {
    // A first-time Vgate that misses one connect is tried again next round; one that connects but
    // doesn't answer as an ELM327 is skipped.
    TEST_ASSERT_FALSE(obdpick::skipAfter("OBDII", obdpick::Failure::Connect));
    TEST_ASSERT_TRUE(obdpick::skipAfter("OBDII", obdpick::Failure::Setup));
    TEST_ASSERT_FALSE(obdpick::skipAfter("vLinker MC-IOS", obdpick::Failure::Connect));
}

void test_a_nameless_fallback_is_skipped_after_either_failure() {
    TEST_ASSERT_TRUE(obdpick::skipAfter("", obdpick::Failure::Connect));
    TEST_ASSERT_TRUE(obdpick::skipAfter("", obdpick::Failure::Setup));
    TEST_ASSERT_TRUE(obdpick::skipAfter(nullptr, obdpick::Failure::Connect));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_an_adapter_name_is_skipped_only_after_a_failed_setup);
    RUN_TEST(test_a_nameless_fallback_is_skipped_after_either_failure);
    RUN_TEST(test_name_match_beats_an_earlier_serial_gadget);
    RUN_TEST(test_best_rssi_among_name_matches);
    RUN_TEST(test_nameless_serial_fallback_only_without_a_name_match);
    RUN_TEST(test_a_recently_failed_address_is_skipped_for_ten_minutes);
    RUN_TEST(test_the_ring_forgets_the_oldest_failure);
    RUN_TEST(test_lights_and_elk_names_are_never_picked);
    RUN_TEST(test_nothing_to_pick);
    return UNITY_END();
}
