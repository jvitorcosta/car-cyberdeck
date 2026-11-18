#include "streetface.h"
#include "gaugehud.h"

namespace streetface {
Mood mood(obdplan::Link link, float rpm) {
    if (link == obdplan::Link::Off || link == obdplan::Link::Connecting) return Mood::Look;
    if (link == obdplan::Link::NoEngine) return Mood::Sleep;
    if (!std::isfinite(rpm) || rpm < CALM_BELOW) return Mood::Calm;
    return rpm < ANGRY_FROM ? Mood::Focus : Mood::Angry;
}

bool eyesShut(Mood m, uint32_t nowMs) {
    if (m == Mood::Sleep) return true;
    return nowMs % BLINK_EVERY_MS < BLINK_MS;
}

int glance(Mood m, uint32_t nowMs) {
    if (m != Mood::Look) return 0;
    // a slow triangle wave, 2.4 s a sweep, held a moment at each end
    const int t = (int)(nowMs % 2400);
    const int tri = t < 1200 ? t : 2400 - t;   // 0..1200..0
    const int x = (tri - 600) * (GLANCE + 2) / 600;   // a little past +-GLANCE, then clamped
    return x > GLANCE ? GLANCE : x < -GLANCE ? -GLANCE : x;
}

int Stripes::update(float kmh, uint32_t nowMs) {
    if (started_) {
        uint32_t dt = nowMs - atMs_;
        if (dt > 250) dt = 250;   // a long gap: don't leap
        if (std::isfinite(kmh) && kmh > 0.0f) pos_ += kmh * PX_PER_KMH * (float)dt / 1000.0f;
        pos_ = std::fmod(pos_, (float)PERIOD);
    }
    started_ = true;
    atMs_ = nowMs;
    return (int)pos_ % PERIOD;
}

void Trace::update(float rpm, uint32_t nowMs) {
    if (count_ > 0 && nowMs - atMs_ < SAMPLE_MS) return;
    atMs_ = count_ > 0 ? atMs_ + SAMPLE_MS : nowMs;
    if (nowMs - atMs_ >= SAMPLE_MS) atMs_ = nowMs;   // fell behind (the screen was off): restart the beat
    rpm_[head_] = rpm;
    head_ = (head_ + 1) % N;
    if (count_ < N) ++count_;
    ++taken_;
}

int Trace::height(int i, int hMax) const {
    if (i < 0 || i >= N) return -1;
    const int age = N - 1 - i;   // bar N-1 is the newest
    if (age >= count_) return -1;
    const float r = rpm_[(head_ - 1 - age + 2 * N) % N];
    if (!std::isfinite(r)) return -1;
    float f = (r - IDLE_FLOOR) / (gaugehud::RPM_MAX - IDLE_FLOOR);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return (int)std::lround(f * (float)hMax);
}
}  // namespace streetface
