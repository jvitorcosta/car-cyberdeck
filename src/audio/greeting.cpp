#include "greeting.h"
#include <Arduino.h>
#include <atomic>
#include <cstring>
#include "sink.h"
#include "../util/loudness.h"
#include "../util/howl.h"
#include "../util/greetpick.h"

extern const uint8_t _binary_data_greeting_pcm_start[] asm("_binary_data_greeting_pcm_start");
extern const uint8_t _binary_data_greeting_pcm_end[] asm("_binary_data_greeting_pcm_end");
extern const uint8_t _binary_data_finale_pcm_start[] asm("_binary_data_finale_pcm_start");
extern const uint8_t _binary_data_finale_pcm_end[] asm("_binary_data_finale_pcm_end");
extern const uint8_t _binary_data_opener_pcm_start[] asm("_binary_data_opener_pcm_start");
extern const uint8_t _binary_data_opener_pcm_end[] asm("_binary_data_opener_pcm_end");
#ifdef BOARD_S3
extern const uint8_t _binary_data_intro_pcm_start[] asm("_binary_data_intro_pcm_start");
extern const uint8_t _binary_data_intro_pcm_end[] asm("_binary_data_intro_pcm_end");
extern const uint8_t _binary_data_howl_pcm_start[] asm("_binary_data_howl_pcm_start");
extern const uint8_t _binary_data_howl_pcm_end[] asm("_binary_data_howl_pcm_end");
extern const uint8_t _binary_data_greeting_alt_pcm_start[] asm("_binary_data_greeting_alt_pcm_start");
extern const uint8_t _binary_data_greeting_alt_pcm_end[] asm("_binary_data_greeting_alt_pcm_end");
#endif

