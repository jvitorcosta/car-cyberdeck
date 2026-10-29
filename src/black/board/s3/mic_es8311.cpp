#include "mic_es8311.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <cmath>
#include "audioport.h"
#include "es8311.h"
#include "pins.h"

namespace mic {

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;     // audioport's
constexpr size_t CHUNK = 256;              // stereo frames per i2s_read
// ADC volume +24 dB, the spike's value. 0 dB (0xBF) was tried on the first car run (2026-10-23):
// every clip peaked at only ~160-355 of 32k, no louder than a silent room.
constexpr uint8_t ADC_VOLUME = 0xEF;
int16_t s_frames[2 * CHUNK];
bool s_held = false;                       // begin() holds the port: end() releases it once
uint8_t s_gain = 7;                        // PGA +42 dB, the spike's max
uint8_t s_adcVol = ADC_VOLUME;
bool s_alc = false;
}  // namespace

bool begin() {
    if (s_held) return true;
    if (!audioport::acquire(RATE)) return false;
    if (!es8311::enableMic(s_gain, s_adcVol) || (s_alc && !es8311::setAlc(true))) {
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

void setGain(uint8_t reg16) {
    s_gain = reg16 > 7 ? 7 : reg16;
    if (s_held) es8311::setMicGain(s_gain);
}

uint8_t gain() { return s_gain; }

void setAdcVolume(uint8_t reg17) {
    s_adcVol = reg17;
    if (s_held) es8311::setAdcVolume(s_adcVol);
}

uint8_t adcVolume() { return s_adcVol; }

void setAlc(bool on) {
    s_alc = on;
    if (s_held) es8311::setAlc(on);
}

bool alc() { return s_alc; }

size_t readStereo(int16_t* lr, size_t maxFrames) {
    if (maxFrames > CHUNK) maxFrames = CHUNK;
    size_t got = 0;
    i2s_read(PORT, lr, maxFrames * 2 * sizeof(int16_t), &got, portMAX_DELAY);
    return got / (2 * sizeof(int16_t));
}

void beep(int ms, int hz) {
    if (!s_held) return;
    es8311::setVolume(0xBF);               // DAC at 0 dB
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, AMP_ON);
    const int total = RATE * ms / 1000;
    for (int n = 0; n < total; n += CHUNK) {
        const int m = total - n < (int)CHUNK ? total - n : (int)CHUNK;
        for (int i = 0; i < m; i++) {
            const int16_t v = (int16_t)(sinf(6.2831853f * (float)hz * (float)(n + i) / (float)RATE) * 8000.0f);
            s_frames[2 * i] = s_frames[2 * i + 1] = v;
        }
        size_t wrote = 0;
        i2s_write(PORT, s_frames, (size_t)m * 2 * sizeof(int16_t), &wrote, portMAX_DELAY);
    }
    delay(80);                             // let the DMA play out before the amp goes off
    digitalWrite(PIN_AMP_EN, !AMP_ON);
}

void flush() {
    if (!s_held) return;
    size_t got = 0;
    for (int i = 0; i < 4; i++) i2s_read(PORT, s_frames, sizeof s_frames, &got, portMAX_DELAY);
}

}  // namespace mic
