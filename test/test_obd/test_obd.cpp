#include <unity.h>
#include <cstring>
#include "../../src/util/obd.h"

using obd::Pid;
using obd::Reply;

void setUp() {}
void tearDown() {}

static Reply run(Pid p, const char* raw, float* v) { *v = -999.0f; return obd::parse(p, raw, v); }

void test_requests_end_with_cr_and_are_read_only() {
    TEST_ASSERT_EQUAL_STRING("010C\r", obd::request(Pid::Rpm));
    TEST_ASSERT_EQUAL_STRING("ATRV\r", obd::request(Pid::Battery));
    for (int i = 0; i < (int)Pid::COUNT; ++i) {          // mode 01 or AT only: never 03 / 04
        const char* r = obd::request((Pid)i);
        TEST_ASSERT_TRUE(strncmp(r, "01", 2) == 0 || strncmp(r, "AT", 2) == 0);
    }
}

void test_formulas_at_their_ends() {
    struct { Pid p; const char* raw; float want; } rows[] = {
        {Pid::Rpm, "41 0C 00 00", 0.0f},          {Pid::Rpm, "41 0C FF FF", 16383.75f},
        {Pid::Rpm, "41 0C 0B B8", 750.0f},        {Pid::Speed, "41 0D FF", 255.0f},
        {Pid::Coolant, "41 05 00", -40.0f},       {Pid::Coolant, "41 05 FF", 215.0f},
        {Pid::Load, "41 04 FF", 100.0f},          {Pid::Intake, "41 0F 3C", 20.0f},
        {Pid::Throttle, "41 11 00", 0.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)run(r.p, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.want, v, r.raw);
    }
}

void test_what_clones_wrap_around_a_reply() {
    const char* forms[] = {
        "410C0BB8",                                  // ATS0: no spaces
        "010C\n41 0C 0B B8",                         // echo still on
        "SEARCHING...\n41 0C 0B B8",                 // first request after ATSP0
        "BUS INIT: ...OK\n41 0C 0B B8\n",            // K-line init
        "7E8 04 41 0C 0B B8",                        // CAN headers on
        "41 0C 0B B8\n41 0C 0B B9",                  // two ECUs: the first wins
        "\n\n41 0c 0b b8  \n",                       // blank lines, lower case
    };
    for (const char* raw : forms) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)run(Pid::Rpm, raw, &v), raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 750.0f, v, raw);
    }
}

void test_errors_never_give_a_number() {
    struct { const char* raw; Reply want; } rows[] = {
        {"NO DATA", Reply::NoData},
        {"SEARCHING...\nUNABLE TO CONNECT", Reply::NoEngine},
        {"CAN ERROR", Reply::NoEngine},
        {"BUS INIT: ...ERROR", Reply::NoEngine},
        {"STOPPED", Reply::Error},
        {"?", Reply::Error},
        {"", Reply::Error},
        {"41 0C 1A", Reply::Error},                  // cut off before its second byte
        {"41 0D 32", Reply::Error},                  // a reply for another PID
        {"ELM327 v1.5", Reply::Error},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)run(Pid::Rpm, r.raw, &v), r.raw);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-999.0f, v, r.raw);   // untouched
    }
}

void test_battery_voltage_forms() {
    struct { const char* raw; Reply want; float v; } rows[] = {
        {"12.6V", Reply::Ok, 12.6f}, {"ATRV\n14.1V", Reply::Ok, 14.1f}, {" 9V ", Reply::Ok, 9.0f},
        {"V", Reply::Error, -999.0f}, {"?", Reply::Error, -999.0f}, {"", Reply::Error, -999.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)run(Pid::Battery, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.v, v, r.raw);
    }
}

void test_adapter_names() {
    const char* yes[] = {"OBDII", "OBD2", "obd-ii", "V-LINK", "ELM327", "Android-Vlink", "IOS-Vlink"};
    const char* no[] = {"JBL Flip 5", "iPhone", "", nullptr};
    for (const char* n : yes) TEST_ASSERT_TRUE_MESSAGE(obd::looksLikeAdapter(n), n);
    for (const char* n : no) TEST_ASSERT_FALSE(obd::looksLikeAdapter(n));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_requests_end_with_cr_and_are_read_only);
    RUN_TEST(test_formulas_at_their_ends);
    RUN_TEST(test_what_clones_wrap_around_a_reply);
    RUN_TEST(test_errors_never_give_a_number);
    RUN_TEST(test_battery_voltage_forms);
    RUN_TEST(test_adapter_names);
    return UNITY_END();
}
