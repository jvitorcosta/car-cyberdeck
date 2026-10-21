#pragma once
#include <cstddef>
#include <cstdint>
// Audio output for the greeting clips: board/s3/sink_es8311.cpp (ES8311 codec).
// Used by one playback task at a time (src/audio/greeting.cpp).
namespace audiosink {
extern const int GREETING_VOLUME;   // percent of the clip's level (<= 100)
extern const int FINALE_VOLUME;
bool open(int clipRate);                    // false: no audio this time (the sink logs why)
void write(const int16_t* s, size_t n);     // clip-rate samples, volume applied; blocks on a full DMA
void close();                               // plays out what's queued, then silence and release
}
