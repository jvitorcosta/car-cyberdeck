#include "musicfeat.h"
#include <cmath>

namespace musicfeat {

namespace {
constexpr float PI_F = 3.14159265f;
float coef(float dt, float tau) { return 1.0f - expf(-dt / tau); }   // one-pole step for a hop of dt
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
uint8_t to255(float v) { return (uint8_t)(clampf(v, 0.0f, 255.0f) + 0.5f); }
}

// RBJ cookbook biquads, normalised by a0.
void Analyzer::bandPass(Biquad& q, float loHz, float hiHz) {
    const float f0 = sqrtf(loHz * hiHz), Q = f0 / (hiHz - loHz);
    const float w = 2.0f * PI_F * f0 / (float)RATE, alpha = sinf(w) / (2.0f * Q);
    const float a0 = 1.0f + alpha;
    q = {alpha / a0, 0.0f, -alpha / a0, -2.0f * cosf(w) / a0, (1.0f - alpha) / a0, 0.0f, 0.0f};
}
void Analyzer::highPass(Biquad& q, float hz) {
    const float w = 2.0f * PI_F * hz / (float)RATE, alpha = sinf(w) / (2.0f * 0.70710678f);
    const float c = cosf(w), a0 = 1.0f + alpha;
    q = {(1.0f + c) / 2.0f / a0, -(1.0f + c) / a0, (1.0f + c) / 2.0f / a0, -2.0f * c / a0, (1.0f - alpha) / a0, 0.0f, 0.0f};
}
float Analyzer::run(Biquad& q, float x) {   // transposed direct form II
    const float y = q.b0 * x + q.z1;
    q.z1 = q.b1 * x - q.a1 * y + q.z2;
    q.z2 = q.b2 * x - q.a2 * y;
    return y;
}

Analyzer::Analyzer()
    : sum_{0, 0, 0}, fill_(0), hops_(0), floor_{0, 0, 0}, prevBass_(0), flux_{}, fluxPos_(0), fluxPeak_(0),
      everBeat_(false), lastBeatMs_(0), anyBeat_(false), bestStrength_(0), eSmooth_(0), ePeak_(0),
      bright_(0), energy_(0), brightness_(0), presentRing_{}, presentPos_(0) {
    bandPass(bass_, BASS_LO_HZ, BASS_HI_HZ);
    bandPass(mid_, MID_LO_HZ, MID_HI_HZ);
    highPass(treble_, TREBLE_HZ);
}

void Analyzer::feed(const int16_t* s, size_t n, uint32_t nowMs) {
    for (size_t i = 0; i < n; ++i) {
        const float x = (float)s[i];
        const float b = run(bass_, x), m = run(mid_, x), t = run(treble_, x);
        sum_[0] += b * b;
        sum_[1] += m * m;
        sum_[2] += t * t;
        if (++fill_ < HOP) continue;
        // The hop ended after sample i; nowMs is the end of the block.
        hop(nowMs - (uint32_t)((n - 1 - i) * 1000u / (size_t)RATE));
        fill_ = 0;
        sum_[0] = sum_[1] = sum_[2] = 0.0f;
    }
}

void Analyzer::hop(uint32_t hopMs) {
    float p[3];
    for (int k = 0; k < 3; ++k) p[k] = sum_[k] / (float)HOP;
    if (hops_ < WARMUP_HOPS) {
        if (++hops_ == WARMUP_HOPS) {
            for (int k = 0; k < 3; ++k) floor_[k] = p[k];
            prevBass_ = p[0];
        }
        return;
    }

    // Beat: bass flux against its median over the last second.
    const float flux = p[0] > prevBass_ ? p[0] - prevBass_ : 0.0f;
    prevBass_ = p[0];
    float sorted[FLUX_RING];
    for (int k = 0; k < FLUX_RING; ++k) {
        const float v = flux_[k];
        int j = k;
        while (j > 0 && sorted[j - 1] > v) { sorted[j] = sorted[j - 1]; --j; }
        sorted[j] = v;
    }
    const float median = sorted[FLUX_RING / 2];
    flux_[fluxPos_] = flux;
    fluxPos_ = (fluxPos_ + 1) % FLUX_RING;
    fluxPeak_ *= 1.0f - coef(HOP_S, FLUX_PEAK_DECAY_S);
    if (flux > fluxPeak_) fluxPeak_ = flux;

    const bool refractory = everBeat_ && (uint32_t)(hopMs - lastBeatMs_) < REFRACTORY_MS;
    if (flux > BEAT_FLUX_X * median && p[0] > BEAT_POWER_X * floor_[0] && !refractory) {
        uint8_t st = to255(255.0f * flux / fluxPeak_);
        if (st == 0) st = 1;
        if (!anyBeat_ || st > bestStrength_) bestStrength_ = st;
        anyBeat_ = true;
        everBeat_ = true;
        lastBeatMs_ = hopMs;
    }
    const bool holding = everBeat_ && (uint32_t)(hopMs - lastBeatMs_) < FLOOR_HOLD_MS;

    // Floors: fast down, slow up, held while beats are coming.
    const float fall = coef(HOP_S, FLOOR_FALL_S), rise = coef(HOP_S, FLOOR_RISE_S);
    float over = 0.0f, floorSum = 0.0f;
    for (int k = 0; k < 3; ++k) {
        if (p[k] < floor_[k]) floor_[k] += (p[k] - floor_[k]) * fall;
        else if (!holding) floor_[k] += (p[k] - floor_[k]) * rise;
        floorSum += floor_[k];
        if (p[k] > floor_[k]) over += p[k] - floor_[k];
    }

    // Energy: power above the floor, attack/release smoothing, normalised by a rolling peak.
    eSmooth_ += (over - eSmooth_) * coef(HOP_S, over > eSmooth_ ? ENERGY_ATTACK_S : ENERGY_RELEASE_S);
    ePeak_ *= 1.0f - coef(HOP_S, ENERGY_PEAK_DECAY_S);
    if (eSmooth_ > ePeak_) ePeak_ = eSmooth_;
    const float peak = ePeak_ > ENERGY_PEAK_MIN_X * floorSum ? ePeak_ : ENERGY_PEAK_MIN_X * floorSum;
    energy_ = peak > 0.0f ? to255(255.0f * eSmooth_ / peak) : 0;

    // Brightness: treble against the rest, squashed to 0..1 (equal power = half).
    const float r = p[2] / (p[0] + p[1] + 1.0f);
    bright_ += (r / (1.0f + r) - bright_) * coef(HOP_S, BRIGHT_SMOOTH_S);
    brightness_ = to255(255.0f * bright_);

    presentRing_[presentPos_] = energy_ > PRESENT_ENERGY;
    presentPos_ = (presentPos_ + 1) % PRESENT_HOPS;
}

Features Analyzer::take() {
    int loud = 0;
    for (int k = 0; k < PRESENT_HOPS; ++k) loud += presentRing_[k] ? 1 : 0;
    const Features f{anyBeat_, anyBeat_ ? bestStrength_ : (uint8_t)0, energy_, brightness_, loud >= PRESENT_NEED_HOPS};
    anyBeat_ = false;
    bestStrength_ = 0;
    return f;
}

}
