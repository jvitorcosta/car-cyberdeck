#include "mic_es8311.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "audioport.h"
#include "es8311.h"

namespace mic {

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;     // audioport's
constexpr int RATE = 16000;
constexpr size_t CHUNK = 256;              // stereo frames per i2s_read
constexpr uint8_t PGA_GAIN = 7;            // +42 dB, the spike's max
// ADC volume +24 dB, the spike's value. 0 dB (0xBF) was tried on the first car run (2026-10-23):
// every clip peaked at only ~160-355 of 32k, no louder than a silent room.
constexpr uint8_t ADC_VOLUME = 0xEF;
int16_t s_frames[2 * CHUNK];
bool s_held = false;                       // begin() holds the port: end() releases it once
}  // namespace

bool begin() {
    if (s_held) return true;
    if (!audioport::acquire(RATE)) return false;
    if (!es8311::enableMic(PGA_GAIN, ADC_VOLUME)) {
        Serial.println("[mic] adc setup FAILED");
        audioport::release();
        return false;
    }
    s_held = true;
    return true;
}

size_t read(int16_t* mono, size_t maxSamples) {
    if (maxSamples > CHUNK) maxSamples = CHUNK;
    size_t got = 0;
    i2s_read(PORT, s_frames, maxSamples * 2 * sizeof(int16_t), &got, portMAX_DELAY);
    const size_t n = got / (2 * sizeof(int16_t));
    for (size_t i = 0; i < n; i++) mono[i] = s_frames[2 * i];
    return n;
}

void end() {
    if (!s_held) return;                   // never begun, or begin() failed: nothing to release
    s_held = false;
    audioport::release();
}

}  // namespace mic
