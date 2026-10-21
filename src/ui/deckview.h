#pragma once
#include "app_state.h"
// The now-playing screen: title and artist, album art, the walker, the genre badge, the top
// strip, the progress bars and the dialogue box (lyrics or a Pokemon-style status). UI loop only.
namespace deckview {
// Draws one loop's worth. fresh: the deck wasn't on screen last loop (or the palette changed):
// everything again. A new track redraws in full too. view is st with its progress interpolated.
void draw(const AppState& st, const AppState& view, bool fresh);
}
