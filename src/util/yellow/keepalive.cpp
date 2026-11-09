#include "keepalive.h"

namespace keepalive {

Ask due(const State& s, bool want, uint32_t nowMs, int light) {
    if (!want) return s.asked ? Ask::Off : Ask::None;
    if (!s.asked || light != s.sentLight) return Ask::On;
    return static_cast<int32_t>(nowMs - s.nextAt) >= 0 ? Ask::On : Ask::None;
}

void start(Confirm& c) {
    c.waiting = true;
    c.misses = 0;
}

Shown answer(Confirm& c, int rc) {
    if (!c.waiting) return Shown::None;
    if (rc == 200) {
        c.waiting = false;
        return Shown::Connected;
    }
    return ++c.misses == NO_ANSWER_MISSES ? Shown::NoAnswer : Shown::None;
}

}  // namespace keepalive
