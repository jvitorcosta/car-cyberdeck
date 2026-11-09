#include <unity.h>
#include "../../src/util/yellow/keepalive.h"

using keepalive::Ask;
using keepalive::State;

void setUp() {}
void tearDown() {}

static State asked(uint32_t nextAt, int sentLight) {
    State s;
    s.asked = true;
    s.nextAt = nextAt;
    s.sentLight = sentLight;
    return s;
}

void test_first_want_asks_on() {
    TEST_ASSERT_EQUAL_INT((int)Ask::On, (int)keepalive::due(State{}, true, 0, 1));
}

void test_waits_until_next() {
    TEST_ASSERT_EQUAL_INT((int)Ask::None, (int)keepalive::due(asked(10000, 1), true, 5000, 1));
    TEST_ASSERT_EQUAL_INT((int)Ask::On, (int)keepalive::due(asked(10000, 1), true, 10000, 1));
}

void test_light_change_asks_early() {
    TEST_ASSERT_EQUAL_INT((int)Ask::On, (int)keepalive::due(asked(10000, 1), true, 5000, 4));
}

void test_light_change_ignored_while_off() {
    TEST_ASSERT_EQUAL_INT((int)Ask::None, (int)keepalive::due(State{}, false, 5000, 4));
}

void test_stop_asks_off_once() {
    TEST_ASSERT_EQUAL_INT((int)Ask::Off, (int)keepalive::due(asked(10000, 1), false, 5000, 1));
}

// millis() wraps after ~49 days: the wait still counts across it.
void test_waits_across_the_wrap() {
    TEST_ASSERT_EQUAL_INT((int)Ask::None, (int)keepalive::due(asked(5000, 1), true, 0xFFFFF000u, 1));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_want_asks_on);
    RUN_TEST(test_waits_until_next);
    RUN_TEST(test_light_change_asks_early);
    RUN_TEST(test_light_change_ignored_while_off);
    RUN_TEST(test_stop_asks_off_once);
    RUN_TEST(test_waits_across_the_wrap);
    return UNITY_END();
}
