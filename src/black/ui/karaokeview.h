#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "../core/shared.h"
// The karaoke view (core/karaoke switches it on): the song, artist and progress bar at the top, a
// large dialogue box with the line just sung (small), the line being sung (font 2 at 2x, up to two
// rows) and the next line (small). A dark tone sweeps across the sung line and the walking Pokémon
// steps along its top edge. Spec: docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md.
namespace ui {
// whole: the screen (a new song, back from another screen); otherwise only the box's lines (the
// next line became current). message (searching, no lyrics: ui/lyricmsg), when not null, takes the
// sung line's place and nothing sweeps. Before the first line, the first line waits there, dim.
void drawKaraoke(TFT_eSPI& t, const AppState& st, const shared::KaraokeLines& k, const char* message,
                 bool whole);
// The progress bar under the artist.
void drawKaraokeBar(TFT_eSPI& t, const AppState& st);
// The sung line's width, both rows (0: a message, nothing to sweep).
int karaokeLineW();
// Darkens the sung line up to px of karaokeLineW(): row 1, then row 2. Only the new part is drawn.
void drawKaraokeSweep(TFT_eSPI& t, int px);
// Where the Pokémon stands for a sweep at px: both rows' progress mapped onto row 1 (it never drops
// onto the text), from row 1's left edge.
int karaokeWalkerOffset(int px);
// The Pokémon (ui::blitWalker) on row 1's top edge at offset, raised by lift (the hop).
void drawKaraokeWalker(TFT_eSPI& t, uint32_t animMs, int step, int offset, int lift);
}
