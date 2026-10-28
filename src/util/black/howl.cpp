#include "howl.h"
#include <cmath>

namespace howl {
namespace {
constexpr float PI = 3.14159265f;
constexpr float RISE_END = 0.9f, HOLD_END = 1.6f, END = DURATION_MS / 1000.0f;
constexpr float ATTACK = 0.12f, RELEASE = 0.5f;
constexpr float AMP = 0.62f * 32767;   // peak of the 3-harmonic wave stays under 90 % full scale

float pitchHz(float t) {
    float f;
    if (t < RISE_END) { float x = 1 - t / RISE_END; f = 350 + 350 * (1 - x * x); }   // eases into 700
    else if (t < HOLD_END) f = 700;
    else f = 700 - 300 * (t - HOLD_END) / (END - HOLD_END);                           // falls to 400
    float depth = t < 0.5f ? 0 : (t < 0.9f ? (t - 0.5f) / 0.4f : 1) * 12;             // vibrato fades in
    return f + depth * sinf(2 * PI * 5.5f * t);
}

float envelope(float t) {
    if (t < ATTACK) { float s = sinf(PI / 2 * t / ATTACK); return s * s; }
    if (t > END - RELEASE) { float c = cosf(PI / 2 * (t - (END - RELEASE)) / RELEASE); return c > 0 ? c * c : 0; }
    return 1;
}
}

size_t Synth::fill(int16_t* out, size_t n) {
    size_t i = 0;
    for (; i < n && pos_ < SAMPLES; ++i, ++pos_) {
        const float t = (float)pos_ / RATE;
        const float p = phase_;
        const float wave = sinf(p) + 0.25f * sinf(2 * p) + 0.1f * sinf(3 * p);   // a little reed
        out[i] = (int16_t)lrintf(AMP * envelope(t) * wave);
        phase_ += 2 * PI * pitchHz(t) / RATE;
        if (phase_ >= 2 * PI) phase_ -= 2 * PI;
    }
    return i;
}
}
