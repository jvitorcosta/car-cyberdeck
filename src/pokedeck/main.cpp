// The Pokédeck (env pokedeck): the 4.3" ESP32-S3 panel that is to take over the music from the
// black deck. This build is its base only: it joins the WiFi (the phone hotspot first, where it
// pins 172.20.10.13), takes updates over WiFi with the decks' boot guard and rollback
// (common/net/ota), is watched by the task watchdog, keeps the crash log and shows its state.
// At boot it plays one of the intro videos from its SD card (ui/opening).
// Serial: [board], [boot], [crash], [sd], [intro], [wifi], [ota], [mem] every minute.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "board/panelgfx.h"
#include "media/sdcard.h"
#include "music/sample.h"
#include "../black/ui/cjkdata.h"
#include "../black/ui/emojidata.h"
#include "ui/opening.h"
#include "../common/board/board.h"
#include "../common/core/crashlog.h"
#include "../common/core/watchdog.h"
#include "../common/net/clock.h"
#include "../common/net/ota.h"
#include "../common/net/remote_config.h"
#include "../common/net/wifi.h"
#include "../common/ui/backlight.h"
#include "../util/common/version.h"

static TaskHandle_t netHandle = nullptr;
static bool screenOk = false;
static bool musicOk = false;   // the battle screen is up (a sample song so far)

// WiFi, then mDNS, SNTP and the update listener; then the rejoin loop. Watched: a join feeds
// before each try.
static void netTask(void*) {
    watchdog::watch();
    net::connectAny();
    while (!net::isOnline()) {
        watchdog::feed();
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (!MDNS.begin(POKEDECK_MDNS_NAME)) Serial.println("[net] mDNS failed: reach the deck by its address");
    netclock::begin();
    ota::begin(POKEDECK_MDNS_NAME);
    for (;;) {
        watchdog::feed();
        // rejoins after a drop; not during an update (a move to the hotspot would cut it)
        if (!ota::active()) net::loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

constexpr int LINES = 3, LINE_Y = 80, LINE_STEP = 44, LINE_H = 32;

// The state, a line repainted only when its text changed.
static void drawStatus() {
    static char shown[LINES][48] = {};
    char text[LINES][48];
    const bool online = net::isOnline();
    if (online) snprintf(text[0], sizeof text[0], "WIFI %.20s", net::ssid().c_str());
    else snprintf(text[0], sizeof text[0], "WIFI ...");
    snprintf(text[1], sizeof text[1], "%s", online ? net::deviceIp().c_str() : "-");
    const ota::View ov = ota::view();
    if (ov.phase == otaview::Phase::Receiving) snprintf(text[2], sizeof text[2], "UPDATE %d%%", ov.pct);
    else if (ov.phase == otaview::Phase::Done) snprintf(text[2], sizeof text[2], "UPDATE DONE");
    else if (ov.phase == otaview::Phase::Failed) snprintf(text[2], sizeof text[2], "UPDATE FAILED");
    else snprintf(text[2], sizeof text[2], "%s", ota::listening() ? "UPDATES ON" : "UPDATES OFF");
    Arduino_GFX& g = panel::gfx();
    for (int i = 0; i < LINES; ++i) {
        if (strcmp(text[i], shown[i]) == 0) continue;
        strlcpy(shown[i], text[i], sizeof shown[i]);
        const int y = LINE_Y + i * LINE_STEP;
        g.fillRect(0, y, panel::W, LINE_H, BLACK);
        g.setCursor(24, y + 4);
        g.setTextSize(3);
        g.setTextColor(WHITE);
        g.print(text[i]);
    }
}

// The opening, watched like any long step. Skipped after a crash or a watchdog reset, so a bad
// file on the card can't restart the deck boot after boot.
static void playOpening() {
    const esp_reset_reason_t r = esp_reset_reason();
    if (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT) {
        Serial.println("[intro] skipped after a crash");
        return;
    }
    crashlog::at("opening");
    watchdog::watch();
    if (sdcard::begin()) {
        opening::play(screenOk);   // an intro video from the card, with its sound
        ui::cjk();                 // the screens' fonts, while the card is mounted
        ui::emoji();
        sdcard::end();
    }
    watchdog::forget();   // loop() is watched by the core from here (enableLoopWDT)
    crashlog::at("");
}

static void drawTitle() {
    Arduino_GFX& g = panel::gfx();
    g.fillScreen(BLACK);
    g.setTextColor(WHITE);
    g.setCursor(24, 16);
    g.setTextSize(4);
    g.print("POKEDECK");
    g.setCursor(24, 56);
    g.setTextSize(2);
    g.print(FW_VERSION);
}

void setup() {
    Serial.begin(115200);
    board::begin();
    crashlog::begin();
    Serial.printf("[boot] pokedeck v%s\n", FW_VERSION);
    ota::checkBoot();   // may boot the other app slot (net/ota), before anything else starts
    watchdog::begin();  // 90 s, before any task subscribes
    screenOk = panel::begin();
    backlight::begin();
    if (screenOk) panel::gfx().fillScreen(BLACK);
    else Serial.println("[screen] the panel didn't start");
    // WiFi joins underneath the opening.
    xTaskCreatePinnedToCore(netTask, "net", 6144, nullptr, 0, &netHandle, 0);
    playOpening();
    musicOk = screenOk && sample::begin();
    if (screenOk && !musicOk) drawTitle();
    enableLoopWDT();   // the core feeds it before each loop()
}

void loop() {
    const uint32_t now = millis();
    static uint32_t drawnAt = 0, memAt = 0;
    crashlog::tick();
    if (now - drawnAt >= 200) {   // often enough for an update's percent
        drawnAt = now;
        // Up bootguard::HEALTHY_MS, online and listening: this firmware has proven itself.
        if (net::isOnline()) ota::onGoodPoll();
        // The status screen while an update comes in, or without memory for the music screen.
        static bool updating = false;
        if (screenOk && (!musicOk || ota::active())) {
            if (musicOk && !updating) drawTitle();
            updating = musicOk;
            drawStatus();
        } else if (updating) {
            updating = false;
            sample::redraw();
        }
    }
    if (musicOk && !ota::active()) sample::tick(now);
    if (now - memAt >= 60000) {
        memAt = now;
        Serial.printf("[mem] internal %u (largest %u) psram %u  stack left: net %u, ota %u, loop %u\n",
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)),
                      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)),
                      static_cast<unsigned>(ESP.getFreePsram()),
                      netHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(netHandle)) : 0u,
                      ota::stackLeft(), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    delay(20);
}
