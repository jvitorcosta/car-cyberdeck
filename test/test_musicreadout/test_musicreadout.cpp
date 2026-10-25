#include <unity.h>
#include "../../src/util/musicreadout.h"

using namespace musicreadout;

void setUp() {}
void tearDown() {}

// Packets every 30 ms from t0 for ms, a beat every `every` ms (the flag on one packet per beat).
static uint32_t feed(Beats& b, uint32_t t0, uint32_t ms, uint32_t every, uint16_t* seq) {
    uint32_t t = t0;
    for (; t < t0 + ms; t += 30) b.onPacket(t, (*seq)++, every && (t - t0) % every < 30);
    return t;
}

void test_no_rate_before_min() {
    Beats b;
    TEST_ASSERT_EQUAL_INT(-1, b.bpm(1000));
    uint16_t seq = 0;
    const uint32_t t = feed(b, 1000, 2900, 500, &seq);
    TEST_ASSERT_EQUAL_INT(-1, b.bpm(t));
}

// 120 BPM read as 120 while the window fills (scaled by the time listened), and once it's full.
void test_scaled_while_filling() {
    Beats b;
    uint16_t seq = 0;
    uint32_t t = feed(b, 1000, 3000, 500, &seq);
    TEST_ASSERT_INT_WITHIN(4, 120, b.bpm(t));
    t = feed(b, t, 5000, 500, &seq);
    TEST_ASSERT_INT_WITHIN(4, 120, b.bpm(t));
    t = feed(b, t, 20000, 500, &seq);  // past the window: the old beats drop out
    TEST_ASSERT_INT_WITHIN(4, 120, b.bpm(t));
}

void test_tempo_change_and_silence() {
    Beats b;
    uint16_t seq = 0;
    uint32_t t = feed(b, 0, 16000, 500, &seq);
    t = feed(b, t, 15000, 1000, &seq);  // 60 BPM for a whole window
    TEST_ASSERT_INT_WITHIN(4, 60, b.bpm(t));
    t = feed(b, t, 15000, 0, &seq);  // packets but no beats
    TEST_ASSERT_EQUAL_INT(0, b.bpm(t));
}

// A held flag (two packets in a row) or a repeated packet is one beat.
void test_edges_and_repeats() {
    Beats b;
    b.onPacket(0, 1, false);
    b.onPacket(30, 2, true);
    b.onPacket(60, 3, true);   // still high: no new beat
    b.onPacket(60, 3, true);   // the same packet again
    b.onPacket(90, 4, false);
    b.onPacket(120, 4, true);  // a repeated seq never counts
    b.onPacket(150, 5, false);
    b.onPacket(180, 6, true);
    b.onPacket(3000, 7, false);
    // 2 beats in 3 s
    TEST_ASSERT_EQUAL_INT(40, b.bpm(3000));
}

// A gap of SIGNAL_MS without packets starts the count over (no beats from before it, no undercount).
void test_gap_starts_over() {
    Beats b;
    uint16_t seq = 0;
    uint32_t t = feed(b, 0, 10000, 250, &seq);  // 240 BPM
    t += SIGNAL_MS;
    t = feed(b, t, 4000, 1000, &seq);
    TEST_ASSERT_INT_WITHIN(4, 60, b.bpm(t));
    b.reset();
    TEST_ASSERT_EQUAL_INT(-1, b.bpm(t));
}

// The ring never holds more than its beats; a flood still reads as a rate, bounded.
void test_ring_bounded() {
    Beats b;
    uint16_t seq = 0;
    for (uint32_t t = 0; t < 15000; t += 60) {
        b.onPacket(t, seq++, true);
        b.onPacket(t + 30, seq++, false);
    }
    TEST_ASSERT_TRUE(b.bpm(15000) <= RING * 60000 / 15000 + 1);
}

void test_state() {
    TEST_ASSERT_EQUAL_INT((int)State::NoSignal, (int)state(false, 0, true, 5000));
    TEST_ASSERT_EQUAL_INT((int)State::Music, (int)state(true, 4000, true, 5000));
    TEST_ASSERT_EQUAL_INT((int)State::Listening, (int)state(true, 4000, false, 5000));
    TEST_ASSERT_EQUAL_INT((int)State::Music, (int)state(true, 2001, true, 5000));
    TEST_ASSERT_EQUAL_INT((int)State::NoSignal, (int)state(true, 2000, true, 5000));   // 3 s without a packet
    TEST_ASSERT_EQUAL_INT((int)State::NoSignal, (int)state(true, 1000, false, 5000));
    TEST_ASSERT_EQUAL_INT((int)State::Music, (int)state(true, 0xFFFFFF00u, true, 100));  // across millis() wrap
}

void test_level_rounds() {
    TEST_ASSERT_EQUAL_INT(0, level(0));
    TEST_ASSERT_EQUAL_INT(100, level(255));
    TEST_ASSERT_EQUAL_INT(70, level(178));   // 69.8
    TEST_ASSERT_EQUAL_INT(50, level(128));   // 50.2
    TEST_ASSERT_EQUAL_INT(1, level(2));      // 0.78
    TEST_ASSERT_EQUAL_INT(0, level(1));      // 0.39
}

void test_format() {
    char s[32];
    format(State::Music, 120, 178, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(">MUSIC 120 BPM  LVL 70%", s);
    format(State::Music, -1, 255, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(">MUSIC -- BPM  LVL 100%", s);
    format(State::Listening, 120, 10, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(">LISTENING", s);
    format(State::NoSignal, 120, 10, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(">NO SIGNAL", s);
    char tiny[8];
    format(State::Music, 999, 255, tiny, sizeof tiny);
    TEST_ASSERT_EQUAL_STRING(">MUSIC ", tiny);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_no_rate_before_min);
    RUN_TEST(test_scaled_while_filling);
    RUN_TEST(test_tempo_change_and_silence);
    RUN_TEST(test_edges_and_repeats);
    RUN_TEST(test_gap_starts_over);
    RUN_TEST(test_ring_bounded);
    RUN_TEST(test_state);
    RUN_TEST(test_level_rounds);
    RUN_TEST(test_format);
    return UNITY_END();
}
