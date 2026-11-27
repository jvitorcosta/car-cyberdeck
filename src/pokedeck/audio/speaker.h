#pragma once
#include <cstddef>
#include <cstdint>
// The Pokédeck's speaker: the board's NS4168 I2S amplifier (no codec, no enable pin), 16-bit mono
// PCM on both channels. One clip at a time, played on its own task.
namespace speaker {
// Starts a clip; the samples must stay in memory until wait() returns. False (logged): no sound.
bool play(const int16_t* samples, size_t count, int rate);
uint32_t startedAt();   // millis() when the running (or last) clip's first sample went out; 0: not yet
void wait();            // until the clip has played out; returns at once without one
}
