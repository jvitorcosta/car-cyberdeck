#pragma once
#include <cstdint>
#include "lyricbuf.h"
// The black deck's karaoke view (ui/karaokeview): which lines it shows, how far the dark tone has
// swept across the line being sung, and the walking Pokémon's hop back to the start of the next one.
// LRCLIB times lines, not words, so the sweep runs evenly from a line's start to the next line's.
// Spec: docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md. PURE, host-tested.
namespace karaokepick {
constexpr uint32_t LAST_LINE_MS = 5000;  // the song's last line sweeps this long (or to the song's end)
constexpr uint32_t HOP_MS = 300;         // the hop back to the left edge
constexpr int HOP_PX = 14;               // and its height at the top of the arc
constexpr uint32_t REWIND_MS = 500;      // a step back this long in the line is a seek, not jitter

// Indices into the lyrics' lines (-1: none). cur is the line being sung: the last non-blank line
// begun by posMs (-1 in the intro, next then the first line). A timed blank line (an instrumental
// break) never shows: the line before it stays current, fully swept. prev and next skip blanks too.
// startMs..endMs: cur's sweep, to the next line in the file (blank or not).
struct Pick {
    int prev, cur, next;
    uint32_t startMs, endMs;
};
Pick pick(const lyricbuf::Lyrics& l, uint32_t posMs, uint32_t durationMs);

// How much of a widthPx-wide line is swept at posMs: 0 before startMs, all of it from endMs (and for
// a line with no length).
int sweepPx(uint32_t posMs, uint32_t startMs, uint32_t endMs, int widthPx);

// The sweep to draw: px, but held at shownPx while px is less than REWIND_MS of the line behind it
// (each Spotify poll can set the interpolated position a little back). A line with no length follows.
int steadyPx(int shownPx, int px, int widthPx, uint32_t startMs, uint32_t endMs);

// The hop sinceMs after it began: x from fromX to toX, lift up to HOP_PX at the arc's top.
struct Hop {
    int x, lift;
    bool done;
};
Hop hopAt(uint32_t sinceMs, int fromX, int toX);
}  // namespace karaokepick
