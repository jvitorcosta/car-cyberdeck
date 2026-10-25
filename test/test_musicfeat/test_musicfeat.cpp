#include <unity.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../../src/util/musicfeat.h"

void setUp() {}
void tearDown() {}

using musicfeat::Analyzer;
using musicfeat::Features;
using musicfeat::RATE;

static const double PI_D = 3.14159265358979323846;

// Deterministic white noise (xorshift32), uniform scaled to the given RMS.
struct Noise {
    uint32_t s = 0x12345678u;
    double next(double rms) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return ((double)s / 4294967295.0 * 2.0 - 1.0) * rms * 1.7320508;   // uniform +-a has RMS a/sqrt(3)
    }
};

static int16_t clip16(double v) { return (int16_t)(v > 32767.0 ? 32767 : (v < -32768.0 ? -32768 : v)); }

// The mic's hum floor: 60 Hz sine RMS 600 + white noise RMS 150.
static double hum(Noise& n, long i) { return 600.0 * 1.4142136 * sin(2.0 * PI_D * 60.0 * i / RATE) + n.next(150.0); }

// A 100 Hz burst of initial RMS `rms`, decaying over ~60 ms, starting at every `periodMs` from `firstMs`.
static double kick(long i, int periodMs, int firstMs, double rms) {
    const double ms = i * 1000.0 / RATE - firstMs;
    if (ms < 0) return 0.0;
    const double t = fmod(ms, (double)periodMs) / 1000.0;
    if (t >= 0.06) return 0.0;
    return rms * 1.4142136 * exp(-3.0 * t / 0.06) * sin(2.0 * PI_D * 100.0 * t);
}

struct Run {
    std::vector<Features> takes;   // one per 256-sample block
    int beatsIn(double fromS, double toS) const {
        int c = 0;
        for (size_t k = 0; k < takes.size(); ++k) {
            const double t = (double)(k + 1) * 256.0 / RATE;
            if (t > fromS && t <= toS && takes[k].beat) ++c;
        }
        return c;
    }
    int maxEnergyAfter(double fromS) const {
        int m = 0;
        for (size_t k = 0; k < takes.size(); ++k)
            if ((double)(k + 1) * 256.0 / RATE > fromS && takes[k].energy > m) m = takes[k].energy;
        return m;
    }
};

// Feed `seconds` of signal(i, noise) in 256-sample blocks, take() after each.
template <class F>
static Run run(double seconds, F signal) {
    Analyzer a;
    Noise n;
    Run r;
    const long total = (long)(seconds * RATE);
    int16_t blk[256];
    for (long base = 0; base + 256 <= total; base += 256) {
        for (int j = 0; j < 256; ++j) blk[j] = clip16(signal(base + j, n));
        a.feed(blk, 256, (uint32_t)((base + 256) * 1000L / RATE));
        r.takes.push_back(a.take());
    }
    return r;
}

void test_hum_only_is_silent() {
    const Run r = run(10.0, [](long i, Noise& n) { return hum(n, i); });
    TEST_ASSERT_EQUAL_INT(0, r.beatsIn(0.0, 10.0));
    for (size_t k = 0; k < r.takes.size(); ++k)
        if ((double)(k + 1) * 256.0 / RATE > 1.0) TEST_ASSERT_FALSE(r.takes[k].present);
}

// Hum that steps up (engine revs, a louder fan) is not music: after a few seconds of settling it must
// neither beat nor read as present. `gain` multiplies the whole hum from 3 s on.
static void check_hum_step(double gain) {
    const Run r = run(20.0, [gain](long i, Noise& n) { return hum(n, i) * (i >= 3 * RATE ? gain : 1.0); });
    TEST_ASSERT_EQUAL_INT(0, r.beatsIn(14.0, 20.0));
    for (size_t k = 0; k < r.takes.size(); ++k)
        if ((double)(k + 1) * 256.0 / RATE > 14.0) TEST_ASSERT_FALSE(r.takes[k].present);
}
void test_hum_step_up_settles_to_silent() {
    check_hum_step(1.2);
    check_hum_step(1.5);
}

