#pragma once
#include <cstdint>
// When the deck gives up on the song and shows "Nothing playing": at once when Spotify has
// nothing at all, or after the song has stayed paused for PAUSE_MS (a short pause keeps the
// deck). PURE, host-tested; the UI loop feeds it every pass.
namespace idlewait {
constexpr uint32_t PAUSE_MS = 30000;

class Clock {
public:
    // paused: a song is there but not playing; stopped: Spotify has nothing. True: "Nothing playing".
    bool update(bool paused, bool stopped, uint32_t now);
private:
    bool timing_ = false;
    uint32_t since_ = 0;
};
}
