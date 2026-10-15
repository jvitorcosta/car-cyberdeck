#include "intro.h"
#include <Arduino.h>
#include <atomic>
#include "montage.h"
#include "../util/introclock.h"
#include "../audio/greeting.h"

#ifdef BOARD_S3
#include <FS.h>
using fs::File;            // JPEGDEC.h names the global File, which this core's FS.h may not export
#include <JPEGDEC.h>
#include "../core/embed.h"
EMBED(data_intro_bin);
#endif

namespace intro {
#ifdef BOARD_S3
static TFT_eSPI* s_tft = nullptr;
static std::atomic<bool> s_playing{false};   // set here, cleared by the intro task

// JPEGDEC (bitbank2): ~2x faster than tjpgd on the S3, ~52 ms a frame with the push. ~18 KB of
// state: static, the S3 has RAM to spare. No DMA: TFT_eSPI's initDMA() on this board left every
// later draw (night drive, deck) on a black screen (2026-10-15).
static JPEGDEC s_jpeg;

static int draw(JPEGDRAW* d) {
    s_tft->pushImage(d->x, d->y, d->iWidth, d->iHeight, d->pPixels);
    return 1;
}

static void task(void*) {
    const uint32_t t0 = millis();
    int shown = -1, drawn = 0;
    uint32_t busy = 0;
    for (;;) {
        const int i = introclock::frameAt(millis() - t0, montage::fps(), montage::count());
        if (i >= montage::count()) break;
        if (i != shown) {                         // late frames are skipped, not queued
            const uint8_t* jpg = nullptr;
            size_t len = 0;
            if (montage::frame(i, &jpg, &len)) {
                const uint32_t f0 = millis();
                if (s_jpeg.openFLASH((uint8_t*)jpg, (int)len, draw)) {
                    s_jpeg.setPixelType(RGB565_BIG_ENDIAN);   // as pushImage sends it as-is
                    s_jpeg.decode(0, 0, 0);
                    s_jpeg.close();
                }
                busy += millis() - f0;
                ++drawn;
            }
            shown = i;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    Serial.printf("[intro] %d of %d frames in %lu ms (%lu ms a frame)\n", drawn, montage::count(),
                  (unsigned long)(millis() - t0), (unsigned long)(drawn ? busy / drawn : 0));
    s_playing = false;
    vTaskDelete(nullptr);
}

void start(TFT_eSPI& t) {
    const size_t len = EMBED_SIZE(data_intro_bin);
    if (!montage::parse(EMBED_DATA(data_intro_bin), len) || montage::count() == 0) {
        Serial.println("[intro] none");
        return;
    }
    s_tft = &t;
    s_playing = true;
    t.fillScreen(TFT_BLACK);
    // The sound starts first: started after the decode task (priority 2, busy from its first
    // frame), its task got going late and played the 14 s clip in 24 s, full of gaps.
    greeting::playIntro();
    if (xTaskCreatePinnedToCore(task, "intro", 6144, nullptr, 2, nullptr, 1) != pdPASS) {
        s_playing = false;     // wait() would spin forever and the deck never boot
        Serial.println("[intro] no memory for its task: skipped (its sound still plays)");
    }
}

bool playing() { return s_playing; }

void wait() {
    while (s_playing) delay(20);
}
#else
void start(TFT_eSPI&) {}
bool playing() { return false; }
void wait() {}
#endif
}
