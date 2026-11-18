#include "sprint.h"
#include <cmath>
#include <cstdio>

namespace sprint {

// When the speed crossed v between the previous sample and this one (linear).
static uint32_t crossing(float v, float v0, uint32_t t0, float v1, uint32_t t1) {
    if (v1 <= v0) return t1;
    float f = (v - v0) / (v1 - v0);
    f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    return t0 + (uint32_t)lroundf(f * (float)(t1 - t0));
}

void Timer::update(float kmh, uint32_t nowMs) {
    if (std::isnan(kmh) || (have_ && nowMs - prevMs_ > GAP_MS)) {
        if (state_ == State::Running) state_ = State::Idle;   // can't time what we didn't see
        have_ = !std::isnan(kmh);
        if (have_) {
            prevKmh_ = kmh;
            prevMs_ = nowMs;
            if (kmh < MOVING) state_ = State::Armed;
        }
        return;
    }
    const bool stopped = kmh < MOVING;
    switch (state_) {
        case State::Idle:
            if (stopped) state_ = State::Armed;
            break;
        case State::Armed:
            if (kmh >= TARGET) {
                state_ = State::Idle;   // 0 to 100 in one sample: a glitch, not a run
            } else if (!stopped) {
                startMs_ = have_ ? crossing(MOVING, prevKmh_, prevMs_, kmh, nowMs) : nowMs;
                state_ = State::Running;
            }
            break;
        case State::Running:
            if (stopped) {
                state_ = State::Armed;
            } else if (kmh >= TARGET) {
                const uint32_t end = crossing(TARGET, prevKmh_, prevMs_, kmh, nowMs);
                last_ = end - startMs_;
                if (best_ == 0 || last_ < best_) best_ = last_;
                state_ = State::Idle;
            } else if (nowMs - startMs_ > MAX_MS) {
                state_ = State::Idle;
            }
            break;
    }
    have_ = true;
    prevKmh_ = kmh;
    prevMs_ = nowMs;
}

void text(uint32_t ms, char* out, size_t n) {
    if (!out || n == 0) return;
    out[0] = '\0';
    if (n < 8) return;
    if (ms == 0) {
        snprintf(out, n, "-- : --");
        return;
    }
    uint32_t cs = ms / 10;   // cut, like a stopwatch
    if (cs > 9999) cs = 9999;
    snprintf(out, n, "%02u : %02u", (unsigned)(cs / 100), (unsigned)(cs % 100));
}

}  // namespace sprint
