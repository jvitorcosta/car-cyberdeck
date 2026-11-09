#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "../core/shared.h"
// The karaoke view (core/karaoke switches it on) in the yellow deck's terminal look: black, in the
// theme the yellow deck shows (the car's INSIDE light's colour, util/black/karaokepal) and its VT323
// (util/black/lyricface). At the top one row with the song left and the artist right, and the
// progress bar with the walking Pokémon on it at the song's progress; then the line just sung (one
// row), the line being sung (three rows at 36 px, else at 24 px, before anything is cut) and the next
// line (one row). The sung words light up from DIM to TEXT as the sweep crosses them. Specs: docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md and
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
// The progress bar under the song's row: the played part solid, the rest a thin line (the Pokémon
// walks at their meeting).
void drawKaraokeBar(TFT_eSPI& t, const AppState& st);
// The sung line's width, all its rows (0: a message, nothing to sweep).
int karaokeLineW();
// Lights the sung line up to px of karaokeLineW(), row by row. Only the new part is drawn.
void drawKaraokeSweep(TFT_eSPI& t, int px);
// The Pokémon (ui::blitWalker) on the progress bar's top at the song's progress, as on the normal screen.
void drawKaraokeWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step);
}
