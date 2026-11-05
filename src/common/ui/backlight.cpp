#include "backlight.h"
#include <Arduino.h>
#include <driver/ledc.h>
#include "pins.h"

namespace backlight {

constexpr int CHANNEL = 1;          // LEDC channel (nothing else uses LEDC)
constexpr int FREQ_HZ = 5000;
constexpr int BITS = 8;
constexpr int DAY_LEVEL = 255;      // full brightness (auto-dim declined 2026-10-07)
constexpr int NIGHT_LEVEL = 90;     // ~35 %; tune on the panel
// esp32-hal-ledc's mapping of an Arduino channel to the IDF's (speed mode, channel)
constexpr ledc_mode_t MODE = (ledc_mode_t)(CHANNEL / 8);
constexpr ledc_channel_t IDF_CHANNEL = (ledc_channel_t)(CHANNEL % 8);
static int s_level = DAY_LEVEL;
static bool s_fades = false;              // ledc_fade_func_install done

void begin() {
    ledcSetup(CHANNEL, FREQ_HZ, BITS);
    ledcAttachPin(PIN_BL, CHANNEL);
    ledcWrite(CHANNEL, DAY_LEVEL);
}

void set(bool night) {
    s_level = night ? NIGHT_LEVEL : DAY_LEVEL;
    ledcWrite(CHANNEL, s_level);
}

void setLevel(int level) {
    s_level = level < 0 ? 0 : level > DAY_LEVEL ? DAY_LEVEL : level;
    ledcWrite(CHANNEL, s_level);
}

bool fadeOut(uint32_t ms) {
    if (!s_fades) s_fades = ledc_fade_func_install(0) == ESP_OK;
    return s_fades && ledc_set_fade_time_and_start(MODE, IDF_CHANNEL, 0, ms, LEDC_FADE_NO_WAIT) == ESP_OK;
}

void restore() { ledcWrite(CHANNEL, s_level); }

}
