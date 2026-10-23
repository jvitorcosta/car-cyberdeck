#include <unity.h>
#include <cstring>
#include "../../src/util/lightstatus.h"

using lightstatus::State;

void setUp() {}
void tearDown() {}

static ledlook::Look look(int swatch, uint8_t pct, bool lit) {
    ledlook::Look l;
    l.swatch = swatch;
    l.pct = pct;
    l.lit = lit;
    return l;
}

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

void test_line() {
    const ledlook::Look pink50 = look(2, 50, true);
    const ledlook::Look pink100 = look(2, 100, true);
    char buf[64];

    lightstatus::line(pink50, true, pink100, true, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("IN ON 50%  OUT ON 100%  LINK 2/2", buf);

    lightstatus::line(pink50, true, pink100, false, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("IN ON 50%  OUT --  LINK 1/2", buf);

    lightstatus::line(look(2, 50, false), true, pink100, false, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("IN OFF  OUT --  LINK 1/2", buf);

    lightstatus::line(pink50, false, pink100, false, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("IN --  OUT --  LINK 0/2", buf);
}

void test_line_cut_to_buffer() {
    char buf[8];
    const ledlook::Look l = look(2, 50, true);
    lightstatus::line(l, true, l, true, buf, sizeof buf);
    TEST_ASSERT_EQUAL_UINT(7, strlen(buf));
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
    RUN_TEST(test_line);
    RUN_TEST(test_line_cut_to_buffer);
    RUN_TEST(test_reply);
    RUN_TEST(test_messages);
    RUN_TEST(test_is_alert);
    return UNITY_END();
}
