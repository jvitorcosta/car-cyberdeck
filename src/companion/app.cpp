// Companion screen (env cyd_companion): the CYD beside the S3, showing the lyrics big. It reads
// the S3's GET /state over the hotspot and has no Spotify client of its own.
// Spec: docs/superpowers/specs/2026-10-13-companion-screen-design.md (v1: lyrics only).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../board/board.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../ui/backlight.h"
#include "../ui/theme.h"
#include "../util/daynight.h"
#include "../util/version.h"
#include "dashboard.h"
#include "link.h"

static TFT_eSPI s_tft;

void setup() {
    Serial.begin(115200);
    board::begin();
    Serial.printf("[boot] cardeck companion v%s\n", FW_VERSION);
    s_tft.init(); s_tft.invertDisplay(true); s_tft.setRotation(1);
    s_tft.fillScreen(theme::pal().top);
    backlight::begin();
    s_tft.setTextColor(theme::pal().topText, theme::pal().top);
    s_tft.drawString("Connecting to WiFi...", 10, 10, 2);
    net::connectAny();          // if it fails, the link task keeps retrying (net::loop)
    netclock::begin();          // night palette
    decklink::start();
    dashboard::invalidate();
}

void loop() {
    static uint32_t lastNightCheck = 0;
    if (lastNightCheck == 0 || millis() - lastNightCheck > 60000) {
        lastNightCheck = millis() | 1;
        int minute = 0;
        const bool night = netclock::minuteOfDay(&minute) && daynight::isNight(minute);
        if (night != theme::isNightActive()) {
            theme::setNight(night);
            backlight::set(night);
            s_tft.fillScreen(theme::pal().top);
            dashboard::invalidate();
        }
    }
    decklink::View v;
    decklink::snapshot(v);
    dashboard::draw(s_tft, v, millis());
    delay(20);
}
