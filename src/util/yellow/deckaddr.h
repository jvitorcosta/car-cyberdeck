#pragma once
#include <cstdint>
// Where the yellow deck finds the black deck. PURE, host-tested.
// Off the hotspot: by its mDNS name, the address kept through short blips (the new deck's
// background scan drops about one poll a minute), so the name is looked up again only after MISSES
// polls in a row without a valid answer (or after a network switch).
// On the iPhone hotspot: at its fixed address (BLACK_DECK_ADDR) first; after MISSES polls there
// without a valid answer (a black deck that landed on another address, a passenger's phone holding
// .10), by name like off the hotspot; back to the fixed address when the name isn't found or its
// address misses MISSES polls in turn.
namespace deckaddr {
constexpr int MISSES = 3;

class Keep {
public:
    void found(uint32_t addr) {
        addr_ = addr;
        misses_ = 0;
    }
    // valid: the deck answered as itself. Counts for the looked-up address when there is one, else
    // for the fixed address (only the hotspot asks with no looked-up address).
    void onPoll(bool valid) {
        if (addr_ == 0) {
            fixedMisses_ = valid ? 0 : fixedMisses_ + 1;
            return;
        }
        if (valid) misses_ = 0;
        else if (++misses_ >= MISSES) forget();
    }
    // On the hotspot, with no looked-up address: ask the fixed address (else look the name up).
    bool tryFixed() const { return fixedMisses_ < MISSES; }
    // The name wasn't found: on the hotspot the fixed address gets its turn again.
    void lookupFailed() { fixedMisses_ = 0; }
    void forget() {
        addr_ = 0;
        misses_ = 0;
        fixedMisses_ = 0;
    }
    uint32_t addr() const { return addr_; }   // 0 = none looked up

private:
    uint32_t addr_ = 0;
    int misses_ = 0;
    int fixedMisses_ = 0;
};
}
