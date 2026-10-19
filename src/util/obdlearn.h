#pragma once
#include <cmath>
// What the gauge cluster learns while it drives (spec 2026-10-19-gauge-cluster): the pedal's and
// throttle's real travel, so both bars read 0-100 like for like, and the battery's dip while the
// engine cranks ("AT START"). PURE, host-tested.
namespace obdlearn {
// Maps a raw position % onto 0-100 between the lowest and highest seen, starting from lo0/hi0
// (raw pedal/throttle PIDs rarely read 0-100: ~15 % released, ~80 % floored).
class Range {
public:
    Range(float lo0, float hi0) : lo_(lo0), hi_(hi0) {}
    float scale(float raw);
private:
    float lo_, hi_;
};

// The engine as the link last knew it: Unknown until RPM has answered (or the ECU is known to be
// silent: Off), then Off (RPM 0 / no ECU) or Running (RPM > 0).
enum class Engine { Unknown, Off, Running };

// The battery's dip while the engine cranks. feed() every battery reading with the engine's state.
// value(): the lowest Off reading before the first Running one; NAN until the engine has started,
// and NAN for the whole power-on when no Off reading came first (the deck came up after the crank:
// missed, never a resting or charging voltage shown as the dip). Unknown readings don't count.
class StartDip {
public:
    void feed(float volts, Engine engine);
    float value() const { return value_; }
private:
    bool sawOff_ = false, done_ = false;
    float min_ = 99.0f, value_ = NAN;
};
}