void test_kicks_over_hum_beat_at_tempo() {
    const Run r = run(10.0, [](long i, Noise& n) { return hum(n, i) + kick(i, 500, 500, 2000.0); });
    const int beats = r.beatsIn(3.0, 10.0);
    TEST_ASSERT_TRUE_MESSAGE(beats >= 14 && beats <= 18, "beats in seconds 3-10 should be about 2 per second");
    TEST_ASSERT_TRUE(r.takes.back().present);
}

void test_refractory() {
    const Run r = run(6.0, [](long i, Noise& n) { return hum(n, i) + kick(i, 100, 500, 2000.0); });
    for (int sec = 0; sec < 5; ++sec) TEST_ASSERT_TRUE(r.beatsIn(sec + 1.0 - 1e-9, sec + 2.0) <= 4);
    TEST_ASSERT_TRUE(r.beatsIn(1.0, 6.0) > 0);   // it does beat, just not faster than the refractory
}

void test_energy_normalises() {
    const Run quiet = run(6.0, [](long i, Noise& n) { return hum(n, i) + kick(i, 500, 500, 2000.0); });
    const Run loud = run(6.0, [](long i, Noise& n) { return hum(n, i) + kick(i, 500, 500, 8000.0); });
    TEST_ASSERT_TRUE(quiet.maxEnergyAfter(3.0) >= 150);
    TEST_ASSERT_TRUE(loud.maxEnergyAfter(3.0) >= 150);
    // Kicks hard-clipped to full scale: the energy must still saturate at the top, not wrap round to a low value.
    const Run clipped = run(6.0, [](long i, Noise& n) { return hum(n, i) + kick(i, 500, 500, 30000.0); });
    for (int sec = 1; sec < 6; ++sec) {
        int m = 0;
        for (size_t k = 0; k < clipped.takes.size(); ++k) {
            const double t = (double)(k + 1) * 256.0 / RATE;
            if (t > sec && t <= sec + 1 && clipped.takes[k].energy > m) m = clipped.takes[k].energy;
        }
        TEST_ASSERT_TRUE_MESSAGE(m >= 200, "clipped kicks should read near full energy in every second");
    }
}

void test_brightness_orders() {
    const double amp = 2000.0 * 1.4142136;
    const Run hi = run(3.0, [amp](long i, Noise&) { return amp * sin(2.0 * PI_D * 5000.0 * i / RATE); });
    const Run lo = run(3.0, [amp](long i, Noise&) { return amp * sin(2.0 * PI_D * 150.0 * i / RATE); });
    TEST_ASSERT_TRUE(hi.takes.back().brightness > lo.takes.back().brightness);
    TEST_ASSERT_TRUE(hi.takes.back().brightness > 128);
    TEST_ASSERT_TRUE(lo.takes.back().brightness < 64);
}

void test_block_size_does_not_matter() {
    // The same signal in 100-sample blocks still beats (hop boundaries fall mid-block).
    Analyzer a;
    Noise n;
    int beats = 0;
    int16_t blk[100];
    for (long base = 0; base + 100 <= 10 * RATE; base += 100) {
        for (int j = 0; j < 100; ++j) blk[j] = clip16(hum(n, base + j) + kick(base + j, 500, 500, 2000.0));
        a.feed(blk, 100, (uint32_t)((base + 100) * 1000L / RATE));
        if (a.take().beat && base > 3 * RATE) ++beats;
    }
    TEST_ASSERT_TRUE(beats >= 14 && beats <= 18);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hum_only_is_silent);
    RUN_TEST(test_hum_step_up_settles_to_silent);
    RUN_TEST(test_kicks_over_hum_beat_at_tempo);
    RUN_TEST(test_refractory);
    RUN_TEST(test_energy_normalises);
    RUN_TEST(test_brightness_orders);
    RUN_TEST(test_block_size_does_not_matter);
    return UNITY_END();
}
