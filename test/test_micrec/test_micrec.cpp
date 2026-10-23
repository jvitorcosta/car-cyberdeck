#include <unity.h>
#include <cstdint>
#include "../../src/util/black/micrec.h"

void setUp() {}
void tearDown() {}

static uint32_t le32(const uint8_t* p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

void test_slots_map_to_condition_and_phrase() {
    TEST_ASSERT_EQUAL_CHAR('A', micrec::condition(0));
    TEST_ASSERT_EQUAL_CHAR('B', micrec::condition(10));
    TEST_ASSERT_EQUAL_CHAR('D', micrec::condition(39));
    TEST_ASSERT_EQUAL_CHAR('?', micrec::condition(40));
    TEST_ASSERT_EQUAL_CHAR('?', micrec::condition(-1));
    TEST_ASSERT_EQUAL_INT(9, micrec::phraseOf(19));
    TEST_ASSERT_EQUAL_INT(-1, micrec::phraseOf(40));
    TEST_ASSERT_EQUAL_STRING("toca Djavan", micrec::phrase(4));
    TEST_ASSERT_EQUAL_STRING("liga as luzes", micrec::phrase(0));
    TEST_ASSERT_EQUAL_STRING("aumenta o volume", micrec::phrase(9));
    TEST_ASSERT_EQUAL_STRING("", micrec::phrase(10));
}

void test_wav_header_is_16k_mono_pcm() {
    uint8_t h[44];
    micrec::wavHeader(h, 128000, 16000);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", h, 4);
    TEST_ASSERT_EQUAL_UINT32(36 + 128000, le32(h + 4));
    TEST_ASSERT_EQUAL_MEMORY("WAVEfmt ", h + 8, 8);
    TEST_ASSERT_EQUAL_UINT32(16, le32(h + 16));
    TEST_ASSERT_EQUAL_UINT16(1, le16(h + 20));      // PCM
    TEST_ASSERT_EQUAL_UINT16(1, le16(h + 22));      // mono
    TEST_ASSERT_EQUAL_UINT32(16000, le32(h + 24));
    TEST_ASSERT_EQUAL_UINT32(32000, le32(h + 28));  // byte rate
    TEST_ASSERT_EQUAL_UINT16(2, le16(h + 32));      // block align
    TEST_ASSERT_EQUAL_UINT16(16, le16(h + 34));
    TEST_ASSERT_EQUAL_MEMORY("data", h + 36, 4);
    TEST_ASSERT_EQUAL_UINT32(128000, le32(h + 40));
}

void test_short_taps_are_dropped_and_long_holds_capped() {
    TEST_ASSERT_EQUAL_INT(0, micrec::samplesToKeep(250));
    TEST_ASSERT_EQUAL_INT(0, micrec::samplesToKeep(299));
    TEST_ASSERT_EQUAL_INT(4800, micrec::samplesToKeep(300));
    TEST_ASSERT_EQUAL_INT(16000, micrec::samplesToKeep(1000));
    TEST_ASSERT_EQUAL_INT(64000, micrec::samplesToKeep(4000));
    TEST_ASSERT_EQUAL_INT(64000, micrec::samplesToKeep(9000));
}

void test_clip_names() {
    char s[8];
    micrec::clipName(0, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("A01", s);
    micrec::clipName(9, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("A10", s);
    micrec::clipName(39, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("D10", s);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slots_map_to_condition_and_phrase);
    RUN_TEST(test_wav_header_is_16k_mono_pcm);
    RUN_TEST(test_short_taps_are_dropped_and_long_holds_capped);
    RUN_TEST(test_clip_names);
    return UNITY_END();
}
