#pragma once
#include <cstddef>
#include <cstdint>
// Audio output for the greeting clips: board/s3/sink_es8311.cpp (ES8311 codec).
// Used by one playback task at a time (src/black/audio/greeting.cpp).
namespace audiosink {
extern const int GREETING_VOLUME;   // percent of the clip's level (<= 100)
extern const int FINALE_VOLUME;
bool open(int clipRate);                    // false: no audio this time (the sink logs why)
void write(const int16_t* s, size_t n);     // clip-rate samples, volume applied; blocks on a full DMA
void close();                               // plays out what's queued, then silence and release
// The output is still on from the last clip at this rate (a 16 kHz clip keeps it on for a while
// after), so the next open() needs no wake: its sound is out at once, not ~0.45 s later.
bool warm(int clipRate);
}
