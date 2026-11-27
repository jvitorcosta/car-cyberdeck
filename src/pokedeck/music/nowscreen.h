#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
// The now-playing battle screen at 480x272 (layout 1, "big battle"): the black deck's
// black/ui/screen_now.cpp copied and laid out for the whole panel, drawing into board/canvas.
// The title is double size on up to two lines. The cover is pushed by the caller into ART_*.
// The status scenes and the bobbing note icons aren't ported.
namespace pokeui {
constexpr int ART_X = 12, ART_Y = 32, ART_W = 116, ART_H = 116;

// Everything: background, top strip, art box, info box, status box, dialogue box.
void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre);
void drawTopStrip(TFT_eSPI& t, const AppState& st);
void drawCdFrame(TFT_eSPI& t, int frame);          // the CD after "NOW PLAYING" (frame 0..3)
void drawPokeballFrame(TFT_eSPI& t, int frame);    // the ball before the time
void drawProgressRegion(TFT_eSPI& t, const AppState& st);   // ball, time, HP and EXP bars
// The dialogue box's text: one double-size line when it fits, else two rows. True when it drew
// (the text changed).
bool drawLyricArea(TFT_eSPI& t, const char* text, bool notes);
void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre);   // when the genre arrives
// The walking Pokémon on the HP bar, into the frame. animMs: the play-time clock; step: the
// frame counter for the fallback bob.
void drawWalker(const AppState& st, uint32_t animMs, int step);
}
