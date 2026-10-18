#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "../../src/util/musicbox.h"

void setUp() {}
void tearDown() {}

using musicbox::RATE;

static std::vector<int16_t> run(musicbox::Player& p, size_t samples) {
    std::vector<int16_t> out(samples);
    size_t got = 0;
    while (got < samples) {
        size_t n = p.fill(out.data() + got, samples - got > 128 ? 128 : samples - got);
        if (n == 0) break;
        got += n;
    }
    out.resize(got);
    return out;
}

static double rms(const std::vector<int16_t>& v, size_t a, size_t b) {
    double s = 0;
    for (size_t i = a; i < b; ++i) s += (double)v[i] * v[i];
    return std::sqrt(s / (double)(b - a));
}

static const char A440[] = "tune A\nA4 1000\n";

void test_freq() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 440.0f, musicbox::freq(69));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 261.63f, musicbox::freq(60));
}

// Random order, never the same tune twice in a row.
void test_next_tune_never_repeats() {
    bool seen[4] = {};
    int last = 0;
    for (uint32_t r = 0; r < 200; ++r) {
        int t = musicbox::nextTune(4, last, r * 2654435761u);
        TEST_ASSERT_TRUE(t >= 0 && t < 4);
        TEST_ASSERT_NOT_EQUAL(last, t);
        seen[t] = true;
        last = t;
    }
    for (bool s : seen) TEST_ASSERT_TRUE(s);
    TEST_ASSERT_EQUAL_INT(0, musicbox::nextTune(1, 0, 12345));
    TEST_ASSERT_EQUAL_INT(0, musicbox::nextTune(3, -1, 0));   // the first pick
}

// A4 sounds at 440 Hz and rings down like a plucked comb.
void test_note_pitch_and_decay() {
    musicbox::Player p(A440, sizeof A440 - 1, 1);
    std::vector<int16_t> v = run(p, RATE);
    TEST_ASSERT_EQUAL_UINT32(RATE, v.size());
    int crossings = 0;
    for (size_t i = RATE / 10; i < RATE / 10 + RATE / 2; ++i)
        crossings += (v[i - 1] < 0) != (v[i] < 0);
    TEST_ASSERT_INT_WITHIN(20, 440, crossings);       // 2 per period over 0.5 s
    TEST_ASSERT_TRUE(rms(v, 0, RATE / 10) > 2 * rms(v, RATE * 9 / 10, RATE));
    TEST_ASSERT_TRUE(rms(v, 0, RATE / 10) > 2000);    // audible
}

// No click: the note starts from silence.
void test_soft_attack() {
    musicbox::Player p(A440, sizeof A440 - 1, 1);
    std::vector<int16_t> v = run(p, 8);
    for (int16_t s : v) TEST_ASSERT_TRUE(std::abs(s) < 3000);
}

// It loops: after a tune and the rest between tunes, the next pass sounds again.
void test_loops_with_a_rest_between() {
    musicbox::Player p(A440, sizeof A440 - 1, 1);
    const size_t pass = RATE + (size_t)RATE * musicbox::REST_MS / 1000;
    std::vector<int16_t> v = run(p, 3 * pass);
    TEST_ASSERT_EQUAL_UINT32(3 * pass, v.size());   // never ends by itself
    TEST_ASSERT_TRUE(rms(v, pass - RATE / 10, pass) < 300);     // quiet at the end of the rest
    TEST_ASSERT_TRUE(rms(v, pass, pass + RATE / 10) > 2000);    // the next pass
}

// stop() fades out quickly, then fill() returns 0.
void test_stop_fades_out_then_ends() {
    musicbox::Player p(A440, sizeof A440 - 1, 1);
    run(p, RATE / 10);
    p.stop();
    std::vector<int16_t> v = run(p, RATE);
    const size_t fade = (size_t)RATE * musicbox::FADE_MS / 1000;
    TEST_ASSERT_TRUE(v.size() >= fade && v.size() <= fade + 128);
    TEST_ASSERT_TRUE(rms(v, v.size() - 64, v.size()) < rms(v, 0, 64) / 4);
    int16_t buf[16];
    TEST_ASSERT_EQUAL_UINT32(0, p.fill(buf, 16));
}

void test_no_tunes_plays_nothing() {
    musicbox::Player p("", 0, 1);
    int16_t buf[16];
    TEST_ASSERT_EQUAL_UINT32(0, p.fill(buf, 16));
}

// Fast notes stacking up on ringing voices never overflow.
void test_fast_notes_never_clip() {
    static const char T[] = "tune Fast\nC6 20 E6 20 G6 20 C7 20 C6 20 E6 20 G6 20 C7 20 C2 20 C2 20\n";
    musicbox::Player p(T, sizeof T - 1, 1);
    std::vector<int16_t> v = run(p, RATE);
    int peak = 0;
    for (int16_t s : v) peak = std::abs(s) > peak ? std::abs(s) : peak;
    TEST_ASSERT_TRUE(peak < 32000);
    TEST_ASSERT_TRUE(peak > 4000);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_freq);
    RUN_TEST(test_next_tune_never_repeats);
    RUN_TEST(test_note_pitch_and_decay);
    RUN_TEST(test_soft_attack);
    RUN_TEST(test_loops_with_a_rest_between);
    RUN_TEST(test_stop_fades_out_then_ends);
    RUN_TEST(test_no_tunes_plays_nothing);
    RUN_TEST(test_fast_notes_never_clip);
    return UNITY_END();
}
