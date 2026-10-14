#include <unity.h>
#include "../../src/util/roam.h"

void setUp() {}
void tearDown() {}

void test_on_the_preferred_network_never_scans() {
    roam::Roam r;
    r.onConnected(0, 0);
    TEST_ASSERT_FALSE(r.wantScan(10 * 60 * 1000));
}
void test_on_a_fallback_scans_after_a_minute() {
    roam::Roam r;
    r.onConnected(1, 1000);
    TEST_ASSERT_FALSE(r.wantScan(1000 + roam::SCAN_MS - 1));
    TEST_ASSERT_TRUE(r.wantScan(1000 + roam::SCAN_MS));
}
void test_moves_back_after_two_good_scans_in_a_row() {
    roam::Roam r;
    r.onConnected(1, 0);
    TEST_ASSERT_FALSE(r.onScan(true, -60, 60000));
    TEST_ASSERT_TRUE(r.onScan(true, -60, 120000));
}
void test_weak_or_missing_hotspot_resets_the_count() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onScan(true, -60, 60000);
    TEST_ASSERT_FALSE(r.onScan(true, roam::MIN_RSSI - 1, 120000));   // too weak: not a sighting
    TEST_ASSERT_FALSE(r.onScan(true, -60, 180000));                  // the count starts again
    r.onScan(false, 0, 240000);
    TEST_ASSERT_FALSE(r.onScan(true, -60, 300000));
}
void test_a_scan_restarts_the_wait() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onScan(false, 0, 60000);                              // a miss: the wait restarts (2 min)
    TEST_ASSERT_FALSE(r.wantScan(60000 + 2 * 60000 - 1));
    TEST_ASSERT_TRUE(r.wantScan(60000 + 2 * 60000));
}
void test_failed_moves_back_off() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onMoveFailed(0);
    TEST_ASSERT_FALSE(r.wantScan(2 * 60000 - 1));
    TEST_ASSERT_TRUE(r.wantScan(2 * 60000));
    r.onMoveFailed(0);
    TEST_ASSERT_FALSE(r.wantScan(3 * 60000 - 1));
    TEST_ASSERT_TRUE(r.wantScan(3 * 60000));
    r.onMoveFailed(0);
    TEST_ASSERT_TRUE(r.wantScan(3 * 60000));                         // capped at 3 min
}
void test_reaching_the_preferred_network_clears_the_back_off() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onMoveFailed(0);
    r.onMoveFailed(0);
    r.onConnected(0, 0);
    r.onConnected(1, 0);
    TEST_ASSERT_TRUE(r.wantScan(roam::SCAN_MS));
}
void test_tries_per_network_after_a_drop() {
    TEST_ASSERT_EQUAL_INT(2, roam::tries(0, 0));    // dropped off the hotspot: give it two tries
    TEST_ASSERT_EQUAL_INT(1, roam::tries(0, 1));    // was on the fallback: one
    TEST_ASSERT_EQUAL_INT(1, roam::tries(1, 0));
    TEST_ASSERT_EQUAL_INT(1, roam::tries(0, -1));   // boot
}

// While the hotspot simply isn't around, scan less (each scan pauses the radio ~2 s): 1, 2, then
// 5 minutes; a sighting brings it back to every minute.
void test_scans_back_off_while_the_hotspot_stays_away() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onScan(false, 0, 60000);                              // miss 1
    TEST_ASSERT_FALSE(r.wantScan(60000 + 2 * 60000 - 1));
    TEST_ASSERT_TRUE(r.wantScan(60000 + 2 * 60000));
    r.onScan(false, 0, 180000);                             // miss 2
    TEST_ASSERT_FALSE(r.wantScan(180000 + 3 * 60000 - 1));
    TEST_ASSERT_TRUE(r.wantScan(180000 + 3 * 60000));
}

// One sighting: confirm it CONFIRM_MS later instead of a whole interval (a move takes ~15 s
// once the hotspot is back), even after the waits had backed off.
void test_a_sighting_is_confirmed_quickly() {
    roam::Roam r;
    r.onConnected(1, 0);
    r.onScan(false, 0, 60000);
    r.onScan(false, 0, 180000);                             // backed off to 3 min
    TEST_ASSERT_FALSE(r.onScan(true, -60, 360000));         // seen once
    TEST_ASSERT_FALSE(r.wantScan(360000 + roam::CONFIRM_MS - 1));
    TEST_ASSERT_TRUE(r.wantScan(360000 + roam::CONFIRM_MS));
    TEST_ASSERT_TRUE(r.onScan(true, -60, 360000 + roam::CONFIRM_MS));   // move
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_on_the_preferred_network_never_scans);
    RUN_TEST(test_on_a_fallback_scans_after_a_minute);
    RUN_TEST(test_moves_back_after_two_good_scans_in_a_row);
    RUN_TEST(test_weak_or_missing_hotspot_resets_the_count);
    RUN_TEST(test_a_scan_restarts_the_wait);
    RUN_TEST(test_failed_moves_back_off);
    RUN_TEST(test_reaching_the_preferred_network_clears_the_back_off);
    RUN_TEST(test_tries_per_network_after_a_drop);
    RUN_TEST(test_scans_back_off_while_the_hotspot_stays_away);
    RUN_TEST(test_a_sighting_is_confirmed_quickly);
    return UNITY_END();
}
