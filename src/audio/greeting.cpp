#include "greeting.h"
#include <Arduino.h>
#include <atomic>
#include <cstring>
#include "sink.h"
#include "../util/loudness.h"
#include "../util/howl.h"
#include "../util/greetpick.h"
#include "../core/embed.h"

EMBED(data_greeting_pcm);
EMBED(data_finale_pcm);
EMBED(data_opener_pcm);
#ifdef BOARD_S3
EMBED(data_intro_pcm);
EMBED(data_howl_pcm);
EMBED(data_greeting_alt_pcm);
#endif

namespace greeting {

constexpr int CLIP_RATE = 16000;         // tools/gen_greeting.py output rate (16-bit signed LE)
constexpr int INTRO_RATE = 48000;        // tools/gen_intro.py and tools/gen_howl.py output rate
constexpr int CHUNK = 128;               // samples per audiosink::write

// Volumes are per board (audiosink): the CYD's DAC distorts with level, the S3's codec doesn't.
// data == nullptr: synthesised while it plays (util/howl) instead of read from flash.
struct Clip { const uint8_t* data; size_t samples; const char* name; const int* volume; int rate; };
static const Clip GREETING = {EMBED_DATA(data_greeting_pcm),
                              EMBED_SIZE(data_greeting_pcm) / 2,
                              "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};
static const Clip FINALE = {EMBED_DATA(data_finale_pcm),
                            EMBED_SIZE(data_finale_pcm) / 2,
                            "finale", &audiosink::FINALE_VOLUME, CLIP_RATE};
static const Clip OPENER = {EMBED_DATA(data_opener_pcm),
                            EMBED_SIZE(data_opener_pcm) / 2,
                            "opener", &audiosink::GREETING_VOLUME, CLIP_RATE};
static_assert(howl::RATE == CLIP_RATE, "the synthesised howl plays at the clip rate");
static const Clip HOWL = {nullptr, howl::SAMPLES, "howl", &audiosink::GREETING_VOLUME, CLIP_RATE};
#ifdef BOARD_S3
static const Clip INTRO = {EMBED_DATA(data_intro_pcm),
                           EMBED_SIZE(data_intro_pcm) / 2,
                           "intro", &audiosink::GREETING_VOLUME, INTRO_RATE};
// A real wolf (tools/gen_howl.py); the synthesised HOWL when the build has none.
static const Clip WOLF = {EMBED_DATA(data_howl_pcm),
                          EMBED_SIZE(data_howl_pcm) / 2,
                          "howl", &audiosink::GREETING_VOLUME, INTRO_RATE};
// The second greeting (McQueen's "Eu sou a velocidade"), picked at random at boot.
static const Clip GREETING_ALT = {EMBED_DATA(data_greeting_alt_pcm),
                                  EMBED_SIZE(data_greeting_alt_pcm) / 2,
                                  "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};
#endif

// An optional clip the build may lack: the pre-build scripts write one silent sample instead.
static bool present(const Clip& c) { return c.samples > 1; }
static uint32_t lengthMs(const Clip& c) { return (uint32_t)((uint64_t)c.samples * 1000 / c.rate); }

static const Clip* pickGreeting() {
#ifdef BOARD_S3
    return greetpick::pick(esp_random(), present(GREETING_ALT)) ? &GREETING_ALT : &GREETING;
#else
    return &GREETING;
#endif
}

// This boot's greeting, picked once (a function-local static: initialised once, thread-safe):
// play(), durationMs() and level() (the boot scene's timing and car lights) must agree on it.
static const Clip& greetingClip() {
    static const Clip* const c = pickGreeting();
    return *c;
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
    static howl::Synth synth;                            // one clip plays at a time (s_output)
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

void play() { start(&greetingClip(), "greeting"); }

void playHowl() {
#ifdef BOARD_S3
    if (present(WOLF)) { start(&WOLF, "howl"); return; }
#endif
    start(&HOWL, "howl");
}

void playFinale() { if (present(FINALE)) start(&FINALE, "finale"); }

void playIntro() {
#ifdef BOARD_S3
    if (present(INTRO)) start(&INTRO, "intro");
#endif
}

void playOpener() { if (present(OPENER)) start(&OPENER, "opener"); }

uint32_t openerMs() { return present(OPENER) ? lengthMs(OPENER) : 0; }

uint32_t durationMs() { return lengthMs(greetingClip()); }

bool playing() { return s_playing.load() > 0; }

uint32_t finaleMs() { return present(FINALE) ? lengthMs(FINALE) : 0; }

uint8_t level(uint32_t ms) {
    const Clip& c = greetingClip();
    const size_t at = (size_t)((uint64_t)ms * c.rate / 1000);
    return loudness::loudness(c.data, c.samples, at, c.rate / 50);
}

}
