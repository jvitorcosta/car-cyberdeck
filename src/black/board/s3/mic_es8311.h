#pragma once
#include <cstddef>
#include <cstdint>
// The black deck's mic through the ES8311 ADC, on the shared I2S port (audioport) at 16 kHz.
// Register values from the 2026-10-22 mic spike. Can run while audiosink plays a 16 kHz clip;
// a 48 kHz clip is refused while the mic holds the port. One user at a time (music mode's capture
// task): it calls begin, read and end itself, so end() never runs while a read() is blocked.
namespace mic {
constexpr int RATE = 16000;
bool begin();                                  // false: port busy at another rate, or the codec failed (logged)
size_t read(int16_t* mono, size_t maxSamples); // blocks; the left channel only, <= 256 samples
void end();                                    // idempotent; a no-op after a failed begin()

// For the mic diag firmwares (src/diag/micrec_s3, micmeter_s3); the deck firmware doesn't call
// these. The settings hold across end()/begin(); setting one before begin() only stores it.
void setGain(uint8_t reg16);                   // mic PGA, 0..7 = 0..42 dB in 6 dB steps
uint8_t gain();
void setAdcVolume(uint8_t reg17);              // 0xBF = 0 dB, 0.5 dB steps (0xEF = +24 dB)
uint8_t adcVolume();
void setAlc(bool on);                          // the codec's automatic level control (untested)
bool alc();
// Both channels as they come (L, R, L, R...): for checking which one carries the mic.
size_t readStereo(int16_t* lr, size_t maxFrames);
// A short tone through the speaker on the held port (the "speak now" cue); blocks for ms.
// Only while nothing else plays (audiosink): it writes the port's TX directly.
void beep(int ms, int hz);
// Throws away what sat in the RX DMA buffers (e.g. the beep), so a take starts clean.
void flush();
}
