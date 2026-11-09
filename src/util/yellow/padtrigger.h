#pragma once
// L2 and R2 of the PS3 controller by their pressure (0-255) instead of their on/off bit: the DS3
// sets that bit at the lightest touch, so a finger resting on a trigger changed the preset and
// buzzed. Down past PRESS_AT, up again only under RELEASE_AT, so a trigger held near the line
// doesn't flicker. PURE, host-tested.
#include <cstdint>

namespace padtrigger {
constexpr uint8_t PRESS_AT = 120, RELEASE_AT = 40;

class Trigger {
public:
    bool feed(uint8_t pressure) {
        if (!down_ && pressure >= PRESS_AT) down_ = true;
        else if (down_ && pressure < RELEASE_AT) down_ = false;
        return down_;
    }

private:
    bool down_ = false;
};
}  // namespace padtrigger
