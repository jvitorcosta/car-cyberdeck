#include "speaker.h"
#include <Arduino.h>
#include <atomic>
#include <driver/i2s.h>
#include "../../common/core/watchdog.h"

namespace speaker {
namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr int BCLK = 42, LRCK = 2, DOUT = 41;
constexpr int DMA_BUFS = 6, DMA_LEN = 256;
constexpr size_t CHUNK = 256;   // mono samples per write
const int16_t* s_samples = nullptr;
size_t s_count = 0;
int s_rate = 16000;
int s_percent = 100;
std::atomic<bool> s_playing{false};
std::atomic<uint32_t> s_started{0};

bool install(int rate) {
    i2s_config_t cfg = {};
    cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = static_cast<uint32_t>(rate);
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count = DMA_BUFS;
    cfg.dma_buf_len = DMA_LEN;
    cfg.tx_desc_auto_clear = true;   // silence, not the last buffer again, when the clip ends
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = BCLK;
    pins.ws_io_num = LRCK;
    pins.data_out_num = DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(PORT, &pins) == ESP_OK) return true;
    i2s_driver_uninstall(PORT);
    return false;
}

void task(void*) {
    static int16_t stereo[2 * CHUNK];
    for (size_t at = 0; at < s_count; at += CHUNK) {
        const size_t n = s_count - at < CHUNK ? s_count - at : CHUNK;
        for (size_t i = 0; i < n; ++i)
            stereo[2 * i] = stereo[2 * i + 1] = static_cast<int16_t>(s_samples[at + i] * s_percent / 100);
        size_t written = 0;
        if (at == 0) {
            const uint32_t now = millis();
            s_started.store(now ? now : 1);
        }
        // Bounded: a driver that stops taking samples ends the clip instead of hanging the boot.
        if (i2s_write(PORT, stereo, n * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(1000)) != ESP_OK ||
            written == 0) {
            Serial.println("[audio] the sound stopped early");
            break;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000u * DMA_BUFS * DMA_LEN / static_cast<uint32_t>(s_rate) + 20));   // play out
    i2s_driver_uninstall(PORT);
    Serial.printf("[audio] clip over, stack left %u\n", static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    s_playing.store(false);
    vTaskDelete(nullptr);
}
}  // namespace

bool play(const int16_t* samples, size_t count, int rate, int percent) {
    if (s_playing.load() || !samples || !count) return false;
    s_started.store(0);
    if (!install(rate)) {
        Serial.println("[audio] the I2S driver didn't start");
        return false;
    }
    s_samples = samples;
    s_count = count;
    s_rate = rate;
    s_percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    s_playing.store(true);
    // Above the drawing loop (priority 1) on the other core, so frames never starve the sound.
    if (xTaskCreatePinnedToCore(task, "speaker", 3072, nullptr, 3, nullptr, 0) == pdPASS) return true;
    s_playing.store(false);
    i2s_driver_uninstall(PORT);
    Serial.println("[audio] no memory for the sound task");
    return false;
}

uint32_t startedAt() { return s_started.load(); }

void wait() {
    while (s_playing.load()) {
        watchdog::feed();
        delay(10);
    }
}
}  // namespace speaker
