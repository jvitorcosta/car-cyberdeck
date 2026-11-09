#include "keepalive.h"

namespace keepalive {

Ask due(const State& s, bool want, uint32_t nowMs, int light) {
    if (!want) return s.asked ? Ask::Off : Ask::None;
    if (!s.asked || light != s.sentLight) return Ask::On;
    return static_cast<int32_t>(nowMs - s.nextAt) >= 0 ? Ask::On : Ask::None;
}

}  // namespace keepalive
