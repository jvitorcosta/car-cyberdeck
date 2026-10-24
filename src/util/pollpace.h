#pragma once
#include <cstdint>
// How often the companion asks the new deck for /state. On the KARAOKE screen: every second, every
// 5 s after ten failures in a row (the deck is off or out of range), back to every second on the
// first answer. On the Menu: every 5 s, every 30 s after those ten failures. Off the hotspot each
// failed poll would also look the deck's name up (mDNS, up to 1.5 s), so while it keeps missing the
// lookups are 30 s apart; the first one after an answer, or after the KARAOKE screen opens (a fresh
// Pace), is at once. PURE, host-tested (test_lyricclock).
namespace pollpace {
constexpr uint32_t FAST_MS = 1000, SLOW_MS = 5000;
constexpr uint32_t MENU_MS = 5000, MENU_SLOW_MS = 30000;
constexpr uint32_t LOOKUP_MS = 30000;
constexpr int MISSES_FOR_SLOW = 10;
class Pace {
public:
    void onResult(bool ok) {
        misses_ = ok ? 0 : misses_ + 1;
        if (ok) looked_ = false;
    }
    uint32_t intervalMs() const { return misses_ >= MISSES_FOR_SLOW ? SLOW_MS : FAST_MS; }
    uint32_t menuIntervalMs() const { return misses_ >= MISSES_FOR_SLOW ? MENU_SLOW_MS : MENU_MS; }
    int misses() const { return misses_; }
    // The name may be looked up now: always, until the deck has missed ten polls in a row; then
    // only LOOKUP_MS after the last lookup.
    bool mayLookup(uint32_t now) const {
        return misses_ < MISSES_FOR_SLOW || !looked_ || now - lookedAt_ >= LOOKUP_MS;
    }
    void lookedUp(uint32_t now) {
        looked_ = true;
        lookedAt_ = now;
    }
private:
    int misses_ = 0;
    bool looked_ = false;
    uint32_t lookedAt_ = 0;
};
}
