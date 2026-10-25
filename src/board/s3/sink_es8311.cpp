#include "../../audio/sink.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "audioport.h"
#include "es8311.h"
#include "pins.h"

namespace audiosink {

// Clean codec path (no 8-bit DAC distortion): conservative start, tuned by ear (spec section 3).
extern const int GREETING_VOLUME = 100;
extern const int FINALE_VOLUME = 100;

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;     // audioport's
constexpr int DMA_BUFS = 4;                // audioport's DMA, for the play-out wait in close()
constexpr int DMA_LEN = 256;
constexpr int CHUNK = 128;                 // mono samples per i2s_write
// Codec DAC gain (0xBF = 0 dB, 0.5 dB steps). The clips peak near full scale, so each dB above
// 0 shaves the loudest peaks; +6 dB was the owner's pick by ear after 100 % volume.
constexpr uint8_t CODEC_GAIN = 0xCB;
int s_rate = 16000;
int16_t s_buf[2 * CHUNK];
}

bool open(int clipRate) {
    // Shares the port with the mic at 16 kHz; a 48 kHz clip while the mic listens is refused.
    if (!audioport::acquire(clipRate)) return false;
    s_rate = clipRate;
    if (!es8311::setVolume(CODEC_GAIN)) {
        Serial.println("[audio] ES8311 not answering");
        audioport::release();
        return false;
    }
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, AMP_ON);      // only now: codec up, I2S sending silence (no pop)
    return true;
}

void write(const int16_t* s, size_t n) {
    while (n) {
        const size_t k = n < (size_t)CHUNK ? n : (size_t)CHUNK;
        for (size_t i = 0; i < k; ++i) s_buf[2 * i] = s_buf[2 * i + 1] = s[i];   // mono on both channels
        size_t written;
        i2s_write(PORT, s_buf, k * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        s += k;
        n -= k;
    }
}

void close() {
    const uint32_t dmaMs = 1000u * DMA_BUFS * DMA_LEN / (uint32_t)s_rate;
    vTaskDelay(pdMS_TO_TICKS(dmaMs + 20));  // let the queued DMA buffers play out
    digitalWrite(PIN_AMP_EN, !AMP_ON);
    audioport::release();                   // uninstalls unless the mic still listens
}

}
