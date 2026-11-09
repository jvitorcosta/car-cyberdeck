#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "../core/shared.h"
// The karaoke view (core/karaoke switches it on), "night drive": no frame, dark, in the colour of
// the car's INSIDE light (util/black/karaokepal). The song, artist and progress bar at the top, the
// line just sung (one row), the line being sung (Atkinson Hyperlegible, util/black/lyricface: up to
// three rows, a size smaller before anything is cut) and the next line (one row). The sung words
// light up near white as the sweep crosses them and the walking Pokémon, at half size, steps along
// row 1's top edge. Specs: docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md and
// 2026-11-09-karaoke-look-and-menu-design.md.
namespace ui {
// The colours for the next whole draw: the light's swatch (karaoke::light(), -1 for none) and night.
void setKaraokeLook(int light, bool night);
// whole: the screen (a new song, back from another screen); otherwise only the box's lines (the
// next line became current). message (searching, no lyrics: ui/lyricmsg), when not null, takes the
// sung line's place and nothing sweeps. Before the first line, the first line waits there, dim.
void drawKaraoke(TFT_eSPI& t, const AppState& st, const shared::KaraokeLines& k, const char* message,
                 bool whole);
// Where the screens may draw this pass (ui/camband::screens: all of it, or left of the camera sign
// and above its band), as main set the viewport. The sweep clips inside it and restores it.
void setKaraokeClip(int x, int y, int w, int h);
// The progress bar under the artist.
void drawKaraokeBar(TFT_eSPI& t, const AppState& st);
// The sung line's width, all its rows (0: a message, nothing to sweep).
int karaokeLineW();
// Lights the sung line up to px of karaokeLineW(), row by row. Only the new part is drawn.
void drawKaraokeSweep(TFT_eSPI& t, int px);
// Where the Pokémon stands for a sweep at px: all rows' progress mapped onto row 1 (it never drops
// onto the text), from row 1's left edge.
int karaokeWalkerOffset(int px);
// The Pokémon (ui::blitWalker) on row 1's top edge at offset, raised by lift (the hop).
void drawKaraokeWalker(TFT_eSPI& t, uint32_t animMs, int step, int offset, int lift);
}
