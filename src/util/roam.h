#pragma once
#include <cstdint>
// Which network to try how often, and when to look for the hotspot again (config.h order: the
// iPhone hotspot first). PURE, host-tested.
//
// The hotspot is the car's network; the others (home WiFi) are fallbacks. At boot an iPhone
// hotspot may not advertise yet (the phone locked, the hotspot screen closed): one 8 s try lost
// it to the home WiFi, and the deck stayed there. So the hotspot gets three tries at boot, and a
// deck on a fallback looks for it once a minute and moves over when it's in range. (2026-10-14
// had decks stay on whatever they joined; the roadmap of 2026-10-21 brought the move back, with
// a scan first so a hotspot that isn't there costs nothing.)
namespace roam {
constexpr uint32_t LOOK_MS = 60000;   // on a fallback: scan for the hotspot this often

// Connect tries for network `index` after a drop from `lastIndex` (-1 = boot): three for the
// hotspot at boot (~24 s, time to unlock the phone), two after a drop from it, else one.
inline int tries(int index, int lastIndex) {
    if (index != 0) return 1;
    return lastIndex < 0 ? 3 : lastIndex == 0 ? 2 : 1;
}

// On network `index`, `sinceLookMs` after the last scan: time to scan for the hotspot again.
inline bool lookForPreferred(int index, uint32_t sinceLookMs) {
    return index > 0 && sinceLookMs >= LOOK_MS;
}
}
