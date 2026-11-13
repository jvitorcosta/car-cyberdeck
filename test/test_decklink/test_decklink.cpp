#include <unity.h>
#include "../../src/util/common/decklink.h"

using namespace decklink;

void setUp() {}
void tearDown() {}

void test_never_heard_is_not_linked() { TEST_ASSERT_FALSE(linked(0, 5000)); }

void test_linked_window() {
    TEST_ASSERT_TRUE(linked(1000, 15999));
    TEST_ASSERT_FALSE(linked(1000, 16000));
}

void test_wrap() { TEST_ASSERT_TRUE(linked(0xFFFFF000u, 0x00000F00u)); }

// millis() | 1 stored in an even millisecond, or a stamp taken after the reader's now (another
// task preempted it): heardAt a little ahead of now is linked, not a 49-day-old exchange.
void test_stamp_just_ahead_is_linked() {
    TEST_ASSERT_TRUE(linked(1001, 1000));
    TEST_ASSERT_TRUE(linked(1500, 1000));
    TEST_ASSERT_TRUE(linked(0x00000001u, 0xFFFFFFFFu));  // ahead across the wrap
}

// The not-answering skip holds for a stamp just ahead of now too.
void test_tracker_skips_after_not_answering_stamp_ahead() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_NULL(t.update(false, Gps::Unknown, 20000, 20001));
}

void test_body_and_parse() {
    char b[16];
    body(true, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("gps=1", b);
    TEST_ASSERT_EQUAL_INT((int)Gps::Fix, (int)parse(b));
    body(false, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("gps=0", b);
    TEST_ASSERT_EQUAL_INT((int)Gps::NoFix, (int)parse(b));
    TEST_ASSERT_EQUAL_INT((int)Gps::Unknown, (int)parse(""));
    TEST_ASSERT_EQUAL_INT((int)Gps::Unknown, (int)parse(nullptr));
    TEST_ASSERT_EQUAL_INT((int)Gps::Unknown, (int)parse("gps=2"));
    TEST_ASSERT_EQUAL_INT((int)Gps::Fix, (int)parse("gps=1\n"));
}

void test_shown() {
    TEST_ASSERT_EQUAL_INT((int)Gps::Unknown, (int)shown(false, Gps::Fix));
    TEST_ASSERT_EQUAL_INT((int)Gps::NoFix, (int)shown(true, Gps::NoFix));
}

void test_is_alert() {
    TEST_ASSERT_TRUE(isAlert(">DECK: NO LINK"));
    TEST_ASSERT_TRUE(isAlert(">GPS: NO FIX"));
    TEST_ASSERT_FALSE(isAlert(">DECK: LINKED"));
    TEST_ASSERT_FALSE(isAlert(">GPS: FIX"));
    TEST_ASSERT_FALSE(isAlert(nullptr));
}

void test_tracker_first_state_silent() {
    Tracker t;
    TEST_ASSERT_NULL(t.update(true, Gps::Fix, 1000, 0));
    TEST_ASSERT_NULL(t.update(true, Gps::Fix, 1250, 0));
}

void test_tracker_first_unlinked_silent() {
    Tracker t;
    TEST_ASSERT_NULL(t.update(false, Gps::Unknown, 1000, 0));
    TEST_ASSERT_EQUAL_STRING(LINKED_MSG, t.update(true, Gps::Fix, 2000, 0));
    TEST_ASSERT_NULL(t.update(true, Gps::Fix, 2250, 0));  // the first GPS state known: silent
}

void test_tracker_drop_and_back() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_EQUAL_STRING(NOLINK_MSG, t.update(false, Gps::Unknown, 20000, 0));
    TEST_ASSERT_EQUAL_STRING(LINKED_MSG, t.update(true, Gps::Fix, 30000, 0));
    TEST_ASSERT_NULL(t.update(true, Gps::Fix, 30250, 0));
}

void test_tracker_gps_change() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_EQUAL_STRING(NOFIX_MSG, t.update(true, Gps::NoFix, 2000, 0));
    TEST_ASSERT_NULL(t.update(true, Gps::NoFix, 2250, 0));
    TEST_ASSERT_EQUAL_STRING(FIX_MSG, t.update(true, Gps::Fix, 3000, 0));
}

void test_tracker_unknown_is_no_change() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_EQUAL_STRING(NOLINK_MSG, t.update(false, Gps::Unknown, 20000, 0));
    TEST_ASSERT_EQUAL_STRING(LINKED_MSG, t.update(true, Gps::NoFix, 30000, 0));
    TEST_ASSERT_EQUAL_STRING(NOFIX_MSG, t.update(true, Gps::NoFix, 30250, 0));
    TEST_ASSERT_NULL(t.update(true, Gps::NoFix, 30500, 0));
}

void test_tracker_skips_after_not_answering() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_NULL(t.update(false, Gps::Unknown, 20000, 10000));
    TEST_ASSERT_NULL(t.update(false, Gps::Unknown, 20250, 10000));
    TEST_ASSERT_EQUAL_STRING(LINKED_MSG, t.update(true, Gps::Fix, 40000, 10000));
}

void test_tracker_not_answering_long_ago_still_says() {
    Tracker t;
    t.update(true, Gps::Fix, 1000, 0);
    TEST_ASSERT_EQUAL_STRING(NOLINK_MSG, t.update(false, Gps::Unknown, 40000, 10000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_heard_is_not_linked);
    RUN_TEST(test_linked_window);
    RUN_TEST(test_wrap);
    RUN_TEST(test_stamp_just_ahead_is_linked);
    RUN_TEST(test_tracker_skips_after_not_answering_stamp_ahead);
    RUN_TEST(test_body_and_parse);
    RUN_TEST(test_shown);
    RUN_TEST(test_is_alert);
    RUN_TEST(test_tracker_first_state_silent);
    RUN_TEST(test_tracker_first_unlinked_silent);
    RUN_TEST(test_tracker_drop_and_back);
    RUN_TEST(test_tracker_gps_change);
    RUN_TEST(test_tracker_unknown_is_no_change);
    RUN_TEST(test_tracker_skips_after_not_answering);
    RUN_TEST(test_tracker_not_answering_long_ago_still_says);
    return UNITY_END();
}
