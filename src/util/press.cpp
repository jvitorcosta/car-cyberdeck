#include "press.h"

namespace press {
int Press::update(bool down, int under, bool hold, uint32_t nowMs) {
    if (down && !down_) {                    // touch-down: this press belongs to `under`
        down_ = true;
        fired_ = false;
        btn_ = under;
        hold_ = hold;
        last_ = under;
        t0_ = nowMs;
        return 0;
    }
    if (down) {
        if (under == btn_ && last_ != btn_) t0_ = nowMs;   // back on the button: a new full second
        last_ = under;
        if (btn_ && hold_ && !fired_ && under == btn_ && nowMs - t0_ >= HOLD_MS) {
            fired_ = true;
            return btn_;
        }
        return 0;
    }
    if (!down_) return 0;
    down_ = false;                           // release
    const int b = btn_;
    btn_ = 0;
    return (b && !hold_ && last_ == b) ? b : 0;
}

float Press::holdProgress(uint32_t nowMs) const {
    if (!down_ || !hold_ || !btn_ || last_ != btn_) return 0.0f;
    const float p = (float)(nowMs - t0_) / (float)HOLD_MS;
    return p > 1.0f ? 1.0f : p;
}
}
