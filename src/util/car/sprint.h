#pragma once
#include <cstddef>
#include <cstdint>
// The 0-100 km/h timer (the Suki HUD's STAGE bars, black/ui/sukiview): armed by a stop, it starts
// when the speed crosses MOVING and ends when it crosses TARGET, both crossing times interpolated
// between the speed samples (they come ~3 a second). A stop, a gap in the samples, an unknown speed
// or a pull longer than MAX_MS cancels the run; a new one needs a stop first. last() and best() are
// in ms (0 = none yet), this power-on only. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Suki look")
namespace sprint {
class Timer {
public:
    static constexpr float MOVING = 2.0f, TARGET = 100.0f;   // km/h
    static constexpr uint32_t GAP_MS = 2000, MAX_MS = 30000;
    void update(float kmh, uint32_t nowMs);
    bool running() const { return state_ == State::Running; }
    uint32_t elapsed(uint32_t nowMs) const { return running() ? nowMs - startMs_ : 0; }
    uint32_t last() const { return last_; }
    uint32_t best() const { return best_; }
private:
    enum class State : uint8_t { Idle, Armed, Running };
    State state_ = State::Idle;
    bool have_ = false;   // a previous sample
    float prevKmh_ = 0.0f;
    uint32_t prevMs_ = 0, startMs_ = 0, last_ = 0, best_ = 0;
};
// ms as a lap time, "ss : cc" ("08 : 92"), "-- : --" for 0, "99 : 99" past it; "" when n is too small.
void text(uint32_t ms, char* out, size_t n);
}  // namespace sprint
