#pragma once
#include <cstdint>
// The companion's boot intro video plays by the clock: frame i is due at i * 1000 / fps, so a
// slow decode skips frames instead of slowing the clip down. PURE, host-tested.
namespace introclock {
// The frame to show `ms` after the start; == count once the clip is over (also for no clip).
inline int frameAt(uint32_t ms, int fps, int count) {
    if (count <= 0 || fps <= 0) return count > 0 ? count : 0;   // nothing to play: done
    const uint32_t i = (uint32_t)((uint64_t)ms * (uint32_t)fps / 1000u);
    return i >= (uint32_t)count ? count : (int)i;
}
}
