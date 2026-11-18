#pragma once
#include <cmath>
#include <cstdint>
#include "obdplan.h"
// The gauge screen's street panel (black/ui/gaugeview): the navigator's mood from the engine, her
// blinks and glances, the stripes behind her that run with the speed, and the RPM trace above her
// (the last ~12 s as mirrored bars). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Street panel")
namespace streetface {
enum class Mood : uint8_t {
    Look,    // no adapter yet (OFF / SEARCH, Link::Off / Connecting): she glances left and right
    Sleep,   // the adapter answers, the engine is off (KEY ON)
    Calm,    // engine running, under CALM_BELOW rpm (or rpm unknown)
    Focus,   // CALM_BELOW .. ANGRY_FROM
    Angry,   // ANGRY_FROM and up: brows down, teeth gritted
};
constexpr float CALM_BELOW = 2500.0f;
constexpr float ANGRY_FROM = 5000.0f;   // gaugehud::AMBER_FROM: the ramp turns pink there too
Mood mood(obdplan::Link link, float rpm);

// Eyes shut for BLINK_MS once every BLINK_EVERY_MS (asleep: always shut).
constexpr uint32_t BLINK_EVERY_MS = 3700, BLINK_MS = 130;
bool eyesShut(Mood m, uint32_t nowMs);
// Where her pupils look, -GLANCE..+GLANCE px: a slow sweep while she waits for the adapter, else 0.
constexpr int GLANCE = 3;
int glance(Mood m, uint32_t nowMs);

// The stripes' scroll: PX_PER_KMH px a second per km/h, wrapped to 0..PERIOD-1. Unknown or
// negative speed: they stand still. A gap in the calls (the screen was off) doesn't jump.
class Stripes {
public:
    static constexpr float PX_PER_KMH = 1.2f;   // 100 km/h: 120 px/s
    static constexpr int PERIOD = 24;           // stripe pitch, px
    int update(float kmh, uint32_t nowMs);
private:
    float pos_ = 0.0f;
    uint32_t atMs_ = 0;
    bool started_ = false;
};

// The RPM trace: one sample per SAMPLE_MS, the newest on the right. height(i) is bar i's half
// height (the bars are mirrored about the middle), 0..hMax from IDLE_FLOOR to gaugehud::RPM_MAX (so
// the swings between idle and a pull show); an unknown rpm (or a bar not yet sampled) is -1 (a dot).
class Trace {
public:
    static constexpr int N = 32;
    static constexpr uint32_t SAMPLE_MS = 375;   // 32 bars: 12 s
    static constexpr float IDLE_FLOOR = 500.0f;
    void update(float rpm, uint32_t nowMs);
    void reset() { count_ = 0; head_ = 0; }   // the screen comes back: no stale bars
    int height(int i, int hMax) const;
private:
    float rpm_[N];
    int head_ = 0, count_ = 0;   // head_: where the next sample goes
    uint32_t atMs_ = 0;
};
}  // namespace streetface
