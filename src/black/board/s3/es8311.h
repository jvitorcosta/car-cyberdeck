#pragma once
#include <cstdint>
// Minimal ES8311 setup for playback and the mic: I2S slave, 16-bit, MCLK from its MCLK pin. Uses Arduino
// Wire (begun by the caller). Register sequence from Espressif's driver (see es8311.cpp).
namespace es8311 {
constexpr uint8_t ADDR = 0x18;
bool begin(int rate, int mclkHz);   // false: no answer on I2C, or no clock row for rate/mclk
bool setVolume(uint8_t reg32);      // DAC volume: 0xBF = 0 dB, 0.5 dB steps
// The ADC side, after begin(): analog mic + PGA on, gain (0..7 = 0..42 dB in 6 dB steps), ADC
// volume (0xBF = 0 dB, 0.5 dB steps), HPF and DC cancel. Leaves the DAC as it is.
bool enableMic(uint8_t gain, uint8_t adcVol);
}
