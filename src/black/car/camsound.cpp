#include "camsound.h"
#include <Arduino.h>
#include <freertos/queue.h>
#include <math.h>
#include "camsvc.h"
#include "../audio/greeting.h"
#include "../audio/sink.h"
#include "../../util/car/camcue.h"

namespace camsound {

constexpr int QUEUE_LEN = 8;             // changes come about once a second at most
constexpr int CHUNK = 128;               // samples per audiosink::write
// Our call, to tune by ear in the car: about -6 dBFS, under the clips' peaks (they are near full
// scale) but well over the speaker's noise.
constexpr float LEVEL = 16000.0f;
constexpr uint32_t RAMP_MS = 4;          // fade in and out: a tone cut at full level clicks
constexpr uint32_t TAIL_MS = 150;        // pause margin past the notes (the DMA queue's play-out)
constexpr int OUTPUT_TRIES = 30;         // x 10 ms: a clip may be opening or closing the output
// While an alarm holds the output open between beeps: how often the task checks whether to let it go
// (the alarm over, mute, a clip waiting for it).
constexpr uint32_t HOLD_CHECK_MS = 100;

struct Change {
    cameras::Stage from, to;
    camlist::Kind kind;
};
static QueueHandle_t s_q = nullptr;
static int16_t s_buf[CHUNK];             // the one task's

void onStage(cameras::Stage from, cameras::Stage to, camlist::Kind kind) {
    if (!s_q) return;
    const Change c{from, to, kind};
    // Never waits (the GPS task's time). A full queue drops it: camcue's tick() checks the live
    // stage before every repeat, so a lost "-> off" can't keep an alarm going.
    xQueueSend(s_q, &c, 0);
}

static bool muted() {
    bool m = false;
    camsvc::snapshot(nullptr, nullptr, &m);
    return m;
}

// One note as a sine (or silence for hz 0) at the output's rate.
static void tone(const camtone::Note& n, int rate) {
    const uint32_t total = (uint32_t)n.ms * (uint32_t)rate / 1000;
    const uint32_t ramp = RAMP_MS * (uint32_t)rate / 1000;
    const float step = 2.0f * (float)M_PI * n.hz / (float)rate;
    float phase = 0.0f;
    for (uint32_t i = 0; i < total;) {
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
}

// keep: hold an output opened for the beeps until the next one (an alarm), see greeting::endTones.
static void play(const camtone::Note* notes, int n, bool log, bool keep) {
    uint32_t ms = 0;
    for (int i = 0; i < n; ++i) ms += notes[i].ms;
    greeting::pauseFor(ms + TAIL_MS);
    int rate = 0;
    for (int t = 0; t < OUTPUT_TRIES && !(rate = greeting::beginTones()); ++t) vTaskDelay(pdMS_TO_TICKS(10));
    if (rate) {
        for (int i = 0; i < n; ++i) {
            if (muted()) break;          // muted mid-pattern: stop at the next note
            tone(notes[i], rate);
        }
    }
    greeting::endTones(keep);            // ends the pause too, also when no output was had
    if (!rate) Serial.println("[cam] beep lost: no audio output");
    else if (log)
        Serial.printf("[cam] beep %d notes at %d Hz (stackfree=%u)\n", n, rate,
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

static void task(void*) {
    static camcue::Cue cue;
    camtone::Note notes[camtone::MAX_NOTES];
    bool mayHold = false;                    // the last beep may have kept the output open
    for (;;) {
        // Idle on the queue; in an alarm, until its next repeat slot; holding the output, at least
        // every HOLD_CHECK_MS.
        uint32_t wait = cue.waitMs(millis());
        if (mayHold && wait > HOLD_CHECK_MS) wait = HOLD_CHECK_MS;
        Change c;
        const bool got =
            xQueueReceive(s_q, &c, wait == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(wait)) == pdTRUE;
        cameras::Status st;
        camlist::Point pt;
        bool mute = false;
        camsvc::snapshot(&st, &pt, &mute);   // the point just entered (for the chime guard), mute
        const uint32_t now = millis();
        const int n = got ? cue.onChange(c.from, c.to, c.kind, st.index >= 0 ? pt.id : "", mute, now, notes)
                          : cue.tick(st.stage, mute, now, notes);
        // Still in the alarm and heard: the output stays open for the next triple beep (no I2S
        // driver install, codec reset and amp click every 1.5 s).
        const bool keep = !mute && cue.waitMs(now) != UINT32_MAX;
        if (n > 0) {
            play(notes, n, got, keep);       // the repeats aren't logged: one line per 1.5 s is noise
            mayHold = keep;
        } else if (mayHold && (!keep || greeting::clipWaiting())) {
            greeting::releaseTones();
            mayHold = false;
        }
    }
}

void begin() {
    if (s_q) return;
    s_q = xQueueCreate(QUEUE_LEN, sizeof(Change));
    if (!s_q) {
        Serial.println("[cam] no beeps: no memory for the queue");
        return;
    }
    // Priority 2, over the clip tasks (1): the beeps' writes go first while a clip waits parked.
    if (xTaskCreate(task, "camsnd", 3072, nullptr, 2, nullptr) != pdPASS) {
        vQueueDelete(s_q);
        s_q = nullptr;
        Serial.println("[cam] no beeps: no memory for the task");
    }
}

}  // namespace camsound
