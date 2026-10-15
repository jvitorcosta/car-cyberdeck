#include <unity.h>
#include <cstdint>
#include "../../src/util/readguard.h"

using readguard::Guard;
using readguard::State;

void setUp() {}
void tearDown() {}

void test_ok_while_data_keeps_coming_before_the_deadline() {
    Guard g(1000, 3000, 10000);
    TEST_ASSERT_EQUAL_INT((int)State::Ok, (int)g.check(3999));
    g.onData(3500);
    TEST_ASSERT_EQUAL_INT((int)State::Ok, (int)g.check(6000));   // stall clock restarted at 3500
}

void test_stalled_when_no_data_for_the_stall_time() {
    Guard g(1000, 3000, 10000);
    TEST_ASSERT_EQUAL_INT((int)State::Stalled, (int)g.check(4001));
}

void test_deadline_even_when_a_byte_trickles_in_every_second() {
    // A server sending one byte per second never stalls, but must not hold the task forever.
    Guard g(0, 3000, 10000);
    uint32_t t = 0;
    for (; t < 10000; t += 1000) {
        TEST_ASSERT_EQUAL_INT((int)State::Ok, (int)g.check(t));
        g.onData(t);
    }
    TEST_ASSERT_EQUAL_INT((int)State::Deadline, (int)g.check(10001));
}

void test_survives_millis_wrap() {
    const uint32_t start = 0xFFFFF000u;   // 4 s before the wrap
    Guard g(start, 3000, 10000);
    g.onData(start + 2000);
    TEST_ASSERT_EQUAL_INT((int)State::Ok, (int)g.check(start + 4500));      // past the wrap
    TEST_ASSERT_EQUAL_INT((int)State::Stalled, (int)g.check(start + 5001));
    Guard d(start, 100000, 10000);
    TEST_ASSERT_EQUAL_INT((int)State::Deadline, (int)d.check(start + 10001));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ok_while_data_keeps_coming_before_the_deadline);
    RUN_TEST(test_stalled_when_no_data_for_the_stall_time);
    RUN_TEST(test_deadline_even_when_a_byte_trickles_in_every_second);
    RUN_TEST(test_survives_millis_wrap);
    return UNITY_END();
}
