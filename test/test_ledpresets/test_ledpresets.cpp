#include <unity.h>
#include <cstring>
#include "../../src/util/ledpresets.h"

void setUp() {}
void tearDown() {}

static void expectPreset(int i, const char* name, const char* word, int sw, int inPct, int outPct) {
    const ledpresets::Preset& p = ledpresets::PRESETS[i];
    TEST_ASSERT_EQUAL_STRING(name, p.name);
    TEST_ASSERT_EQUAL_STRING(word, p.word);
    TEST_ASSERT_TRUE(p.in == (ledlook::Look{sw, static_cast<uint8_t>(inPct), true}));
    TEST_ASSERT_TRUE(p.out == (ledlook::Look{sw, static_cast<uint8_t>(outPct), true}));
}

void test_table() {
    expectPreset(0, "ROSE", "rose", 2, 40, 100);
    expectPreset(1, "FIRE", "fire", 0, 50, 100);
    expectPreset(2, "CALM", "calm", 6, 40, 40);
    expectPreset(3, "PURPURA", "purpura", 3, 40, 100);
}
void test_describe() {
    char buf[112];
    ledpresets::describe(1, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Rose: inside Pink 40 percent, outside Pink 100 percent", buf);
    ledpresets::describe(4, buf, sizeof buf);
    TEST_ASSERT_EQUAL_INT(0, strncmp(buf, "Purpura: ", 9));
}
void test_describe_fits() {
    char buf[112];
    for (int s = 1; s <= ledpresets::N; s++) {
        ledpresets::describe(s, buf, sizeof buf);
        TEST_ASSERT_TRUE(strlen(buf) < sizeof buf - 1);
    }
}
void test_wire_line() {
    char buf[64];
    ledpresets::wireLine(3, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("3 CALM white 40 white 40", buf);
    ledpresets::wireLine(1, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("1 ROSE pink 40 pink 100", buf);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_table);
    RUN_TEST(test_describe);
    RUN_TEST(test_describe_fits);
    RUN_TEST(test_wire_line);
    return UNITY_END();
}
