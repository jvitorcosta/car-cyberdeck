#include "../../audio/sink.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_timer.h>
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
// From cold (codec just reset, amp just enabled) the speaker stays near silent for ~0.3 s and then
// fades in: the deck's own mic heard a steady chime start ~300 ms late with a 60 ms wait, and on
// time with 400 ms (2026-11-05, AUDIO_PROBE below). Clips cut their first syllable with 60.
#ifndef AMP_SETTLE_MS
#define AMP_SETTLE_MS 400                  // amp enable to first sample, from cold
#endif
// So the path stays on (amp enabled, I2S sending silence) for this long after a clip at WARM_RATE:
// the next one, the boot's chime, montage start and logo sound, starts at once and whole.
constexpr int WARM_RATE = 16000;           // the clips' rate; the mic's too, so it can still join
constexpr uint32_t LINGER_MS = 20000;
int s_rate = 16000;
int16_t s_buf[2 * CHUNK];
bool s_held = false;                       // open() holds the port: close() releases it once
bool s_warm = false;                       // between clips: the port held and the amp on
SemaphoreHandle_t s_lock = nullptr;        // s_held / s_warm: the clip task and the cool-down timer
esp_timer_handle_t s_cool = nullptr;

void powerDown() {                         // under s_lock
    digitalWrite(PIN_AMP_EN, !AMP_ON);
    audioport::release();                  // uninstalls unless the mic still listens
}

void coolDown(void*) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_warm && !s_held) {
        s_warm = false;
        powerDown();
    }
    xSemaphoreGive(s_lock);
}

#ifdef AUDIO_PROBE
// -D AUDIO_PROBE: the deck's mic listens to the speaker while a clip plays. close() logs 4 ms RMS
// blocks (dBFS) from amp-on (mic) and from the first sample (out): a steady clip whose mic level
// rises late or slowly is losing its start in the audio path.
constexpr int PB = 64, PN = 150;
uint16_t s_micE[PN], s_outE[PN];
int s_micN = 0, s_outN = 0, s_micK = 0, s_outK = 0, s_settleBlocks = 0;
uint64_t s_micA = 0, s_outA = 0;
int16_t s_rx[2 * PB];
void probeStart() {
    es8311::enableMic(4, 0xBF);
    s_micN = s_outN = s_micK = s_outK = s_settleBlocks = 0;
    s_micA = s_outA = 0;
    size_t got;
    while (i2s_read(PORT, s_rx, sizeof s_rx, &got, 0) == ESP_OK && got) {}   // stale RX
}
void probeRx(TickType_t wait) {
    size_t got = 0;
    while (s_micN < PN && i2s_read(PORT, s_rx, sizeof s_rx, &got, wait) == ESP_OK && got) {
        for (size_t i = 0; i < got / 4; ++i) {
            s_micA += (int32_t)s_rx[2 * i] * s_rx[2 * i];
            if (++s_micK == PB && s_micN < PN) { s_micE[s_micN++] = (uint16_t)sqrtf((float)(s_micA / PB)); s_micA = 0; s_micK = 0; }
        }
        wait = 0;
    }
}
void probeOut(const int16_t* x, size_t n) {
    for (size_t i = 0; i < n && s_outN < PN; ++i) {
        s_outA += (int32_t)x[i] * x[i];
        if (++s_outK == PB) { s_outE[s_outN++] = (uint16_t)sqrtf((float)(s_outA / PB)); s_outA = 0; s_outK = 0; }
    }
}
int db(uint16_t v) { return v ? (int)lroundf(20.0f * log10f(v / 32768.0f)) : -99; }
void probeLog() {
    Serial.printf("[probe] settle %d blocks; mic dB per 4 ms from amp on:", s_settleBlocks);
    for (int i = 0; i < s_micN; ++i) Serial.printf(" %d", db(s_micE[i]));
    Serial.println();
    Serial.print("[probe] out dB per 4 ms from first sample:");
    for (int i = 0; i < s_outN; ++i) Serial.printf(" %d", db(s_outE[i]));
    Serial.println();
}
#endif
}

bool warm(int clipRate) {
    if (!s_lock) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool w = s_warm && clipRate == s_rate;
    xSemaphoreGive(s_lock);
    return w;
}

bool open(int clipRate) {
    if (!s_lock) s_lock = xSemaphoreCreateMutex();   // clips play from one task at a time
    if (!s_lock) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_cool) esp_timer_stop(s_cool);
    if (s_warm && clipRate == s_rate) {    // still on from the last clip: no wake needed
        s_warm = false;
        s_held = true;
#ifdef AUDIO_PROBE
        probeStart();
#endif
        xSemaphoreGive(s_lock);
        return true;
    }
    if (s_warm) {                          // on at another rate: off first
        s_warm = false;
        powerDown();
    }
    // Shares the port with the mic at 16 kHz; a 48 kHz clip while the mic listens is refused.
    bool ok = audioport::acquire(clipRate);
    if (ok) {
        s_rate = clipRate;
        ok = es8311::setVolume(CODEC_GAIN);
        if (!ok) {
            Serial.println("[audio] ES8311 not answering");
            audioport::release();
        }
    }
    if (ok) {
#ifdef AUDIO_PROBE
        probeStart();
#endif
        pinMode(PIN_AMP_EN, OUTPUT);
        digitalWrite(PIN_AMP_EN, AMP_ON);  // only now: codec up, I2S sending silence (no pop)
        // I2S sends silence while the speaker wakes (tx_desc_auto_clear).
#ifdef AUDIO_PROBE
        for (const uint32_t t = millis(); millis() - t < AMP_SETTLE_MS;) probeRx(pdMS_TO_TICKS(10));
        s_settleBlocks = s_micN;
#else
        vTaskDelay(pdMS_TO_TICKS(AMP_SETTLE_MS));
#endif
        s_held = true;
    }
    xSemaphoreGive(s_lock);
    return ok;
}

void write(const int16_t* s, size_t n) {
    while (n) {
        const size_t k = n < (size_t)CHUNK ? n : (size_t)CHUNK;
        for (size_t i = 0; i < k; ++i) s_buf[2 * i] = s_buf[2 * i + 1] = s[i];   // mono on both channels
        size_t written;
        i2s_write(PORT, s_buf, k * 2 * sizeof(int16_t), &written, portMAX_DELAY);
#ifdef AUDIO_PROBE
        probeOut(s, k);
        probeRx(0);
#endif
        s += k;
        n -= k;
    }
}

void close() {
    if (!s_held) return;                    // open() failed (or never ran): nothing to release
    const uint32_t dmaMs = 1000u * DMA_BUFS * DMA_LEN / (uint32_t)s_rate;
    vTaskDelay(pdMS_TO_TICKS(dmaMs + 20));  // let the queued DMA buffers play out
#ifdef AUDIO_PROBE
    probeLog();
#endif
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_held = false;
    if (s_rate == WARM_RATE) {              // stay on for the next clip; coolDown() turns it off
        if (!s_cool) {
            const esp_timer_create_args_t a = {coolDown, nullptr, ESP_TIMER_TASK, "audiocool", false};
            esp_timer_create(&a, &s_cool);
        }
        s_warm = s_cool && esp_timer_start_once(s_cool, (uint64_t)LINGER_MS * 1000) == ESP_OK;
    }
    if (!s_warm) powerDown();
    xSemaphoreGive(s_lock);
}

}
