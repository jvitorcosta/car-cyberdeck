#pragma once
#include <cstdint>
#include "../wire/lyricstatus.h"
// The black deck's karaoke switch and which screen it gives (black/core/karaoke). The yellow deck
// holds the switch, like music mode: GET /karaoke?on=1 at once and every 10 s while it is on, on=0
// to stop. KEEP_MS without one and it is off. While on, each song decides: the karaoke view while
// its lyrics are searched for or synced, the "no lyrics" message for NO_LYRICS_MS from the moment it
// is known to have none (plain lyrics count as none: nothing to sweep), then the normal screen until
// the next song. Spec: docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md.
// PURE, host-tested.
namespace karaokemode {
constexpr uint32_t KEEP_MS = 30000;
constexpr uint32_t NO_LYRICS_MS = 30000;

enum class Screen : uint8_t { Normal, Karaoke, NoLyrics };

class Mode {
public:
    void keepAlive(uint32_t nowMs) {
        if (!on(nowMs)) noneSeen_ = false;   // switched on again: a song without lyrics says so again
        on_ = true;
        aliveAt_ = nowMs;
    }
    void stop() { on_ = false; }
    bool on(uint32_t nowMs) const { return on_ && nowMs - aliveAt_ < KEEP_MS; }

    // Call it for the song shown (its generation) as the screen is drawn; it keeps the timer.
    Screen screen(lyricstatus::Status s, uint32_t gen, uint32_t nowMs) {
        if (gen != gen_) {
            gen_ = gen;
            noneSince_ = 0;
            noneSeen_ = false;
        }
        if (!on(nowMs)) return Screen::Normal;
        if (s == lyricstatus::Status::Searching || s == lyricstatus::Status::Retrying ||
            s == lyricstatus::Status::Synced)
            return Screen::Karaoke;
        if (!noneSeen_) {
            noneSeen_ = true;
            noneSince_ = nowMs;
        }
        return nowMs - noneSince_ < NO_LYRICS_MS ? Screen::NoLyrics : Screen::Normal;
    }

private:
    bool on_ = false;
    uint32_t aliveAt_ = 0;
    uint32_t gen_ = 0;
    bool noneSeen_ = false;
    uint32_t noneSince_ = 0;
};

// The yellow deck's INSIDE light with each on=1 (light=N): a ledlook swatch 0..MAX_LIGHT, or -1 for
// off, no link, or anything else (missing, empty, not a plain number, out of range).
constexpr int MAX_LIGHT = 8;
inline int parseLight(const char* s) {
    if (!s || s[0] < '0' || s[0] > '9' || s[1]) return -1;  // one digit and nothing after it
    const int n = s[0] - '0';
    return n <= MAX_LIGHT ? n : -1;
}
}  // namespace karaokemode
