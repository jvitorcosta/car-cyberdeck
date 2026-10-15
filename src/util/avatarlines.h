#pragma once
#include <cstddef>
#include <cstdint>
// When the deck avatar speaks and what she says: canned lines on boot and, now and then, on a
// new track. PURE, host-tested. Spec: docs/superpowers/specs/2026-10-14-avatar-design.md
namespace avatarlines {
constexpr size_t TEXT_CAP = 160;
constexpr uint32_t GAP_MS = 5UL * 60 * 1000;   // at most one track remark per 5 min
constexpr uint32_t ODDS = 3;                    // then 1 in ODDS track changes
// Boot / Track follow the gap and odds rules; the reactions (Howl ... Unreachable) always speak.
enum class Event : uint8_t { Boot, Track, Howl, Vroom, TripReset, Busy, Unreachable };
struct In {
    Event event;
    bool spokeBefore;       // she has had a scene since boot
    uint32_t sinceLastMs;   // since her last scene started (ignored when !spokeBefore)
    uint32_t rnd;           // a random draw (esp_random on the deck)
    const char* title;      // may be nullptr / ""
    const char* artist;
    const char* poke;       // the walking Pokemon, as stored
};
struct Out {
    uint8_t mood;           // avatarbin::Mood
    char text[TEXT_CAP];
};
// True, with out filled, when she should speak now.
bool pick(const In& in, Out& out);
// Copies tmpl into out (n bytes incl. NUL), replacing {T} with the title, {A} with the artist
// and {P} with the Pokemon in capitals. Empty values: "this song", "this artist", "POKéMON".
// Never cuts a UTF-8 character; other braces are copied as they are.
void fill(const char* tmpl, const char* title, const char* artist, const char* poke, char* out,
          size_t n);
struct Line {
    uint8_t mood;
    const char* text;
};
constexpr int BOOT_N = 3, TRACK_N = 12, REACT_N = 5;
extern const Line BOOT[BOOT_N];
extern const Line TRACK[TRACK_N];
extern const Line REACT[REACT_N];   // Howl, Vroom, TripReset, Busy, Unreachable
}
