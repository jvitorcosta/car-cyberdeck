#include "intro.h"
#include <Arduino.h>
#include <cstring>
#include "tjpgd.h"
#include "../ui/montage.h"
#include "../util/introclock.h"

extern const uint8_t _binary_data_intro_bin_start[] asm("_binary_data_intro_bin_start");
extern const uint8_t _binary_data_intro_bin_end[] asm("_binary_data_intro_bin_end");

namespace intro {
static TFT_eSPI* s_tft = nullptr;
static volatile bool s_playing = false;

// Plain tjpgd, as the S3's boot scene decodes its montage (ui/bootscene).
struct Src { const uint8_t* p; size_t len, pos; };

static size_t in(JDEC* jd, uint8_t* buf, size_t n) {
    Src* s = (Src*)jd->device;
    if (n > s->len - s->pos) n = s->len - s->pos;
    if (buf) memcpy(buf, s->p + s->pos, n);
    s->pos += n;
    return n;
}

static int out(JDEC*, void* bmp, JRECT* r) {
    s_tft->pushImage(r->left, r->top, r->right - r->left + 1, r->bottom - r->top + 1, (uint16_t*)bmp);
    return 1;
}

static void task(void*) {
    static uint8_t work[TJPGD_WORKSPACE_SIZE];   // the companion has no TLS to share RAM with
    const uint32_t t0 = millis();
    int shown = -1, drawn = 0;
    for (;;) {
        const int i = introclock::frameAt(millis() - t0, montage::fps(), montage::count());
        if (i >= montage::count()) break;
        if (i != shown) {                         // late frames are skipped, not queued
            const uint8_t* jpg = nullptr;
            size_t len = 0;
            if (montage::frame(i, &jpg, &len)) {
                Src src{jpg, len, 0};
                JDEC jd = {};
                jd.swap = 1;                      // big-endian RGB565, as pushImage sends it as-is
                if (jd_prepare(&jd, in, work, sizeof work, &src) == JDR_OK) jd_decomp(&jd, out, 0);
                ++drawn;
            }
            shown = i;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    Serial.printf("[intro] %d of %d frames in %lu ms\n", drawn, montage::count(),
                  (unsigned long)(millis() - t0));
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
    xTaskCreatePinnedToCore(task, "intro", 6144, nullptr, 2, nullptr, 1);
}

bool playing() { return s_playing; }

void wait() {
    while (s_playing) delay(20);
}
}
