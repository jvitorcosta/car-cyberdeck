#pragma once
// Decides which of the PS3 controller's buttons count as pressed. The DS3 sets a button's on/off
// bit at the lightest touch, so the deck acted on things nobody meant: setting the pad down on a
// table pressed six buttons at pressures of 13-30, resting thumbs held the D-pad, and one bit stayed
// on for minutes. Measured 2026-11-12; see docs/superpowers/specs/2026-11-12-ps3-pad-redesign-design.md.
// The rules, in order:
//   1. A button counts past a quarter of its travel (L2/R2 past ~200), and lets go only well below.
//   2. While 3 or more bits are on in one report, however light, no new press counts. It counts
//      bits, not presses, or it would never see a set-down; "fewer than 3" rather than "all
//      released", so one stuck bit can't lock the pad.
//   3. Lying still on a table is "resting"; leaving rest or arriving at it pauses new presses for
//      half a second. Hand or road motion never rests, so it never pauses.
//   4. A button held for 5 s is ignored until it is let go.
// Presses act the moment they pass the line, so the pad still feels instant. PURE, host-tested.
#include <cstdint>
#include <cstdlib>

namespace padguard {
// The bit layout onPadReport has always used. HOME is the PS button (the ESP32 SDK defines PS).
enum Bit : uint8_t {
    SELECT = 0, START = 3, UP = 4, RIGHT = 5, DOWN = 6, LEFT = 7,
    L2 = 8, R2 = 9, L1 = 10, R1 = 11, TRIANGLE = 12, CIRCLE = 13, CROSS = 14, SQUARE = 15, HOME = 16,
    N_BITS = 17
};

constexpr uint8_t PRESS_AT = 64, RELEASE_AT = 24;
constexpr uint8_t TRIGGER_PRESS_AT = 200, TRIGGER_RELEASE_AT = 60;
constexpr int CHORD = 3;
constexpr uint32_t STUCK_MS = 5000, PAUSE_MS = 500, REST_MS = 1000;
constexpr int REST_BAND = 12;  // |dx|+|dy|+|dz| from the rest reference, raw accelerometer units

// One report from the pad. pressure[b] is that button's pressure; buttons without one (START,
// SELECT, the PS button) report 255 while their bit is on.
struct Report {
    uint32_t bits;
    uint8_t pressure[N_BITS];
    int16_t ax, ay, az;
    uint32_t now;
};

class Guard {
public:
    // The bits that count as held right now.
    uint32_t feed(const Report& r) {
        rest(r);
        if (pausing_ && !paused(r.now)) pausing_ = false;  // or it would come back after 24 days
        const bool chord = popcount(r.bits) >= CHORD;
        const bool hold = chord || paused(r.now);
        uint32_t out = 0;
        stuck_ = 0;
        for (int b = 0; b < N_BITS; b++) {
            const bool trigger = b == L2 || b == R2;
            const uint8_t p = r.pressure[b];
            const uint32_t m = 1u << b;
            if (held_ & m) {
                if (p < (trigger ? TRIGGER_RELEASE_AT : RELEASE_AT)) { held_ &= ~m; continue; }
            } else {
                if (hold || p < (trigger ? TRIGGER_PRESS_AT : PRESS_AT)) continue;
                held_ |= m;
                since_[b] = r.now;
            }
            if (static_cast<int32_t>(r.now - since_[b]) >= static_cast<int32_t>(STUCK_MS)) stuck_ |= m;
            else out |= m;
        }
        return out;
    }

    void reset() { *this = Guard(); }

    bool resting() const { return resting_; }

    bool paused(uint32_t now) const {
        return pausing_ && static_cast<int32_t>(now - pauseUntil_) < 0;
    }

    // The bits rule 4 is ignoring right now.
    uint32_t stuck() const { return stuck_; }

private:
    static int popcount(uint32_t v) {
        int n = 0;
        for (; v; v &= v - 1) n++;
        return n;
    }

    void pause(uint32_t now) {
        pausing_ = true;
        pauseUntil_ = now + PAUSE_MS;
    }

    void rest(const Report& r) {
        const int moved = std::abs(r.ax - refX_) + std::abs(r.ay - refY_) + std::abs(r.az - refZ_);
        if (!haveRef_ || moved > REST_BAND) {
            haveRef_ = true;
            refX_ = r.ax; refY_ = r.ay; refZ_ = r.az;
            restSince_ = r.now;
            if (resting_) { resting_ = false; pause(r.now); }
        } else if (!resting_ && static_cast<int32_t>(r.now - restSince_) >= static_cast<int32_t>(REST_MS)) {
            resting_ = true;
            pause(r.now);
        }
    }

    uint32_t held_ = 0, stuck_ = 0;
    uint32_t since_[N_BITS] = {};
    bool haveRef_ = false, resting_ = false, pausing_ = false;
    int16_t refX_ = 0, refY_ = 0, refZ_ = 0;
    uint32_t restSince_ = 0, pauseUntil_ = 0;
};
}  // namespace padguard
