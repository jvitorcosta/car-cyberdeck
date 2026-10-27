#include "intro.h"
#include <Arduino.h>
#include <atomic>
#include <new>
#include <esp_heap_caps.h>
#include "montage.h"
#include "../util/black/introclock.h"
#include "../audio/greeting.h"

#include <FS.h>
using fs::File;            // JPEGDEC.h names the global File, which this core's FS.h may not export
#include <JPEGDEC.h>
#include "../core/media.h"
#include "bootterm.h"
#include "../util/common/rgb565.h"
#include "../util/black/termblend.h"
MEDIA_DECL(data_intro_bin);
MEDIA_DECL(data_max_bin);

namespace intro {
static TFT_eSPI* s_tft = nullptr;
static std::atomic<bool> s_playing{false};   // set here, cleared by the intro task

// JPEGDEC (bitbank2): ~2x faster than tjpgd on the S3, ~52 ms a frame with the push. ~18 KB of
// state, on the heap for the intro only (it was static: held all session for a few seconds' use).
// Internal RAM, as the static was, so decoding is no slower; PSRAM if that is short (CPU only:
// no DMA, as TFT_eSPI's initDMA() on this board left every later draw (night drive, deck) on a
// black screen, 2026-10-15).
static JPEGDEC* s_jpeg = nullptr;

static bool newJpeg() {
    void* p = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_jpeg = p ? new (p) JPEGDEC() : nullptr;
    return s_jpeg;
}

static void freeJpeg() {
    if (!s_jpeg) return;
    s_jpeg->~JPEGDEC();
    heap_caps_free(s_jpeg);
    s_jpeg = nullptr;
}

static bool (*s_online)() = nullptr;
static TFT_eSprite* s_term = nullptr;   // bootterm::W x H, KEY where there's no glyph
static uint16_t* s_termPx = nullptr;
static uint32_t s_t0 = 0;

// The terminal's pixels for this frame (util/termblend paints them into each decoded block).
static void renderTerm(uint32_t t) {
    if (!s_term) return;
    bootterm::events(t, false);
    s_term->fillSprite(rgb565::be(termblend::KEY));   // fillSprite takes native; KEY is big-endian
    bootterm::drawInto(*s_term, t, bootterm::X, bootterm::Y);
}

static void freeTerm() {
    if (s_term) {
        s_term->deleteSprite();
        delete s_term;
        s_term = nullptr;
    }
    s_termPx = nullptr;
}

static int draw(JPEGDRAW* d) {
    if (s_termPx)
        termblend::composite((uint16_t*)d->pPixels, d->x, d->y, d->iWidth, d->iHeight,
                             s_termPx, bootterm::X, bootterm::Y, bootterm::W, bootterm::H);
    s_tft->pushImage(d->x, d->y, d->iWidth, d->iHeight, d->pPixels);
    return 1;
}

// Frame i, decoded straight to the display. False: no such frame.
static bool show(int i) {
    const uint8_t* jpg = nullptr;
    size_t len = 0;
    if (!montage::frame(i, &jpg, &len)) return false;
    if (s_jpeg->openFLASH((uint8_t*)jpg, (int)len, draw)) {
        s_jpeg->setPixelType(RGB565_BIG_ENDIAN);   // as pushImage sends it as-is
        s_jpeg->decode(0, 0, 0);
        s_jpeg->close();
    }
    return true;
}

static void task(void*) {
    const uint32_t t0 = s_t0;
    int shown = -1, drawn = 0;
    uint32_t busy = 0;
    for (;;) {
        const int i = introclock::frameAt(millis() - t0, montage::fps(), montage::count());
        if (i >= montage::count()) break;
        if (i != shown) {                         // late frames are skipped, not queued
            const uint32_t f0 = millis();
            renderTerm(f0 - t0);
            if (show(i)) {
                busy += millis() - f0;
                ++drawn;
            }
            shown = i;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    Serial.printf("[intro] %d of %d frames in %lu ms (%lu ms a frame)\n", drawn, montage::count(),
                  (unsigned long)(millis() - t0), (unsigned long)(drawn ? busy / drawn : 0));
    // Not online yet: keep the last frame up with the terminal (RETRYING...), redrawn at 5 fps.
    const uint32_t h0 = millis();
    while (!s_online()) {
        renderTerm(millis() - t0);
        show(montage::count() - 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    Serial.printf("[intro] held %lu ms for WiFi\n", (unsigned long)(millis() - h0));
    freeTerm();
    freeJpeg();
    s_playing = false;
    vTaskDelete(nullptr);
}

// Parses o's clip into ui/montage's reader (the boot montage parses its own after).
static bool load(intropick::Opening o) {
    const media::Blob b = o == intropick::MAX ? MEDIA(data_max_bin, "max.bin")
                        : o == intropick::KISS ? MEDIA(data_intro_bin, "intro.bin")
                        : media::Blob{nullptr, 0};
    return montage::parse(b.data, b.size) && montage::count() > 0;
}

bool available(intropick::Opening o) { return load(o); }

void start(TFT_eSPI& t, bool (*online)(), intropick::Opening o) {
    if (!load(o)) {
        Serial.println("[intro] none");
        return;
    }
    if (!newJpeg()) {
        Serial.println("[intro] no memory for the JPEG decoder: skipped");
        return;
    }
    s_tft = &t;
    s_online = online;
    s_playing = true;
    t.fillScreen(TFT_BLACK);
    s_t0 = millis();
    bootterm::begin(online);
    s_term = new TFT_eSprite(&t);
    s_term->setColorDepth(16);
    s_termPx = (uint16_t*)s_term->createSprite(bootterm::W, bootterm::H);   // ~25 KB; no memory = no terminal
    if (!s_termPx) freeTerm();
    // The sound starts first: started after the decode task (priority 2, busy from its first
    // frame), its task got going late and played the 14 s clip in 24 s, full of gaps.
    greeting::playIntro(o);
    if (xTaskCreatePinnedToCore(task, "intro", 6144, nullptr, 2, nullptr, 1) != pdPASS) {
        s_playing = false;     // wait() would spin forever and the deck never boot
        freeTerm();
        freeJpeg();
        Serial.println("[intro] no memory for its task: skipped (its sound still plays)");
    }
}

bool playing() { return s_playing; }

void wait() {
    while (s_playing) delay(20);
}
}
