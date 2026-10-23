#include <unity.h>
#include <cstring>
#include "../../src/util/lightstatus.h"

using lightstatus::State;

void setUp() {}
void tearDown() {}

void test_no_link_never_on() {
    TEST_ASSERT_EQUAL_INT((int)State::NoLink, (int)lightstatus::of(false, true));
    TEST_ASSERT_EQUAL_INT((int)State::NoLink, (int)lightstatus::of(false, false));
    TEST_ASSERT_EQUAL_INT((int)State::On, (int)lightstatus::of(true, true));
    TEST_ASSERT_EQUAL_INT((int)State::Off, (int)lightstatus::of(true, false));
}

void test_words() {
    TEST_ASSERT_EQUAL_STRING("nolink", lightstatus::word(State::NoLink));
    TEST_ASSERT_EQUAL_STRING("off", lightstatus::word(State::Off));
    TEST_ASSERT_EQUAL_STRING("on", lightstatus::word(State::On));
}

// The Lights page's status line: just the link state; a light down is named, in ALERT.
void test_link() {
    char buf[24];
    TEST_ASSERT_FALSE(lightstatus::link(true, true, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("LINK 2/2", buf);
    TEST_ASSERT_TRUE(lightstatus::link(true, false, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("OUTSIDE: NO LINK", buf);
    TEST_ASSERT_TRUE(lightstatus::link(false, true, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("INSIDE: NO LINK", buf);
    TEST_ASSERT_TRUE(lightstatus::link(false, false, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("NO LINK", buf);
}

void test_link_cut_to_buffer() {
    char buf[8];
    TEST_ASSERT_TRUE(lightstatus::link(true, false, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("OUTSIDE", buf);
}

void test_reply() {
    char buf[40];
    lightstatus::reply(State::On, State::NoLink, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("inside=on outside=nolink", buf);
}

void test_messages() {
    char buf[32];
    lightstatus::applied("ROSE", buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING(">ROSE APPLIED", buf);
    lightstatus::linkChange(true, false, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING(">INSIDE: NO LINK", buf);
    lightstatus::linkChange(false, true, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING(">OUTSIDE: LINKED", buf);
    lightstatus::linkChangeBoth(false, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING(">LIGHTS: NO LINK", buf);
    lightstatus::linkChangeBoth(true, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING(">LIGHTS: LINKED", buf);
}

void test_is_alert() {
    TEST_ASSERT_TRUE(lightstatus::isAlert(">INSIDE: NO LINK"));
    TEST_ASSERT_TRUE(lightstatus::isAlert(">OUTSIDE: NO LINK"));
    TEST_ASSERT_TRUE(lightstatus::isAlert(">LIGHTS: NO LINK"));
    TEST_ASSERT_FALSE(lightstatus::isAlert(">INSIDE: LINKED"));
    TEST_ASSERT_FALSE(lightstatus::isAlert(">LIGHTS: LINKED"));
    TEST_ASSERT_FALSE(lightstatus::isAlert(">ROSE APPLIED"));
    TEST_ASSERT_FALSE(lightstatus::isAlert(""));
    TEST_ASSERT_FALSE(lightstatus::isAlert(nullptr));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_no_link_never_on);
    RUN_TEST(test_words);
    RUN_TEST(test_link);
    RUN_TEST(test_link_cut_to_buffer);
    RUN_TEST(test_reply);
    RUN_TEST(test_messages);
    RUN_TEST(test_is_alert);
    return UNITY_END();
}
