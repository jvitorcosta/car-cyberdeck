#include "karaoke.h"
#include <Arduino.h>

namespace karaoke {
namespace {
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
karaokemode::Mode s_mode;  // under s_mux
int s_light = -1;          // under s_mux
bool s_logged = false;     // the last state logged: on or off, not every keep-alive
}  // namespace

void keepAlive(int light) {
    portENTER_CRITICAL(&s_mux);
    s_mode.keepAlive(millis());
    s_light = light;
    portEXIT_CRITICAL(&s_mux);
}

int light() {
    portENTER_CRITICAL(&s_mux);
    const int l = s_light;
    portEXIT_CRITICAL(&s_mux);
    return l;
}

void stop() {
    portENTER_CRITICAL(&s_mux);
    s_mode.stop();
    portEXIT_CRITICAL(&s_mux);
}

karaokemode::Screen screen(lyricstatus::Status s, uint32_t gen) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    const bool on = s_mode.on(now);
    const karaokemode::Screen sc = s_mode.screen(s, gen, now);
    portEXIT_CRITICAL(&s_mux);
    if (on != s_logged) {  // the UI asks often: the log tells each change, once
        s_logged = on;
        Serial.printf("[karaoke] %s\n", on ? "on" : "off");
    }
    return sc;
}
}  // namespace karaoke
