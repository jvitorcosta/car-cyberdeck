#pragma once
#include <cstdint>
// Getting back to the preferred network (config.h order: the iPhone hotspot first). On a
// fallback network the deck scans in the background every SCAN_MS and moves back once the
// preferred network was seen strong enough in SEEN_NEEDED scans in a row (no flapping at the
// edge of range); a move that doesn't stick backs the scans off to 2, then 5 minutes. On the
// preferred network it never scans. PURE, host-tested.
namespace roam {
constexpr uint32_t SCAN_MS = 60000;
constexpr int MIN_RSSI = -75;     // dBm: weaker sightings don't count
constexpr int SEEN_NEEDED = 2;

// Connect tries for network `index` after a drop from `lastIndex` (-1 = boot): two for the
// preferred network the deck was just on (a short hotspot blip), else one.
inline int tries(int index, int lastIndex) { return (index == 0 && lastIndex == 0) ? 2 : 1; }

class Roam {
public:
    // Connected to network `index` (0 = preferred).
    void onConnected(int index, uint32_t nowMs) {
        index_ = index;
        since_ = nowMs;
        seen_ = 0;
        if (index == 0) fails_ = 0;
    }
    // On a fallback and the wait since the last scan (or connect) is over.
    bool wantScan(uint32_t nowMs) const { return index_ > 0 && nowMs - since_ >= waitMs(); }
    // A scan finished: whether the preferred network was seen, and its signal. True = move back now.
    bool onScan(bool seen, int rssi, uint32_t nowMs) {
        since_ = nowMs;
        seen_ = (seen && rssi >= MIN_RSSI) ? seen_ + 1 : 0;
        if (seen_ < SEEN_NEEDED) return false;
        seen_ = 0;
        return true;
    }
    // A move back didn't stick (the deck is on a fallback again): scan less often.
    void onMoveFailed(uint32_t nowMs) {
        ++fails_;
        since_ = nowMs;
        seen_ = 0;
    }
    uint32_t waitMs() const { return fails_ == 0 ? SCAN_MS : fails_ == 1 ? 2 * 60000u : 5 * 60000u; }

private:
    int index_ = -1, seen_ = 0, fails_ = 0;
    uint32_t since_ = 0;
};
}
