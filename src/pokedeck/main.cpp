// The Pokédeck (env pokedeck): the 4.3" ESP32-S3 panel that is taking over the music from the
// black deck. It plays Spotify's current song on the battle screen (pokedeck/music), and it joins the WiFi (the phone hotspot first, where it
// pins 172.20.10.13), takes updates over WiFi with the decks' boot guard and rollback
// (common/net/ota), is watched by the task watchdog, keeps the crash log and shows its state.
// At boot it plays one of the intro videos from its SD card (ui/opening).
// Serial: [board], [boot], [crash], [sd], [intro], [wifi], [ota], [mem] every minute.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "board/panelgfx.h"
#include "media/sdcard.h"
#include "music/art.h"
#include "music/nettask.h"
#include "music/screen.h"
#include "../black/core/mem.h"
#include "../black/core/shared.h"
#include "../black/images/walksprite.h"
#include "../black/ui/cjkdata.h"
#include "../black/ui/emojidata.h"
#include "ui/opening.h"
#include "../common/board/board.h"
#include "../common/core/crashlog.h"
#include "../common/core/watchdog.h"
#include "../common/net/ota.h"
#include "../common/net/wifi.h"
#include "../common/ui/backlight.h"
#include "../util/common/version.h"

static bool screenOk = false;
static bool musicOk = false;   // the music screen has its frame

constexpr int LINES = 3, LINE_Y = 80, LINE_STEP = 44, LINE_H = 32;

static char s_shown[LINES][48] = {};   // the status screen's lines as drawn; cleared with the screen

// The state, a line repainted only when its text changed.
static void drawStatus() {
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
        if (strcmp(text[i], s_shown[i]) == 0) continue;
        strlcpy(s_shown[i], text[i], sizeof s_shown[i]);
        const int y = LINE_Y + i * LINE_STEP;
        g.fillRect(0, y, panel::W, LINE_H, BLACK);
        g.setCursor(24, y + 4);
        g.setTextSize(3);
        g.setTextColor(WHITE);
        g.print(text[i]);
    }
}

// The last run ended in a crash or a watchdog reset.
static bool crashedLast() {
    const esp_reset_reason_t r = esp_reset_reason();
    return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

// The screens' CJK and emoji fonts, read from the card once and kept. Watched like the opening, and
// skipped after a crash too, so a bad card can't restart the deck boot after boot.
static void loadFonts() {
    if (crashedLast()) return;
    crashlog::at("fonts");
    watchdog::watch();
    if (sdcard::begin()) {
        ui::cjk();
        ui::emoji();
        sdcard::end();
    }
    watchdog::forget();
    crashlog::at("");
}

// The opening, watched like any long step. Skipped after a crash or a watchdog reset, so a bad
// file on the card can't restart the deck boot after boot.
static void playOpening() {
    if (crashedLast()) {
        Serial.println("[intro] skipped after a crash");
        return;
    }
    crashlog::at("opening");
    watchdog::watch();
    if (sdcard::begin()) {
        opening::play(screenOk);   // an intro video from the card, with its sound
        sdcard::end();
    }
    watchdog::forget();   // loop() is watched by the core from here (enableLoopWDT)
    crashlog::at("");
}

static void drawTitle() {
    Arduino_GFX& g = panel::gfx();
    g.fillScreen(BLACK);
    memset(s_shown, 0, sizeof s_shown);
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
    shared::begin();
    walk::begin();  // both walker slots, before the heap fragments
    art::begin();   // the cover's bitmap, before WiFi takes the internal heap
    // WiFi, the update listener and Spotify come up underneath the opening (music/nettask).
    nettask::start();
    // No opening after the network task's self-heal restart (util/quietboot): mid-drive, the screen
    // comes straight back.
    if (!nettask::quietBoot()) playOpening();
    loadFonts();   // on every boot, the skipped openings' too
    musicOk = screenOk && screen::begin();
    if (screenOk && !musicOk) drawTitle();
    enableLoopWDT();   // the core feeds it before each loop()
}

void loop() {
    const uint32_t now = millis();
    static uint32_t drawnAt = 0, memAt = 0;
    crashlog::tick();
    if (now - drawnAt >= 200) {   // often enough for an update's percent
        drawnAt = now;
        // The status screen while an update comes in, or without memory for the music screen.
        static bool updating = false;
        if (screenOk && (!musicOk || ota::active())) {
            if (musicOk && !updating) drawTitle();
            updating = musicOk;
            drawStatus();
        } else if (updating) {
            updating = false;
            screen::redraw();
        }
    }
    if (musicOk && !ota::active()) screen::tick();
    if (now - memAt >= 60000) {
        memAt = now;
        mem::log("minute");
        Serial.printf("[mem] stack left: ota %u, loop %u\n", ota::stackLeft(),
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    delay(20);
}
