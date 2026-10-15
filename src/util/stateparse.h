#pragma once
#include <cstdint>
// The new deck's GET /state, as the companion screen needs it. PURE, host-tested.
namespace stateparse {
struct Deck {
    uint32_t trackGen, progressMs, durationMs, tripS, lineStartMs, nextStartMs;
    bool playing, hasVroom;
    char track[96], artist[96], status[24], detail[48], lyricKind[16], line[160], next[160];
};
// False (out untouched) for anything that isn't a complete JSON object: a truncated body on a
// weak link, an HTML captive portal, empty. Missing optional keys get neutral defaults (an
// older deck): numbers 0, strings "", lyricKind "none".
bool parse(const char* body, Deck& out);
}
