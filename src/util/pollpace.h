#pragma once
#include <cstdint>
// How often the companion asks the new deck for /state: every second, every 5 s after ten
// failures in a row (the deck is off or out of range), back to every second on the first
// answer. PURE, host-tested (test_lyricclock).
namespace pollpace {
constexpr uint32_t FAST_MS = 1000, SLOW_MS = 5000;
constexpr int MISSES_FOR_SLOW = 10;
class Pace {
public:
    void onResult(bool ok) { misses_ = ok ? 0 : misses_ + 1; }
    uint32_t intervalMs() const { return misses_ >= MISSES_FOR_SLOW ? SLOW_MS : FAST_MS; }
    int misses() const { return misses_; }
private:
    int misses_ = 0;
};
}
