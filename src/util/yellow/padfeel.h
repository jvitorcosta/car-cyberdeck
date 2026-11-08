#pragma once
// What the PS3 controller tells the hand on the yellow deck, so a shortcut needs no look at the
// screen: one short buzz when a command went to lights that are all linked, two when one it was
// for isn't; and its four player LEDs: 1 = connected to the deck (always lit, like a console's
// player 1), 2 = the inside light linked, 3 = the outside one, 4 = the pad's own battery low.
// PURE, host-tested.
#include <cstdint>

namespace padfeel {
constexpr uint32_t BUZZ_MS = 80;    // one buzz
constexpr uint32_t BUZZ_GAP_MS = 140;  // from one buzz's start to the next's

// Buzzes for a command that went to the inside and/or outside light (`toIn`, `toOut`).
inline int buzzes(bool toIn, bool toOut, bool inUp, bool outUp) {
    return (toIn && !inUp) || (toOut && !outUp) ? 2 : 1;
}

// The LED bits (led1 = bit 0 .. led4 = bit 3) of a connected controller.
inline uint8_t leds(bool inUp, bool outUp, bool batteryLow) {
    return static_cast<uint8_t>(1 | (inUp ? 2 : 0) | (outUp ? 4 : 0) | (batteryLow ? 8 : 0));
}

// A run of buzzes, BUZZ_GAP_MS apart: start() it, then each tick due() says whether to send one now.
class Buzz {
public:
    void start(int n, uint32_t now) {
        left_ = n;
        at_ = now;
    }
    bool due(uint32_t now) {
        if (left_ <= 0 || static_cast<int32_t>(now - at_) < 0) return false;
        left_--;
        at_ = now + BUZZ_GAP_MS;
        return true;
    }
    bool busy() const { return left_ > 0; }

private:
    int left_ = 0;
    uint32_t at_ = 0;
};
}  // namespace padfeel
