#pragma once
#include <cstdint>
// The S3's boot intro video plays by the clock: frame i is due at i * 1000 / fps, so a
// slow decode skips frames instead of slowing the clip down. PURE, host-tested.
namespace introclock {
// The frame to show `ms` after the start; == count once the clip is over (also for no clip).
inline int frameAt(uint32_t ms, int fps, int count) {
    if (count <= 0 || fps <= 0) return count > 0 ? count : 0;   // nothing to play: done
    const uint32_t i = (uint32_t)((uint64_t)ms * (uint32_t)fps / 1000u);
    return i >= (uint32_t)count ? count : (int)i;
}
// The longest frame 0 waits for the video's sound (it's out ~0.1 s after the cue).
constexpr uint32_t SOUND_WAIT_MS = 500;
// The video's clock at `now`: ms since its sound's first sample (soundAt, millis(); 0 = not yet,
// one before t0 is an older clip's), so the lips match the voice; 0 (frame 0) until it starts. A
// sound that hasn't started SOUND_WAIT_MS after t0, or a silent video, runs from t0 instead.
inline uint32_t elapsed(uint32_t now, uint32_t t0, uint32_t soundAt, bool hasSound) {
    if (!hasSound) return now - t0;
    if (soundAt != 0 && soundAt >= t0) return now > soundAt ? now - soundAt : 0;
    return now - t0 > SOUND_WAIT_MS ? now - t0 - SOUND_WAIT_MS : 0;
}
}
