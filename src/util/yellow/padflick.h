#pragma once
// A flick of the PS3 controller's right stick, left or right: the previous / next colour. It counts
// only near the edge of the stick's travel, once per flick, and the stick must come back near the
// centre before the next one, so drift (the sticks rest up to ~20 off centre) or holding the stick
// over does nothing more. PURE, host-tested.
#include <cstdint>

namespace padflick {
constexpr int EDGE = 90;    // ~70 % of the stick's 127
constexpr int CENTRE = 30;  // back under this: the next flick can count

class Flick {
public:
    // The stick's X (-128..127): -1 / +1 once as a flick reaches the edge, else 0.
    int feed(int8_t rx) {
        const int v = rx;
        if (!armed_) {
            if (v > -CENTRE && v < CENTRE) armed_ = true;
            return 0;
        }
        if (v >= EDGE || v <= -EDGE) {
            armed_ = false;
            return v > 0 ? 1 : -1;
        }
        return 0;
    }

    void reset() { armed_ = true; }

private:
    bool armed_ = true;
};
}  // namespace padflick
