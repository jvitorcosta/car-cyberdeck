#pragma once
#include <cstddef>
#include <cstdint>
// The black deck's mic through the ES8311 ADC, on the shared I2S port (audioport) at 16 kHz.
// Register values from the 2026-10-22 mic spike. Can run while audiosink plays a 16 kHz clip;
// a 48 kHz clip is refused while the mic holds the port. One user at a time (music mode's capture
// task): it calls begin, read and end itself, so end() never runs while a read() is blocked.
namespace mic {
bool begin();                                  // false: port busy at another rate, or the codec failed (logged)
size_t read(int16_t* mono, size_t maxSamples); // blocks; the left channel only, <= 256 samples
void end();                                    // idempotent; a no-op after a failed begin()
}
