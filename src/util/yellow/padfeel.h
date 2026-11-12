#pragma once
// What the PS3 controller tells the hand on the yellow deck, so a shortcut needs no look at the
// screen. Each kind of action buzzes its own way on the DS3's two motors (the small one is only on
// or off, the big one has a strength): a step (preset, brightness, colour) one short tick, music
// mode two ticks, karaoke one long buzz, the lights on / off one firm buzz. Turning something off
// plays the same shape softer, on the big motor at low strength. When a light the action was for
// isn't linked, one weak long buzz follows (big motor, so it can't pass for music mode's double).
// And its four player LEDs: 1 = connected to the deck (always lit, like a console's player 1),
// 2 = the inside light linked, 3 = the outside one, 4 = the pad's own battery low. PURE, host-tested.
#include <cstdint>

namespace padfeel {
// One buzz: the small motor for smallMs, the big one at bigPower for bigMs (0: that motor off), then
// gapMs of quiet before the next buzz of the same feel.
struct Pulse {
    uint16_t smallMs, bigMs;
    uint8_t bigPower;
    uint16_t gapMs;
};

enum class Feel { Tick, Double, Long, Firm };

constexpr uint16_t TICK_MS = 40, DOUBLE_GAP_MS = 120;
constexpr uint16_t LONG_MS = 400, FIRM_MS = 150;
constexpr uint8_t LONG_POWER = 160, FIRM_POWER = 255;
constexpr uint8_t SOFT_POWER = 90;  // a tick turning something off, on the big motor
constexpr uint16_t PARTIAL_GAP_MS = 160, PARTIAL_MS = 200;
constexpr uint8_t PARTIAL_POWER = 90;

// The LED bits (led1 = bit 0 .. led4 = bit 3) of a connected controller.
inline uint8_t leds(bool inUp, bool outUp, bool batteryLow) {
    return static_cast<uint8_t>(1 | (inUp ? 2 : 0) | (outUp ? 4 : 0) | (batteryLow ? 8 : 0));
}

// A feel's buzzes: start() it, then each tick due() hands over the buzz to send now, if one is due.
// A new start() replaces the one still running.
class Buzz {
public:
    void start(Feel f, bool on, bool partial, uint32_t now) {
        n_ = i_ = 0;
        at_ = now;
        auto tick = [&](uint16_t gap) {
            add(on ? Pulse{TICK_MS, 0, 0, gap} : Pulse{0, TICK_MS, SOFT_POWER, gap});
        };
        switch (f) {
            case Feel::Tick: tick(0); break;
            case Feel::Double: tick(DOUBLE_GAP_MS); tick(0); break;
            case Feel::Long: add(Pulse{0, LONG_MS, static_cast<uint8_t>(on ? LONG_POWER : LONG_POWER / 2), 0}); break;
            case Feel::Firm: add(Pulse{0, FIRM_MS, static_cast<uint8_t>(on ? FIRM_POWER : FIRM_POWER / 2), 0}); break;
        }
        if (partial) {
            p_[n_ - 1].gapMs = PARTIAL_GAP_MS;
            add(Pulse{0, PARTIAL_MS, PARTIAL_POWER, 0});
        }
    }

    bool due(uint32_t now, Pulse* out) {
        if (i_ >= n_ || static_cast<int32_t>(now - at_) < 0) return false;
        *out = p_[i_++];
        const uint16_t len = out->smallMs > out->bigMs ? out->smallMs : out->bigMs;
        at_ = now + len + out->gapMs;
        return true;
    }

    bool busy() const { return i_ < n_; }

private:
    void add(const Pulse& p) {
        if (n_ < MAX) p_[n_++] = p;
    }

    static constexpr int MAX = 3;
    Pulse p_[MAX] = {};
    int n_ = 0, i_ = 0;
    uint32_t at_ = 0;
};
}  // namespace padfeel
