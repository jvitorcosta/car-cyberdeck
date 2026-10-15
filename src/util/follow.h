#pragma once
#include <cstdint>
// The companion follows the new deck onto its network: the new deck leads (it needs the
// internet and roams back to the hotspot itself); the companion never roams on its own. When the
// new deck has been unreachable for LOST_MS, the companion switches to the other network to look
// there; further switches back off (1, 2, then 5 minutes apart) so a deck that is simply off
// doesn't make it hop all night. Reaching the deck resets everything. PURE, host-tested.
namespace follow {
constexpr uint32_t LOST_MS = 30000;

class Follow {
public:
    // One poll result; true = switch to the other network now.
    bool update(bool deckOk, uint32_t nowMs) {
        if (deckOk) {
            lost_ = false;
            switches_ = 0;
            return false;
        }
        if (!lost_) {
            lost_ = true;
            lostSince_ = nowMs;
            return false;
        }
        if (nowMs - lostSince_ < LOST_MS) return false;
        if (switches_ > 0 && nowMs - lastSwitch_ < gapMs()) return false;
        ++switches_;
        lastSwitch_ = nowMs;
        return true;
    }

private:
    uint32_t gapMs() const { return switches_ == 1 ? 60000u : switches_ == 2 ? 120000u : 300000u; }
    bool lost_ = false;
    int switches_ = 0;
    uint32_t lostSince_ = 0, lastSwitch_ = 0;
};
}
