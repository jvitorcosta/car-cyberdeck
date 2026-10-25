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
    uint32_t s;
    explicit Noise(uint32_t seed = 0x12345678u) : s(seed) {}
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
static Run run(double seconds, F signal, uint32_t seed = 0x12345678u) {
    Analyzer a;
    Noise n(seed);
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
static void check_hum_step(double gain, uint32_t seed) {
    const Run r = run(25.0, [gain](long i, Noise& n) { return hum(n, i) * (i >= 3 * RATE ? gain : 1.0); }, seed);
    TEST_ASSERT_EQUAL_INT(0, r.beatsIn(14.0, 25.0));
    for (size_t k = 0; k < r.takes.size(); ++k)
        if ((double)(k + 1) * 256.0 / RATE > 14.0) TEST_ASSERT_FALSE(r.takes[k].present);
}
void test_hum_step_up_settles_to_silent() {
    const uint32_t seeds[] = {0x12345678u, 0xdeadbeefu, 0x1badf00du};
    const double gains[] = {1.2, 1.5, 2.0, 3.0};   // up to ~10 dB: an engine rev or a fan stepping up
    for (double g : gains)
        for (uint32_t sd : seeds) check_hum_step(g, sd);
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

// The readout's floor: 0 before the warm-up, then near the hum's band power; other bands read 0.
static void test_floor_readout() {
    Analyzer a;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.floorPower(0));
    Noise n;
    int16_t buf[256];
    long i = 0;
    for (int b = 0; b < 3 * RATE / 256; ++b) {
        for (int k = 0; k < 256; ++k, ++i) buf[k] = clip16(hum(n, i));
        a.feed(buf, 256, (uint32_t)(i * 1000 / RATE));
    }
    TEST_ASSERT_TRUE(a.floorPower(0) > 0.0f);
    TEST_ASSERT_TRUE(a.floorPower(1) > 0.0f);
    TEST_ASSERT_TRUE(a.floorPower(0) < 600.0f * 600.0f);   // a band of the hum, not all of it
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.floorPower(3));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.floorPower(-1));
}

// Talking in the car: noise band-limited to 300-2000 Hz (two high-pass + two low-pass biquads), in
// ~4 Hz syllables with a 0.5 s pause every 2 s, at twice the hum's RMS. It has no kick, but its onsets
// leak into the bass band through the band-pass's 2nd-order skirt: 0-3 beats per 12 s across the seeds
// (<= 15/min, far below a song's tempo). Pinned at that, not 0 (a "bass rose more than the mid" gate
// stopped some of them but also lost the kicks over hum). Energy and present rise too (a known
// caveat; printed, not pinned: present in every take after 4 s, energy 255).
struct Bq {
    double b0, b1, b2, a1, a2, z1, z2;
    double step(double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
static Bq butter(double hz, bool high) {   // RBJ Butterworth (Q = 0.7071) low- or high-pass
    const double w = 2.0 * PI_D * hz / RATE, al = sin(w) / (2.0 * 0.70710678), c = cos(w), a0 = 1.0 + al;
    const double b0 = high ? (1.0 + c) / 2.0 : (1.0 - c) / 2.0, b1 = high ? -(1.0 + c) : 1.0 - c;
    return Bq{b0 / a0, b1 / a0, b0 / a0, -2.0 * c / a0, (1.0 - al) / a0, 0.0, 0.0};
}
static Run speech(double seconds, uint32_t seed) {
    const long total = (long)(seconds * RATE);
    std::vector<double> v((size_t)total);
    Bq f[4] = {butter(300.0, true), butter(300.0, true), butter(2000.0, false), butter(2000.0, false)};
    Noise src(seed ^ 0xc0ffee11u);
    double sum = 0.0;
    for (long i = 0; i < total; ++i) {
        const double t = (double)i / RATE;
        double x = src.next(1000.0);
        for (Bq& q : f) x = q.step(x);
        const double env = fmod(t, 2.0) >= 1.5 ? 0.0 : pow(sin(PI_D * fmod(t, 0.25) / 0.25), 2.0);
        v[(size_t)i] = x * env;
        sum += v[(size_t)i] * v[(size_t)i];
    }
    const double humRms = sqrt(600.0 * 600.0 + 150.0 * 150.0);
    const double scale = 2.0 * humRms / sqrt(sum / (double)total);
    return run(seconds, [&](long i, Noise& n) { return hum(n, i) + v[(size_t)i] * scale; }, seed);
}
static void test_speech_rarely_beats() {
    const uint32_t seeds[] = {0x12345678u, 0xdeadbeefu, 0x1badf00du, 0x0badcafeu};
    for (uint32_t sd : seeds) {
        const Run r = speech(12.0, sd);
        TEST_ASSERT_TRUE_MESSAGE(r.beatsIn(0.0, 12.0) <= 3, "speech should beat at most 3 times in 12 s");
    }
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
    RUN_TEST(test_floor_readout);
    RUN_TEST(test_speech_rarely_beats);
    return UNITY_END();
}
