#include <unity.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../../src/util/car/fuel22.h"
#include "../../src/util/car/obd.h"

void setUp() {}
void tearDown() {}

// "62 70 29" and then `lead` filler bytes, the 16-bit value at body bytes 12-13, and 2 more.
static void reply(char* out, size_t n, unsigned raw16, int bodyBytes = 16) {
    char* p = out;
    p += snprintf(p, n, "62 70 29");
    for (int i = 0; i < bodyBytes; ++i) {
        unsigned b = i == 12 ? raw16 >> 8 : i == 13 ? (raw16 & 0xFF) : 0x11;
        p += snprintf(p, n - static_cast<size_t>(p - out), " %02X", b);
    }
}

void test_the_three_commands_are_exact_and_allowed() {
    TEST_ASSERT_EQUAL_STRING("ATSH DA60F1\r", fuel22::SET_METER);
    TEST_ASSERT_EQUAL_STRING("227029\r", fuel22::ASK);
    TEST_ASSERT_EQUAL_STRING("ATSH DB33F1\r", fuel22::SET_DEFAULT);
    TEST_ASSERT_TRUE(obd::allowed(fuel22::SET_METER));
    TEST_ASSERT_TRUE(obd::allowed(fuel22::ASK));
    TEST_ASSERT_TRUE(obd::allowed(fuel22::SET_DEFAULT));
}

void test_the_neighbours_stay_refused() {
    const char* bad[] = {"227028\r", "22702A\r", "227030\r", "2270290\r", "22702\r", "227022\r", "222660\r", "2201\r",
                         "2E7029\r", "2F7029\r", "317029\r", "107029\r", "147029\r", "277029\r", "3E7029\r",
                         "ATSH DA61F1\r", "ATSH DA10F1\r", "ATSH DA1EF1\r", "ATSH DA60F2\r", "ATSH DB33F2\r", "ATSH 7E0\r",
                         "ATSH DA60F1", "227029", "ATSH DA60F1\r\r", "227029\r\r", "atsh da60f1\r", "ATSH  DA60F1\r",
                         "ATCP 18\r", "ATSH DA60F1\r227029\r", "ATSH\r", "ATCRA 18DAF160\r", "ATH1\r"};
    for (const char* c : bad) TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c);
}

void test_protocol_29() {
    TEST_ASSERT_TRUE(fuel22::protocol29("A7"));
    TEST_ASSERT_TRUE(fuel22::protocol29("7"));
    TEST_ASSERT_TRUE(fuel22::protocol29("A7\n"));
    TEST_ASSERT_TRUE(fuel22::protocol29("ATDPN\nA7"));
    TEST_ASSERT_FALSE(fuel22::protocol29("A0"));
    TEST_ASSERT_FALSE(fuel22::protocol29("A6"));
    TEST_ASSERT_FALSE(fuel22::protocol29("6"));
    TEST_ASSERT_FALSE(fuel22::protocol29("A9"));
    TEST_ASSERT_FALSE(fuel22::protocol29("17"));
    TEST_ASSERT_FALSE(fuel22::protocol29("NO DATA"));
    TEST_ASSERT_FALSE(fuel22::protocol29(""));
    TEST_ASSERT_FALSE(fuel22::protocol29(nullptr));
}

void test_parse_a_single_frame_style_reply() {
    char raw[160];
    reply(raw, sizeof raw, 2143);   // 21.43 L
    float l = -1;
    TEST_ASSERT_TRUE(fuel22::parse(raw, &l));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.43f, l);
}

void test_parse_a_multi_frame_reply() {
    // the ELM's ISO-TP output: a length line, then "0:", "1:" ... lines (headers off)
    const char* raw = "01B\n0: 62 70 29 11 11 11\n1: 11 11 11 11 11 11 11\n2: 11 11 08 34 11 11\n";
    float l = -1;
    TEST_ASSERT_TRUE(fuel22::parse(raw, &l));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.0f, l);   // 0x0834 = 2100 at body bytes 12-13
}

void test_parse_ignores_what_ask_adds() {
    // ask() hands back '\n' lines; the adapter may say SEARCHING... first, and two ECUs may answer
    const char* searching = "SEARCHING...\n013\n0: 62 70 29 11 11 11\n1: 11 11 11 11 11 11 11\n2: 11 11 08 34 11 11\n";
    float l = -1;
    TEST_ASSERT_TRUE(fuel22::parse(searching, &l));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.0f, l);
    // the meter ECU's answer, then another ECU's one-line NO DATA: the first good one wins
    char two[200];
    reply(two, sizeof two, 3000);
    strncat(two, "\nNO DATA", sizeof two - strlen(two) - 1);
    TEST_ASSERT_TRUE(fuel22::parse(two, &l));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, l);
    // an echo of the request in front does not matter
    TEST_ASSERT_TRUE(fuel22::parse("227029\n013\n0: 62 70 29 11 11 11\n1: 11 11 11 11 11 11 11\n2: 11 11 08 34 11 11", &l));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.0f, l);
}

