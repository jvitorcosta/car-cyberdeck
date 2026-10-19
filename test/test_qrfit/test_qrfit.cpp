#include <unity.h>
#include <cstring>
#include "../../src/util/qrfit.h"

void setUp() {}
void tearDown() {}

void test_smallest_version_for_the_link() {
    TEST_ASSERT_EQUAL_INT(1, qrfit::version(17));
    TEST_ASSERT_EQUAL_INT(2, qrfit::version(18));
    TEST_ASSERT_EQUAL_INT(2, qrfit::version(32));
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(33));
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(53));
    // The real link with a 24-character key: http://172.20.10.10/?k=<24> = 47 bytes.
    TEST_ASSERT_EQUAL_INT(3, qrfit::version(strlen("http://172.20.10.10/?k=0123456789abcdefghijklmn")));
}

void test_too_long_for_the_dialogue_box_is_zero() {
    TEST_ASSERT_EQUAL_INT(0, qrfit::version(54));
    TEST_ASSERT_EQUAL_INT(0, qrfit::version(0));
}

void test_drawn_size_with_the_standard_quiet_zone() {
    TEST_ASSERT_EQUAL_INT(21, qrfit::modules(1));
    TEST_ASSERT_EQUAL_INT(29, qrfit::modules(3));
    TEST_ASSERT_EQUAL_INT((29 + 8) * 2, qrfit::pixels(3));   // 74 px: fits the sky left of Pikachu
}

// The phone-remote link from the deck's address right now: the fixed one on the iPhone hotspot,
// the DHCP one anywhere else.
void test_link_from_the_current_address() {
    char out[96];
    TEST_ASSERT_TRUE(qrfit::link("172.20.10.10", "0123456789abcdefghijklmn", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("http://172.20.10.10/?k=0123456789abcdefghijklmn", out);
    TEST_ASSERT_TRUE(qrfit::link("192.168.100.200", "0123456789abcdefghijklmn", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("http://192.168.100.200/?k=0123456789abcdefghijklmn", out);
    TEST_ASSERT_TRUE(qrfit::version(strlen(out)) > 0);   // the longest address still fits
}

void test_no_link_without_a_key_or_an_address() {
    char out[96] = "old";
    TEST_ASSERT_FALSE(qrfit::link("172.20.10.10", "", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_FALSE(qrfit::link("0.0.0.0", "key", out, sizeof out));      // offline
    TEST_ASSERT_FALSE(qrfit::link("", "key", out, sizeof out));
    TEST_ASSERT_FALSE(qrfit::link(nullptr, "key", out, sizeof out));
    TEST_ASSERT_FALSE(qrfit::link("172.20.10.10", nullptr, out, sizeof out));
}

void test_no_link_too_long_for_the_code_or_the_buffer() {
    char out[96];
    TEST_ASSERT_FALSE(qrfit::link("192.168.100.200", "0123456789abcdefghijklmnopqrstuvwxyz", out, sizeof out));
    char small[20];
    TEST_ASSERT_FALSE(qrfit::link("172.20.10.10", "key", small, sizeof small));
    TEST_ASSERT_EQUAL_STRING("", small);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_smallest_version_for_the_link);
    RUN_TEST(test_too_long_for_the_dialogue_box_is_zero);
    RUN_TEST(test_drawn_size_with_the_standard_quiet_zone);
    RUN_TEST(test_link_from_the_current_address);
    RUN_TEST(test_no_link_without_a_key_or_an_address);
    RUN_TEST(test_no_link_too_long_for_the_code_or_the_buffer);
    return UNITY_END();
}
