#pragma once
#include <cstddef>
#include <cstdint>
// A wolf howl, synthesised while it plays (a stored 16-bit clip would be ~80 KB of flash):
// rises from ~350 Hz to 700 Hz, holds with a growing vibrato, falls to 400 Hz, with soft
// attack and release. Pure: the clip task (audio/greeting) pulls samples in chunks.
namespace howl {
constexpr int RATE = 16000;                                   // audiosink clip rate
constexpr uint32_t DURATION_MS = 2300;
constexpr size_t SAMPLES = (size_t)RATE * DURATION_MS / 1000;

class Synth {
public:
    // Writes up to n samples; returns how many (0 once the howl is over).
    size_t fill(int16_t* out, size_t n);
private:
    size_t pos_ = 0;
    float phase_ = 0;   // radians, kept in [0, 2*pi)
};
}
