#include "intro.h"
#include <Arduino.h>
#include <cstring>
#include "montage.h"
#include "../util/introclock.h"
#include "../audio/greeting.h"

#ifdef BOARD_S3
#include <FS.h>
using fs::File;            // JPEGDEC.h names the global File, which this core's FS.h may not export
#include <JPEGDEC.h>
extern const uint8_t _binary_data_intro_bin_start[] asm("_binary_data_intro_bin_start");
extern const uint8_t _binary_data_intro_bin_end[] asm("_binary_data_intro_bin_end");
#endif

namespace intro {
#ifdef BOARD_S3
static TFT_eSPI* s_tft = nullptr;
static volatile bool s_playing = false;

// JPEGDEC (bitbank2): ~3x faster than tjpgd on the S3 (SIMD), so the clip plays at its full rate
// while WiFi and the Spotify login run underneath. ~18 KB of state: static, the S3 has RAM to
// spare. A 320 x 240 frame takes ~31 ms on the 40 MHz SPI bus, so each block it hands over goes
// out by DMA from one of two buffers while the next one decodes.
static JPEGDEC s_jpeg;
constexpr int BLOCK_PX = MAX_BUFFERED_PIXELS;   // the most JPEGDEC hands over per call
static uint16_t s_dma[2][BLOCK_PX];
static int s_buf = 0;
static bool s_useDma = false;

static int draw(JPEGDRAW* d) {
    const int n = d->iWidth * d->iHeight;
    if (!s_useDma || n > BLOCK_PX) {
        s_tft->pushImage(d->x, d->y, d->iWidth, d->iHeight, d->pPixels);
        return 1;
    }
    uint16_t* b = s_dma[s_buf];
    s_buf ^= 1;
    memcpy(b, d->pPixels, 2 * n);   // free: only the other buffer can still be going out
    s_tft->pushImageDMA(d->x, d->y, d->iWidth, d->iHeight, b);
    return 1;
}

static void task(void*) {
    s_useDma = s_tft->initDMA();
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
                    s_tft->startWrite();                      // DMA needs the bus held
                    s_jpeg.decode(0, 0, 0);
                    s_tft->dmaWait();
                    s_tft->endWrite();
                    s_jpeg.close();
                }
                busy += millis() - f0;
                ++drawn;
            }
            shown = i;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    // DMA stays attached: deInitDMA() frees the SPI bus, and every later draw (night drive, deck)
    // went nowhere, a black screen (2026-10-15). Plain pushes work alongside it.
    Serial.printf("[intro] %d of %d frames in %lu ms (%lu ms a frame%s)\n", drawn, montage::count(),
                  (unsigned long)(millis() - t0), (unsigned long)(drawn ? busy / drawn : 0), s_useDma ? ", DMA" : "");
    s_playing = false;
    vTaskDelete(nullptr);
}

void start(TFT_eSPI& t) {
    const size_t len = (size_t)(_binary_data_intro_bin_end - _binary_data_intro_bin_start);
    if (!montage::parse(_binary_data_intro_bin_start, len) || montage::count() == 0) {
        Serial.println("[intro] none");
        return;
    }
    s_tft = &t;
    s_playing = true;
    t.fillScreen(TFT_BLACK);
    greeting::playIntro();     // the clip's sound, started with the first frame
    xTaskCreatePinnedToCore(task, "intro", 6144, nullptr, 2, nullptr, 1);
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
