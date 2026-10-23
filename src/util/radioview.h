#pragma once
#include <cstddef>
#include <cstdint>
#include "stateparse.h"
// What the yellow deck's RADIO screen shows of the black deck's GET /state: the song position
// moved on between polls, the clock text, the block bar's filled cells, the state word and the
// lyric line due now. PURE, host-tested.
namespace radioview {
// The song position sincePollMs after the poll that brought `d`: while playing it moves on,
// capped at durationMs (when the deck knows it); paused it stays.
int progressMs(const stateparse::Deck& d, uint32_t sincePollMs);
// "3:07" (m:ss); from an hour on "1:02:03". Whole seconds, rounded down. Cut to n bytes.
void clock(uint32_t ms, char* buf, size_t n);
// Filled cells of an n-cell bar at pos of dur (full only at the end); 0 when dur is 0.
int cells(uint32_t pos, uint32_t dur, int n);
// ">WRONG KEY" (the deck turned the key down), ">NO SIGNAL" (no state came), ">STANDBY"
// (paused, or no track) or "ON AIR".
const char* state(bool answered, bool wrongKey, bool playing, bool hasTrack);
// The synced lyric line due at posMs: `line` from lineStartMs, `next` from nextStartMs (when
// known, > 0); "" before the line, and for plain / instrumental / no lyrics.
const char* lyric(const stateparse::Deck& d, uint32_t posMs);
}
