#include <unity.h>
#include <cstdlib>
#include <vector>
#include "../../src/util/black/howl.h"

// The howl is synthesised while it plays (no stored clip). What a listener and the speaker
// rely on: it lasts the advertised time, rises then falls like a howl, never clips, and starts
// and ends at silence (a jump there is an audible click).

void setUp() {}
void tearDown() {}

static std::vector<int16_t> render(size_t chunk) {
    std::vector<int16_t> out(howl::SAMPLES);
    howl::Synth s;
    for (size_t i = 0; i < out.size(); i += chunk) {
        size_t n = out.size() - i < chunk ? out.size() - i : chunk;
        TEST_ASSERT_EQUAL_size_t(n, s.fill(out.data() + i, n));
    }
    int16_t extra[8];
    TEST_ASSERT_EQUAL_size_t(0, s.fill(extra, 8));   // nothing past the end
    return out;
}

// Pitch over [a, b) seconds: rising zero crossings per second (one per period).
static double pitch(const std::vector<int16_t>& v, double a, double b) {
    size_t i0 = (size_t)(a * howl::RATE), i1 = (size_t)(b * howl::RATE);
    int up = 0;
    for (size_t i = i0 + 1; i < i1; ++i) up += v[i - 1] < 0 && v[i] >= 0;
    return up / (b - a);
}

void test_lasts_the_advertised_time() {
    TEST_ASSERT_EQUAL_UINT32(howl::DURATION_MS, (uint32_t)(howl::SAMPLES * 1000 / howl::RATE));
    TEST_ASSERT_TRUE(howl::DURATION_MS >= 1500 && howl::DURATION_MS <= 3000);
    render(128);
}
void test_pitch_rises_then_falls() {
    auto v = render(128);
    double start = pitch(v, 0.10, 0.30), peak = pitch(v, 1.00, 1.40), tail = pitch(v, 1.90, 2.10);
    TEST_ASSERT_TRUE_MESSAGE(start > 250 && start < peak, "rises");
    TEST_ASSERT_TRUE_MESSAGE(peak > 600 && peak < 800, "peak near 700 Hz");
    TEST_ASSERT_TRUE_MESSAGE(tail < peak - 100, "falls at the end");
}
void test_never_clips_and_is_loud_enough() {
    auto v = render(128);
    int peak = 0;
    for (int16_t s : v) peak = abs(s) > peak ? abs(s) : peak;
    TEST_ASSERT_TRUE(peak <= 32767 * 9 / 10);
    TEST_ASSERT_TRUE(peak >= 32767 / 2);
}
void test_starts_and_ends_at_silence() {
    auto v = render(128);
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_TRUE(abs(v[i]) < 330);                  // < 1 % of full scale
        TEST_ASSERT_TRUE(abs(v[v.size() - 1 - i]) < 330);
    }
}
void test_chunk_size_doesnt_change_the_sound() {
    auto a = render(128), b = render(37);
    TEST_ASSERT_EQUAL_INT16_ARRAY(a.data(), b.data(), a.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_lasts_the_advertised_time);
    RUN_TEST(test_pitch_rises_then_falls);
    RUN_TEST(test_never_clips_and_is_loud_enough);
    RUN_TEST(test_starts_and_ends_at_silence);
    RUN_TEST(test_chunk_size_doesnt_change_the_sound);
    return UNITY_END();
}
