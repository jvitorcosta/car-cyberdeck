#pragma once
#include <cstdint>
// Trip time: seconds since power-on (the car started) or the last reset from the companion's
// Trip button. millis() wrap-safe. PURE, host-tested.
namespace tripclock {
class Clock {
public:
    explicit Clock(uint32_t nowMs = 0) : start_(nowMs) {}
    void reset(uint32_t nowMs) { start_ = nowMs; }
    uint32_t seconds(uint32_t nowMs) const { return (nowMs - start_) / 1000; }
private:
    uint32_t start_;
};
}