void test_parse_refuses_what_is_not_a_level() {
    float l = 7.0f;
    char raw[160];
    TEST_ASSERT_FALSE(fuel22::parse("NO DATA", &l));
    TEST_ASSERT_FALSE(fuel22::parse("7F 22 31", &l));
    TEST_ASSERT_FALSE(fuel22::parse("", &l));
    TEST_ASSERT_FALSE(fuel22::parse(nullptr, &l));
    TEST_ASSERT_FALSE(fuel22::parse("62 70 29 11 22", &l));            // cut off before bytes 12-13
    TEST_ASSERT_FALSE(fuel22::parse("62 70 28 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF", &l));   // another DID
    reply(raw, sizeof raw, 5100);                                       // 51.00 L: more than a City's tank
    TEST_ASSERT_FALSE(fuel22::parse(raw, &l));
    reply(raw, sizeof raw, 65535);                                      // 655.35
    TEST_ASSERT_FALSE(fuel22::parse(raw, &l));
    reply(raw, sizeof raw, 4400, 13);                                   // exactly one byte short
    TEST_ASSERT_FALSE(fuel22::parse(raw, &l));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, l);                                   // untouched on every refusal
    reply(raw, sizeof raw, 4400);
    TEST_ASSERT_TRUE(fuel22::parse(raw, &l));                           // 44.00 L, the full tank
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 44.0f, l);
    reply(raw, sizeof raw, 0);
    TEST_ASSERT_TRUE(fuel22::parse(raw, &l));                           // 0.00 L: empty is a level too
    TEST_ASSERT_EQUAL_FLOAT(0.0f, l);
}

void test_a_level_is_fresh_for_five_minutes() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, fuel22::freshLitres(20.0f, 1000, 1000 + fuel22::FRESH_MS - 1));
    TEST_ASSERT_TRUE(std::isnan(fuel22::freshLitres(20.0f, 1000, 1000 + fuel22::FRESH_MS)));
    TEST_ASSERT_TRUE(std::isnan(fuel22::freshLitres(NAN, 1000, 1001)));
}

void test_the_plan_waits_then_asks_every_minute() {
    fuel22::Plan p;
    TEST_ASSERT_FALSE(p.due(100000));                 // never started
    p.start(1000, true);
    TEST_ASSERT_FALSE(p.due(1000 + fuel22::FIRST_MS - 1));
    TEST_ASSERT_TRUE(p.due(1000 + fuel22::FIRST_MS));
    p.done(20000, true);
    TEST_ASSERT_FALSE(p.due(20000 + fuel22::EVERY_MS - 1));
    TEST_ASSERT_TRUE(p.due(20000 + fuel22::EVERY_MS));
    TEST_ASSERT_FALSE(p.off());
}

void test_the_plan_gives_up_after_three_misses_in_a_row() {
    fuel22::Plan p;
    p.start(0, true);
    p.done(1, false);
    p.done(2, false);
    TEST_ASSERT_FALSE(p.off());
    p.done(3, true);                                  // a hit resets the count
    p.done(4, false);
    p.done(5, false);
    TEST_ASSERT_FALSE(p.off());
    p.done(6, false);
    TEST_ASSERT_TRUE(p.off());
    TEST_ASSERT_FALSE(p.due(1000000));
}

void test_the_plan_is_off_without_a_29_bit_bus() {
    fuel22::Plan p;
    p.start(0, false);
    TEST_ASSERT_TRUE(p.off());
    TEST_ASSERT_FALSE(p.due(1000000));
}

void test_the_plan_survives_the_millis_wrap() {
    fuel22::Plan p;
    p.start(0xFFFFFFFFu - 5000u, true);               // due 15 s later, past the wrap
    TEST_ASSERT_FALSE(p.due(0xFFFFFFFFu));
    TEST_ASSERT_TRUE(p.due(10000u));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_three_commands_are_exact_and_allowed);
    RUN_TEST(test_the_neighbours_stay_refused);
    RUN_TEST(test_protocol_29);
    RUN_TEST(test_parse_a_single_frame_style_reply);
    RUN_TEST(test_parse_a_multi_frame_reply);
    RUN_TEST(test_parse_ignores_what_ask_adds);
    RUN_TEST(test_parse_refuses_what_is_not_a_level);
    RUN_TEST(test_a_level_is_fresh_for_five_minutes);
    RUN_TEST(test_the_plan_waits_then_asks_every_minute);
    RUN_TEST(test_the_plan_gives_up_after_three_misses_in_a_row);
    RUN_TEST(test_the_plan_is_off_without_a_29_bit_bus);
    RUN_TEST(test_the_plan_survives_the_millis_wrap);
    return UNITY_END();
}
