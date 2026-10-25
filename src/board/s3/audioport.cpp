#include "audioport.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "es8311.h"
#include "pins.h"

namespace audioport {

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr int DMA_BUFS = 4;
constexpr int DMA_LEN = 256;
constexpr int MCLK_MULT = 384;             // Freenove's tested setting: 6.144 MHz at 16 kHz
int s_users = 0;                           // guarded by lock()
int s_rate = 0;
bool s_wire = false;

// Created on first use (a function-local static: thread-safe).
SemaphoreHandle_t lock() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

bool install(int rate) {
    if (!s_wire) s_wire = Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    i2s_config_t cfg = {};
    // RX always, even for a clip alone: the mic can then join a port that's already playing.
    // TX is unchanged by it (same rate, DMA, silence on underflow); unread RX buffers just recycle.
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    cfg.sample_rate = rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count = DMA_BUFS;
    cfg.dma_buf_len = DMA_LEN;
    cfg.tx_desc_auto_clear = true;         // silence on underflow
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[audio] i2s install failed");
        return false;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = PIN_I2S_MCLK;
    pins.bck_io_num = PIN_I2S_BCLK;
    pins.ws_io_num = PIN_I2S_WS;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num = PIN_I2S_DIN;
    i2s_set_pin(PORT, &pins);
    if (!s_wire || !es8311::begin(rate, rate * MCLK_MULT)) {   // MCLK is running: codec can clock
        Serial.println("[audio] ES8311 not answering");
        i2s_driver_uninstall(PORT);
        return false;
    }
    return true;
}
}  // namespace

bool acquire(int rate) {
    if (!lock()) {                         // no memory for the mutex: no audio rather than a crash
        Serial.println("[audio] no memory for the port's lock");
        return false;
    }
    xSemaphoreTake(lock(), portMAX_DELAY);
    bool ok;
    if (s_users > 0) {
        ok = rate == s_rate;
        if (!ok) Serial.printf("[audio] busy at %d Hz, %d Hz refused\n", s_rate, rate);
    } else {
        ok = install(rate);
        if (ok) s_rate = rate;
    }
    if (ok) ++s_users;
    xSemaphoreGive(lock());
    return ok;
}

void release() {
    if (!lock()) return;                   // acquire() never succeeded
    xSemaphoreTake(lock(), portMAX_DELAY);
    if (s_users > 0 && --s_users == 0) i2s_driver_uninstall(PORT);
    xSemaphoreGive(lock());
}

}  // namespace audioport
