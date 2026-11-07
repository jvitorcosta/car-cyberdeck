#include "greeting.h"
#include <Arduino.h>
#include <atomic>
#include <cstring>
#include "sink.h"
#include "../../util/black/loudness.h"
#include "../../util/black/howl.h"
#include "../../util/black/intropick.h"
#include "../../util/black/sndroster.h"
#include "../core/media.h"

MEDIA_DECL(data_greeting_pcm);
MEDIA_DECL(data_finale_pcm);
MEDIA_DECL(data_opener_pcm);
MEDIA_DECL(data_intro_pcm);
MEDIA_DECL(data_max_pcm);
MEDIA_DECL(data_howl_pcm);
MEDIA_DECL(data_greeting_alt_pcm);
MEDIA_DECL(data_drive_snd);
MEDIA_DECL(data_opener_snd);
MEDIA_DECL(data_finale_snd);
MEDIA_DECL(data_connect_snd);
// In the app, so a deck with an empty media partition still boots with sound.
EMBED(data_greeting_default_pcm);

namespace greeting {

constexpr int CLIP_RATE = 16000;         // tools/gen_greeting.py output rate (16-bit signed LE)
constexpr int INTRO_RATE = 48000;        // tools/gen_intro.py and tools/gen_howl.py output rate
constexpr int CHUNK = 128;               // samples per audiosink::write

// data == nullptr: synthesised while it plays (util/howl) instead of read from flash.
struct Clip { const uint8_t* data; size_t samples; const char* name; const int* volume; int rate; };
static Clip clip(media::Blob b, const char* name, const int* volume, int rate) {
    return {b.data, b.size / 2, name, volume, rate};
}
static const int INTRO_VOLUME = 50;      // percent: the intro video's song, quieter than the greeting
static_assert(howl::RATE == CLIP_RATE, "the synthesised howl plays at the clip rate");
static const Clip HOWL = {nullptr, howl::SAMPLES, "howl", &audiosink::GREETING_VOLUME, CLIP_RATE};

struct Clips {
    Clip greeting, finale, opener;
    Clip intro;
    Clip max;            // Max Verstappen's F1 intro, the other video opening
    Clip wolf;           // a real wolf (tools/gen_howl.py); the synthesised HOWL when the build has none
    Clip greetingAlt;    // the second greeting (McQueen's "Eu sou a velocidade"), picked at random at boot
    media::Blob roster[4];   // per Slot: data/<slot>.snd (util/sndroster), {nullptr, 0} without one
};

// Looked up once, on first use (a function-local static: thread-safe).
static const Clips& clips() {
    static const Clips c = [] {
        Clips k;
        media::Blob g = MEDIA(data_greeting_pcm, "greeting.pcm");
        if (g.size <= 2) g = {EMBED_DATA(data_greeting_default_pcm), EMBED_SIZE(data_greeting_default_pcm)};
        k.greeting = clip(g, "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE);
        k.finale = clip(MEDIA(data_finale_pcm, "finale.pcm"), "finale", &audiosink::FINALE_VOLUME, CLIP_RATE);
        k.opener = clip(MEDIA(data_opener_pcm, "opener.pcm"), "opener", &audiosink::GREETING_VOLUME, CLIP_RATE);
        k.intro = clip(MEDIA(data_intro_pcm, "intro.pcm"), "intro", &INTRO_VOLUME, INTRO_RATE);
        k.max = clip(MEDIA(data_max_pcm, "max.pcm"), "intro", &INTRO_VOLUME, INTRO_RATE);
        k.wolf = clip(MEDIA(data_howl_pcm, "howl.pcm"), "howl", &audiosink::GREETING_VOLUME, INTRO_RATE);
        k.greetingAlt = clip(MEDIA(data_greeting_alt_pcm, "greeting_alt.pcm"), "greeting",
                             &audiosink::GREETING_VOLUME, CLIP_RATE);
        k.roster[DRIVE] = MEDIA(data_drive_snd, "drive.snd");
        k.roster[OPENER] = MEDIA(data_opener_snd, "opener.snd");
        k.roster[FINALE] = MEDIA(data_finale_snd, "finale.snd");
        k.roster[CONNECT] = MEDIA(data_connect_snd, "connect.snd");
        return k;
    }();
    return c;
}

// An optional clip the build may lack: the pre-build scripts write one silent sample instead.
static bool present(const Clip& c) { return c.samples > 1; }
static uint32_t lengthMs(const Clip& c) { return (uint32_t)((uint64_t)c.samples * 1000 / c.rate); }

// This boot's greeting (main.cpp chooses it from the opening before play()); MAIN by default.
static intropick::Greeting s_choice = intropick::MAIN;
static const Clip SILENT = {nullptr, 0, "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};

void choose(intropick::Greeting g) { s_choice = g; }

// This boot's roster clips (pick()); static: a playing task keeps the pointer.
static Clip s_picked[4];
static bool s_hasPick[4] = {false, false, false, false};

static bool rosterClip(Slot s, int i, sndroster::Clip* out) {
    if (s > CONNECT) return false;
    const media::Blob& b = clips().roster[s];
    return sndroster::at(b.data, b.size, i, out);
}

int rosterCount(Slot s) { return s > CONNECT ? 0 : sndroster::count(clips().roster[s].data, clips().roster[s].size); }

uint32_t rosterMs(Slot s, int i) {
    sndroster::Clip c;
    return rosterClip(s, i, &c) ? (uint32_t)((uint64_t)(c.bytes / 2) * 1000 / CLIP_RATE) : 0;
}

const char* rosterName(Slot s, int i) {
    sndroster::Clip c;
    return rosterClip(s, i, &c) ? c.name : "?";
}

void pick(Slot s, int i) {
    if (s > CONNECT) return;
    static const char* const NAMES[4] = {"greeting", "opener", "finale", "connect"};
    const int* volume = s == FINALE ? &audiosink::FINALE_VOLUME : &audiosink::GREETING_VOLUME;
    sndroster::Clip c;
    if (rosterClip(s, i, &c)) {
        s_picked[s] = {c.data, c.bytes / 2, NAMES[s], volume, CLIP_RATE};
        s_hasPick[s] = true;
    } else {                         // with a roster, no fitting clip is no sound (not the single file)
        s_picked[s] = {nullptr, 0, NAMES[s], volume, CLIP_RATE};
        s_hasPick[s] = rosterCount(s) > 0;
    }
}

// The clip play(), durationMs() and level() agree on (the boot scene's timing and car lights).
static const Clip& greetingClip() {
    if (s_choice == intropick::NONE) return SILENT;
    if (s_choice == intropick::ALT && present(clips().greetingAlt)) return clips().greetingAlt;
    if (s_hasPick[DRIVE] && present(s_picked[DRIVE])) return s_picked[DRIVE];
    return clips().greeting;
}

static const Clip& openerClip() { return s_hasPick[OPENER] ? s_picked[OPENER] : clips().opener; }
static const Clip& finaleClip() { return s_hasPick[FINALE] ? s_picked[FINALE] : clips().finale; }

// Clips playing (task alive, audio output open). Atomic: callers on core 1, clip tasks on either core.
static std::atomic<int> s_playing{0};
// One clip at a time on the audio output (audiosink's contract): a clip that starts while another
// still plays waits here instead of colliding with it, and so do the camera beeps (beginTones) when
// no clip plays. Created on first use (a function-local static: thread-safe).
static SemaphoreHandle_t output() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

// The clip whose sound came out last and when (millis(), after the amp's wake): the pictures that
// follow a sound (car lights, intro video) run on this clock, not on their own play() call's.
static portMUX_TYPE s_heardMux = portMUX_INITIALIZER_UNLOCKED;
static const Clip* s_heard = nullptr;
static uint32_t s_heardAt = 0;
static std::atomic<uint32_t> s_cuedAt{0};               // the last start(): for the log

static uint32_t heardAt(const Clip* c) {
    portENTER_CRITICAL(&s_heardMux);
    const uint32_t at = s_heard == c ? s_heardAt : 0;
    portEXIT_CRITICAL(&s_heardMux);
    return at;
}

// The camera beeps cutting in over a clip (pauseFor / beginTones / endTones). The clip checks
// between its 128-sample writes; parked, it writes nothing (the port plays silence on underflow)
// and keeps its output open for the beeps. One spinlock over the lot, so a clip can't resume in the
// instant the beeps take its output.
static portMUX_TYPE s_parkMux = portMUX_INITIALIZER_UNLOCKED;
static bool s_paused = false;           // pauseFor() asked, until s_pauseUntil
static uint32_t s_pauseUntil = 0;
static bool s_borrowed = false;         // the beeps write on the parked clip's open output
static int s_parkedRate = 0;            // a clip is parked, its output open at this rate; 0 = none
static std::atomic<int> s_openRate{0};  // a clip has its output open (parked or not); 0 = none
static bool s_ownTones = false;         // beginTones opened the output itself (no clip played)

static void holdWhilePaused(int rate) {
    for (;;) {
        const uint32_t now = millis();
        portENTER_CRITICAL(&s_parkMux);
        const bool hold = s_borrowed || (s_paused && (int32_t)(s_pauseUntil - now) > 0);
        if (!hold) s_paused = false;
        s_parkedRate = hold ? rate : 0;
        portEXIT_CRITICAL(&s_parkMux);
        if (!hold) return;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void task(void* arg) {
    const Clip* c = (const Clip*)arg;
    const int volume = *c->volume;
    xSemaphoreTake(output(), portMAX_DELAY);
    const uint32_t cued = s_cuedAt.load();
    const uint32_t t0 = millis();
    if (!audiosink::open(c->rate)) {
        xSemaphoreGive(output());
        s_playing.fetch_sub(1);
        vTaskDelete(nullptr);
    }
    s_openRate.store(c->rate);
    const uint32_t out = millis();                       // the amp is awake: the first sample is out
    portENTER_CRITICAL(&s_heardMux);
    s_heard = c;
    s_heardAt = out ? out : 1;
    portEXIT_CRITICAL(&s_heardMux);
    int16_t buf[CHUNK];
    static howl::Synth synth;                            // one clip plays at a time (output())
    if (!c->data) synth = howl::Synth();
    for (size_t i = 0; i < c->samples;) {
        size_t n = 0;
        if (!c->data) {
            n = synth.fill(buf, CHUNK);
            if (n == 0) break;
            i += n;
            for (size_t k = 0; k < n; ++k) buf[k] = (int16_t)((int32_t)buf[k] * volume / 1000);
        }
        for (; c->data && n < CHUNK && i < c->samples; ++n, ++i) {
            int16_t s;
            memcpy(&s, c->data + 2 * i, 2);              // embedded data has no alignment guarantee
            buf[n] = (int16_t)((int32_t)s * volume / 100);
        }
        holdWhilePaused(c->rate);                        // a camera beep goes first
        audiosink::write(buf, n);
    }
    s_openRate.store(0);
    audiosink::close();
    xSemaphoreGive(output());
    Serial.printf("[greeting] %s: out %u ms after its cue, played %u samples in ~%u ms (clip %u ms)\n",
                  c->name, (unsigned)(out - cued), (unsigned)c->samples, (unsigned)(millis() - t0),
                  (unsigned)(c->samples * 1000 / c->rate));
    s_playing.fetch_sub(1);
    vTaskDelete(nullptr);
}

static void start(const Clip* c, const char* name) {
    if (!output()) {
        Serial.printf("[greeting] %s: no memory for the output's lock\n", name);
        return;
    }
    s_cuedAt.store(millis());
    s_playing.fetch_add(1);
    if (xTaskCreate(task, name, 3072, const_cast<Clip*>(c), 1, nullptr) != pdPASS) {
        s_playing.fetch_sub(1);              // never started: don't keep the boot scene waiting
        Serial.printf("[greeting] %s: no memory for its task\n", name);
    }
}

void play() {
    if (s_choice == intropick::NONE) return;   // the videos have their own sound
    start(&greetingClip(), "greeting");
}

void playHowl() {
    if (present(clips().wolf)) { start(&clips().wolf, "howl"); return; }
    start(&HOWL, "howl");
}

void playFinale() { if (present(finaleClip())) start(&finaleClip(), "finale"); }

bool playIntro(intropick::Opening o) {
    const Clip& c = o == intropick::MAX ? clips().max : clips().intro;
    if (!present(c)) return false;
    start(&c, "intro");
    return true;
}

uint32_t introStartedAt() {
    const uint32_t a = heardAt(&clips().intro);
    return a ? a : heardAt(&clips().max);
}

void playOpener() { if (present(openerClip())) start(&openerClip(), "opener"); }

void playConnect() { if (s_hasPick[CONNECT] && present(s_picked[CONNECT])) start(&s_picked[CONNECT], "connect"); }

uint32_t openerMs() { return present(openerClip()) ? lengthMs(openerClip()) : 0; }

uint32_t durationMs() { return lengthMs(greetingClip()); }

bool playing() { return s_playing.load() > 0; }

bool warm() { return audiosink::warm(CLIP_RATE); }

uint32_t finaleMs() { return present(finaleClip()) ? lengthMs(finaleClip()) : 0; }

uint8_t level() {
    const Clip& c = greetingClip();
    const uint32_t heard = heardAt(&c);
    if (!heard) return 0;                                // not out yet (or another clip since)
    const uint32_t ms = millis() - heard;
    const size_t at = (size_t)((uint64_t)ms * c.rate / 1000);
    return loudness::loudness(c.data, c.samples, at, c.rate / 50);
}

void pauseFor(uint32_t ms) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_parkMux);
    s_paused = true;
    s_pauseUntil = now + ms;
    portEXIT_CRITICAL(&s_parkMux);
    // Back once a clip with its output open has stopped writing: it checks every 128 samples
    // (3-8 ms), so 200 ms is only a bound against a stuck write.
    for (int k = 0; k < 40; ++k) {
        portENTER_CRITICAL(&s_parkMux);
        const bool parked = s_parkedRate != 0;
        portEXIT_CRITICAL(&s_parkMux);
        if (parked || s_openRate.load() == 0) return;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

int beginTones() {
    if (s_ownTones) return CLIP_RATE;    // still open from the last beep (endTones(true))
    int rate = 0;
    portENTER_CRITICAL(&s_parkMux);
    if (s_parkedRate) {
        s_borrowed = true;               // keeps the clip parked until endTones, whatever pauseFor said
        rate = s_parkedRate;
    }
    portEXIT_CRITICAL(&s_parkMux);
    if (rate) return rate;
    // No clip parked. The lock free means none plays: open the output at the clips' rate (the mic's
    // too, so music mode's mic can share the port). Taken means a clip is opening or closing it.
    if (!output() || xSemaphoreTake(output(), 0) != pdTRUE) return 0;
    if (!audiosink::open(CLIP_RATE)) {
        xSemaphoreGive(output());
        return 0;
    }
    s_ownTones = true;
    return CLIP_RATE;
}

void releaseTones() {
    if (!s_ownTones) return;
    s_ownTones = false;
    audiosink::close();                  // plays out the queued beep first
    xSemaphoreGive(output());
}

void endTones(bool keepOpen) {
    // Kept open, the port plays silence between beeps (tx_desc_auto_clear); a clip waiting for the
    // output (counted in s_playing from its start) gets it now instead.
    if (!keepOpen || clipWaiting()) releaseTones();
    portENTER_CRITICAL(&s_parkMux);
    s_borrowed = false;                  // a parked clip carries on behind the beep in the DMA queue
    s_paused = false;
    portEXIT_CRITICAL(&s_parkMux);
}

bool clipWaiting() { return s_ownTones && s_playing.load() > 0; }

}
