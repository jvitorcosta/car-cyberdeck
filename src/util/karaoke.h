#pragma once
#include <cstddef>
#include <cstdint>
#include "stateparse.h"
// What the yellow deck's KARAOKE screen shows of the black deck's GET /state: whether the song can
// be sung along (synced lyrics), the song position moved on between polls, the progress bar, the
// state word, and the lyric line due now and the one after it (ui/lyricrun wraps them).
// PURE, host-tested.
namespace karaoke {
// The song position sincePollMs after the poll that brought `d`: while playing it moves on,
// capped at durationMs (when the deck knows it); paused it stays.
int progressMs(const stateparse::Deck& d, uint32_t sincePollMs);
// Filled cells (or pixels) of an n-cell bar at pos of dur (full only at the end); 0 when dur is 0.
int cells(uint32_t pos, uint32_t dur, int n);
// The song can be sung along: the black deck answered, it plays, and its lyrics are synced with a
// line known (the current one, or the next one's start).
bool available(bool answered, bool playing, const stateparse::Deck& d);
// The KARAOKE screen's word in place of the lyrics: ">WRONG KEY" (the deck turned the key down),
// ">LYRICS: NO SIGNAL" (no state came), ">NOTHING PLAYING" (paused, or no track), ">NO SYNCED
// LYRICS", or "" (sing). The same words whyNot() gives.
const char* state(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d);
// Why the Menu's LYRICS item doesn't open, on the Menu's prompt line: ">WRONG KEY", ">LYRICS: NO
// SIGNAL" (named, as music mode's ">MUSIC: NO SIGNAL" shares the line), ">NOTHING PLAYING" (heard, but
// paused or no track) or ">NO SYNCED LYRICS" (heard, plays, nothing synced).
const char* whyNot(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d);
// A state word / message that reports a problem (drawn in the ALERT colour): ">WRONG KEY".
bool isAlert(const char* msg);
// The synced lyric line due at posMs: `line` from lineStartMs, `next` from nextStartMs (when
// known, > 0); "" before the line, and for plain / instrumental / no lyrics.
const char* lyric(const stateparse::Deck& d, uint32_t posMs);
// The line after the one lyric() returns: `line` before it starts, `next` while `line` shows (when
// known), "" once `next` shows (the line after it comes with the next poll) and when not synced.
const char* nextLine(const stateparse::Deck& d, uint32_t posMs);
}
