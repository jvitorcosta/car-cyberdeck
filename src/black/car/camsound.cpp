#include "camsound.h"
#include <Arduino.h>
#include <freertos/queue.h>
#include <math.h>
#include "camsvc.h"
#include "../audio/greeting.h"
#include "../audio/sink.h"
#include "../core/media.h"
#include "../../util/car/camclip.h"

MEDIA_DECL(data_camvoice_bin);

namespace camsound {

constexpr int QUEUE_LEN = 8;             // events come about once a second at most
constexpr int CHUNK = 128;               // samples per audiosink::write
// Our call, to tune by ear in the car: about -6 dBFS, under the clips' peaks (they are near full
// scale) but well over the speaker's noise.
constexpr float LEVEL = 16000.0f;
constexpr uint32_t RAMP_MS = 4;          // fade in and out: a tone cut at full level clicks
constexpr uint32_t TAIL_MS = 150;        // pause margin past the first step (the DMA queue's play-out)
constexpr int OUTPUT_TRIES = 30;         // x 10 ms: a clip may be opening or closing the output
// Every this many writes, a step reads the live stage too (camsvc's lock): an alarm whose "-> off"
// was dropped by a full queue stops within ~64 ms at 16 kHz.
constexpr int LIVE_EVERY = 8;
// While an alarm holds the output open between beeps: how often the task checks whether to let it go
// (the alarm over, a clip waiting for it).
constexpr uint32_t HOLD_CHECK_MS = 100;

static QueueHandle_t s_q = nullptr;
// The task's own: the samples on their way out, what to play when, the clips in the media pack.
static int16_t s_buf[CHUNK];
static camseq::Seq s_seq;
static camclip::Table s_clips;

void onStage(const camseq::Event& e) {
    if (!s_q) return;
    // Never waits (the GPS task's time). A full queue drops it: camseq checks the live stage
    // before every alarm slot and alert line, so a lost "-> off" can't keep the sound going.
    xQueueSend(s_q, &e, 0);
}

void onMark(bool saved) {
    camseq::Event e;
    e.mark = saved ? camseq::MARK_SAVED : camseq::MARK_FAILED;
    onStage(e);
}

// The queued events into the sequencer.
static void drain() {
    camseq::Event e;
    while (xQueueReceive(s_q, &e, 0) == pdTRUE) s_seq.onEvent(e, millis());
}

// Between two writes: stop the step playing now for mute (all but the alarm's beeps), for a
// higher step that just came due, or for an alarm that ended (silence at once).
static int s_writes = 0;
static bool cut(const camseq::Step& s) {
    drain();
    if (++s_writes % LIVE_EVERY == 0) {
        cameras::Status st;
        camsvc::snapshot(&st, nullptr, nullptr);
        s_seq.sync(st.stage);
    }
    return (s.muteable && camsvc::muted()) || s_seq.stops(s) || s_seq.cuts(s.prio, millis());
}

// One note as a sine (or silence for hz 0) at the output's rate. False: cut short.
static bool tone(const camtone::Note& n, int rate, const camseq::Step& s) {
    const uint32_t total = (uint32_t)n.ms * (uint32_t)rate / 1000;
    const uint32_t ramp = RAMP_MS * (uint32_t)rate / 1000;
    const float step = 2.0f * (float)M_PI * n.hz / (float)rate;
    float phase = 0.0f;
    for (uint32_t i = 0; i < total;) {
        if (cut(s)) return false;
        size_t k = 0;
        for (; k < (size_t)CHUNK && i < total; ++k, ++i) {
            if (!n.hz) {
                s_buf[k] = 0;
                continue;
            }
            float env = 1.0f;
            if (i < ramp) env = (float)i / ramp;
            else if (total - i < ramp) env = (float)(total - i) / ramp;
            s_buf[k] = (int16_t)(LEVEL * env * sinf(phase));
            phase += step;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }
        audiosink::write(s_buf, k);
    }
    return true;
}

static void play(const camseq::Step& s, int rate) {
    if (s.clip) {
        camclip::Reader r(s_clips.get(s.slot), rate, audiosink::GREETING_VOLUME);
        for (size_t k; (k = r.fill(s_buf, CHUNK)) > 0;) {
            if (cut(s)) return;
            audiosink::write(s_buf, k);
        }
        return;
    }
    for (int i = 0; i < s.n; ++i)
        if (!tone(s.notes[i], rate, s)) return;
}

static uint32_t lengthMs(const camseq::Step& s) {
    if (s.clip) return camclip::lengthMs(s_clips.get(s.slot));
    uint32_t ms = 0;
    for (int i = 0; i < s.n; ++i) ms += s.notes[i].ms;
    return ms;
}

static bool nextStep(camseq::Step* s) {
    cameras::Status st;
    camlist::Point pt;
    camsvc::snapshot(&st, &pt, nullptr);   // the live stage and point: stale lines are dropped
    return s_seq.next(st.stage, st.index >= 0 ? pt.id : "", camsvc::muted(), millis(), s);
}

// Plays `s` and every step due right after it on one output (no clip resuming between the chime and
// the voice). Returns whether the output may still be held open (an alarm's next slot).
static bool session(camseq::Step s) {
    greeting::pauseFor(lengthMs(s) + TAIL_MS);
    int rate = 0;
    for (int t = 0; t < OUTPUT_TRIES && !(rate = greeting::beginTones()); ++t) vTaskDelay(pdMS_TO_TICKS(10));
    int steps = 0;
    bool quiet = true;                      // only the alarm's repeats: not logged (one line per 1.5 s)
    if (rate) {
        for (;;) {
            play(s, rate);
            ++steps;
            if (s.clip || s.prio != camseq::ALARM) quiet = false;
            drain();
            if (s_seq.waitMs(millis()) != 0 || !nextStep(&s)) break;
        }
    }
    // Still in the alarm: the output stays open for the next slot (no I2S driver install, codec
    // reset and amp click every 1.5 s).
    const bool keep = s_seq.alarming();
    greeting::endTones(keep);               // ends the pause too, also when no output was had
    if (!rate) Serial.println("[cam] sound lost: no audio output");
    else if (!quiet)
        Serial.printf("[cam] sound: %d step(s) at %d Hz (stackfree=%u)\n", steps, rate,
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    return keep && rate;
}

static void task(void*) {
    // The clips where the media pack maps them, looked up here rather than in setup(): the first
    // get() checks the file's CRC.
    const media::Blob b = MEDIA(data_camvoice_bin, "camvoice.bin");
    if (s_clips.load(b.data, b.size)) {
        int n = 0;
        for (uint16_t m = s_clips.present(); m; m >>= 1) n += m & 1;
        Serial.printf("[cam] voice: %d of %d clips\n", n, camclip::COUNT);
    } else {
        Serial.println("[cam] voice: no camvoice.bin, beeps only");
    }
    s_seq.clips(s_clips.present());
    bool mayHold = false;                   // the last session may have kept the output open
    for (;;) {
        // Idle on the queue; with steps pending, until the next is due; holding the output, at
        // least every HOLD_CHECK_MS.
        uint32_t wait = s_seq.waitMs(millis());
        if (mayHold && wait > HOLD_CHECK_MS) wait = HOLD_CHECK_MS;
        camseq::Event e;
        if (xQueueReceive(s_q, &e, wait == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(wait)) == pdTRUE) {
            s_seq.onEvent(e, millis());
            drain();
        }
        camseq::Step s;
        if (nextStep(&s)) {
            mayHold = session(s);
        } else if (mayHold && (!s_seq.alarming() || greeting::clipWaiting())) {
            greeting::releaseTones();
            mayHold = false;
        }
    }
}

void begin() {
    if (s_q) return;
    s_q = xQueueCreate(QUEUE_LEN, sizeof(camseq::Event));
    if (!s_q) {
        Serial.println("[cam] no sound: no memory for the queue");
        return;
    }
    // Priority 2, over the clip tasks (1): the alert's writes go first while a clip waits parked.
    if (xTaskCreate(task, "camsnd", 3072, nullptr, 2, nullptr) != pdPASS) {
        vQueueDelete(s_q);
        s_q = nullptr;
        Serial.println("[cam] no sound: no memory for the task");
    }
}

}  // namespace camsound
