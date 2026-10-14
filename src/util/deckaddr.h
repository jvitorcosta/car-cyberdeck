#pragma once
#include <cstdint>
// The companion's looked-up address of the new deck (mDNS, off the hotspot), kept through short
// blips: the new deck's background scan drops about one poll a minute, so the name is looked up
// again only after MISSES failed polls in a row (or after a network switch). PURE, host-tested.
namespace deckaddr {
constexpr int MISSES = 3;

class Keep {
public:
    void found(uint32_t addr) {
        addr_ = addr;
        misses_ = 0;
    }
    void onPoll(bool ok) {
        if (ok) misses_ = 0;
        else if (++misses_ >= MISSES) forget();
    }
    void forget() {
        addr_ = 0;
        misses_ = 0;
    }
    uint32_t addr() const { return addr_; }   // 0 = look it up

private:
    uint32_t addr_ = 0;
    int misses_ = 0;
};
}
