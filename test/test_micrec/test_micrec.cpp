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
    TEST_ASSERT_EQUAL_CHAR('A', micrec::condition(9));
    TEST_ASSERT_EQUAL_CHAR('?', micrec::condition(10));
    TEST_ASSERT_EQUAL_STRING("Spotify a 20%, vidros fechados", micrec::conditionText(0));
    TEST_ASSERT_EQUAL_CHAR('?', micrec::condition(-1));
    TEST_ASSERT_EQUAL_INT(9, micrec::phraseOf(9));
    TEST_ASSERT_EQUAL_INT(-1, micrec::phraseOf(10));
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
    micrec::clipName(10, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("?", s);
}

void test_a_phrase_counts_down_then_records_then_rests() {
    using micrec::Phase;
    TEST_ASSERT_EQUAL_INT((int)Phase::Countdown, (int)micrec::stepAt(0).phase);
    TEST_ASSERT_EQUAL_INT(3, micrec::stepAt(0).count);
    TEST_ASSERT_EQUAL_INT(2, micrec::stepAt(1000).count);
    TEST_ASSERT_EQUAL_INT(1, micrec::stepAt(2999).count);
    TEST_ASSERT_EQUAL_INT((int)Phase::Record, (int)micrec::stepAt(3000).phase);
    TEST_ASSERT_EQUAL_INT((int)Phase::Record, (int)micrec::stepAt(3000 + micrec::MAX_MS - 1).phase);
    TEST_ASSERT_EQUAL_INT((int)Phase::Gap, (int)micrec::stepAt(3000 + micrec::MAX_MS).phase);
    TEST_ASSERT_EQUAL_INT((int)Phase::Done,
                          (int)micrec::stepAt(3000 + micrec::MAX_MS + micrec::GAP_MS).phase);
}

void test_a_run_stops_at_the_end_of_its_condition() {
    TEST_ASSERT_EQUAL_INT(10, micrec::runEnd(0));
    TEST_ASSERT_EQUAL_INT(10, micrec::runEnd(7));    // started mid-condition: finish it
    TEST_ASSERT_EQUAL_INT(10, micrec::runEnd(10));   // full: nothing to run
}

void test_screen_thirds_are_back_resume_skip() {
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Back, (int)micrec::tapAt(0));
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Back, (int)micrec::tapAt(105));
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Resume, (int)micrec::tapAt(106));
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Resume, (int)micrec::tapAt(213));
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Skip, (int)micrec::tapAt(214));
    TEST_ASSERT_EQUAL_INT((int)micrec::Tap::Skip, (int)micrec::tapAt(319));
}

void test_a_break_comes_before_every_condition_but_the_first() {
    TEST_ASSERT_FALSE(micrec::breakBefore(0));
    TEST_ASSERT_FALSE(micrec::breakBefore(5));
    TEST_ASSERT_FALSE(micrec::breakBefore(9));
    TEST_ASSERT_FALSE(micrec::breakBefore(10));   // one condition: never a break
}

void test_it_moves_to_the_hotspot_only_when_seen_and_not_already_there() {
    TEST_ASSERT_TRUE(micrec::moveToHotspot("Cecilia", "iPhone", true));
    TEST_ASSERT_FALSE(micrec::moveToHotspot("Cecilia", "iPhone", false));   // not around
    TEST_ASSERT_FALSE(micrec::moveToHotspot("iPhone", "iPhone", true));     // already there
    TEST_ASSERT_TRUE(micrec::moveToHotspot("", "iPhone", true));            // offline
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slots_map_to_condition_and_phrase);
    RUN_TEST(test_wav_header_is_16k_mono_pcm);
    RUN_TEST(test_short_taps_are_dropped_and_long_holds_capped);
    RUN_TEST(test_clip_names);
    RUN_TEST(test_a_phrase_counts_down_then_records_then_rests);
    RUN_TEST(test_a_run_stops_at_the_end_of_its_condition);
    RUN_TEST(test_screen_thirds_are_back_resume_skip);
    RUN_TEST(test_a_break_comes_before_every_condition_but_the_first);
    RUN_TEST(test_it_moves_to_the_hotspot_only_when_seen_and_not_already_there);
    return UNITY_END();
}