namespace greeting {

constexpr int CLIP_RATE = 16000;         // tools/gen_greeting.py output rate (16-bit signed LE)
constexpr int INTRO_RATE = 48000;        // tools/gen_intro.py and tools/gen_howl.py output rate
constexpr int CHUNK = 128;               // samples per audiosink::write

// Volumes are per board (audiosink): the CYD's DAC distorts with level, the S3's codec doesn't.
// data == nullptr: synthesised while it plays (util/howl) instead of read from flash.
struct Clip { const uint8_t* data; size_t samples; const char* name; const int* volume; int rate; };
static const Clip GREETING = {_binary_data_greeting_pcm_start,
                              (size_t)(_binary_data_greeting_pcm_end - _binary_data_greeting_pcm_start) / 2,
                              "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};
static const Clip FINALE = {_binary_data_finale_pcm_start,
                            (size_t)(_binary_data_finale_pcm_end - _binary_data_finale_pcm_start) / 2,
                            "finale", &audiosink::FINALE_VOLUME, CLIP_RATE};
static const Clip OPENER = {_binary_data_opener_pcm_start,
                            (size_t)(_binary_data_opener_pcm_end - _binary_data_opener_pcm_start) / 2,
                            "opener", &audiosink::GREETING_VOLUME, CLIP_RATE};
static const Clip HOWL = {nullptr, howl::SAMPLES, "howl", &audiosink::GREETING_VOLUME, CLIP_RATE};
#ifdef BOARD_S3
static const Clip INTRO = {_binary_data_intro_pcm_start,
                           (size_t)(_binary_data_intro_pcm_end - _binary_data_intro_pcm_start) / 2,
                           "intro", &audiosink::GREETING_VOLUME, INTRO_RATE};
// A real wolf (tools/gen_howl.py); the synthesised HOWL when the build has none.
static const Clip WOLF = {_binary_data_howl_pcm_start,
                          (size_t)(_binary_data_howl_pcm_end - _binary_data_howl_pcm_start) / 2,
                          "howl", &audiosink::GREETING_VOLUME, INTRO_RATE};
// The second greeting (McQueen's "Eu sou a velocidade"), picked at random at boot.
static const Clip GREETING_ALT = {_binary_data_greeting_alt_pcm_start,
                                  (size_t)(_binary_data_greeting_alt_pcm_end - _binary_data_greeting_alt_pcm_start) / 2,
                                  "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};
#endif

// This boot's greeting, picked once: play(), durationMs() and level() (the boot scene's timing
// and car lights) must all agree on it.
static const Clip* greetingClip() {
    static const Clip* c = nullptr;
    if (!c) {
#ifdef BOARD_S3
        c = greetpick::pick(esp_random(), GREETING_ALT.samples > 1) ? &GREETING_ALT : &GREETING;
#else
        c = &GREETING;
#endif
    }
    return c;
}

// Clips playing (task alive, audio output open). Atomic: callers on core 1, clip tasks on either core.
static std::atomic<int> s_playing{0};
// One clip at a time on the audio output (audiosink's contract): a clip that starts while another
// still plays waits here instead of colliding with it. Created by the first play*() call (setup()).
static SemaphoreHandle_t s_output = nullptr;


static void task(void* arg) {
    const Clip* c = (const Clip*)arg;
    const int volume = *c->volume;
    xSemaphoreTake(s_output, portMAX_DELAY);
    const uint32_t t0 = millis();
    if (!audiosink::open(c->rate)) {
        xSemaphoreGive(s_output);
        s_playing.fetch_sub(1);
        vTaskDelete(nullptr);
    }
    int16_t buf[CHUNK];
    static howl::Synth synth;                            // one howl at a time (net/remote's gate)
    if (!c->data) synth = howl::Synth();
    for (size_t i = 0; i < c->samples;) {
        size_t n = 0;
        if (!c->data) {
            n = synth.fill(buf, CHUNK);
            if (n == 0) break;
            i += n;
            for (size_t k = 0; k < n; ++k) buf[k] = (int16_t)((int32_t)buf[k] * volume / 100);
        }
        for (; c->data && n < CHUNK && i < c->samples; ++n, ++i) {
            int16_t s;
            memcpy(&s, c->data + 2 * i, 2);              // embedded data has no alignment guarantee
            buf[n] = (int16_t)((int32_t)s * volume / 100);
        }
        audiosink::write(buf, n);
    }
    audiosink::close();
    xSemaphoreGive(s_output);
    Serial.printf("[greeting] %s: played %u samples in ~%u ms (clip %u ms)\n", c->name,
                  (unsigned)c->samples, (unsigned)(millis() - t0),
                  (unsigned)(c->samples * 1000 / c->rate));
    s_playing.fetch_sub(1);
    vTaskDelete(nullptr);
}

static void start(const Clip* c, const char* name) {
    if (!s_output) s_output = xSemaphoreCreateMutex();
    s_playing.fetch_add(1);
    if (xTaskCreate(task, name, 3072, const_cast<Clip*>(c), 1, nullptr) != pdPASS) {
        s_playing.fetch_sub(1);              // never started: don't keep the boot scene waiting
        Serial.printf("[greeting] %s: no memory for its task\n", name);
    }
}

void play() { start(greetingClip(), "greeting"); }

void playHowl() {
#ifdef BOARD_S3
    if (WOLF.samples > 1) { start(&WOLF, "howl"); return; }   // 1 sample = the build's placeholder
#endif
    start(&HOWL, "howl");
}

void playFinale() {
    if (FINALE.samples <= 1) return;                 // 1 sample = the build's placeholder
    start(&FINALE, "finale");
}

void playIntro() {
#ifdef BOARD_S3
    if (INTRO.samples <= 1) return;                  // 1 sample = the build's placeholder
    start(&INTRO, "intro");
#endif
}

void playOpener() {
    if (OPENER.samples <= 1) return;                 // 1 sample = the build's placeholder
    start(&OPENER, "opener");
}

uint32_t openerMs() { return OPENER.samples > 1 ? (uint32_t)((uint64_t)OPENER.samples * 1000 / CLIP_RATE) : 0; }

uint32_t durationMs() { return (uint32_t)((uint64_t)greetingClip()->samples * 1000 / CLIP_RATE); }

bool playing() { return s_playing.load() > 0; }

uint32_t finaleMs() { return FINALE.samples > 1 ? (uint32_t)((uint64_t)FINALE.samples * 1000 / CLIP_RATE) : 0; }

uint8_t level(uint32_t ms) {
    const size_t at = (size_t)((uint64_t)ms * CLIP_RATE / 1000);
    return loudness::loudness(greetingClip()->data, greetingClip()->samples, at, CLIP_RATE / 50);
}

}
