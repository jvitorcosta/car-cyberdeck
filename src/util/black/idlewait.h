#pragma once
#include <cstdint>
// When the deck gives up on the song and shows "Waiting for a song...": at once when Spotify has
// nothing at all, or after the song has stayed paused for PAUSE_MS (a short pause keeps the
// deck). PURE, host-tested; the UI loop feeds it every pass.
namespace idlewait {
constexpr uint32_t PAUSE_MS = 30000;

// The waiting screen's hint for a phone that dropped off Spotify (it plays on, but the account
// shows no active device until the app is touched): from HINT_AFTER_MS on screen, HINT_SHOW_MS
// out of every HINT_EVERY_MS on the detail line. PHONE_HINT fits the line (233 px of 236).
constexpr uint32_t HINT_AFTER_MS = 30000, HINT_EVERY_MS = 20000, HINT_SHOW_MS = 8000;
constexpr const char* PHONE_HINT = "Phone playing? Pause and play once.";
inline bool hintNow(uint32_t waitingMs) {
    return waitingMs >= HINT_AFTER_MS && (waitingMs - HINT_AFTER_MS) % HINT_EVERY_MS < HINT_SHOW_MS;
}

class Clock {
public:
    // paused: a song is there but not playing; stopped: Spotify has nothing. True: "Waiting for a song...".
    bool update(bool paused, bool stopped, uint32_t now);
private:
    bool timing_ = false;
    uint32_t since_ = 0;
};
}
