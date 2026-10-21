#pragma once
#include "app_state.h"
// The status screens: "offline" (whose fault it is: WiFi, Spotify, a 429...) and "Waiting for a
// song..." with the phone link's QR card, the music box and its speaker button. UI loop only.
namespace statusview {
enum Mode { OFFLINE = 1, WAITING = 2 };
// Draws one loop's worth. fresh: this screen wasn't up last loop (or the palette changed).
void draw(const AppState& st, Mode mode, bool fresh);
// The music box loops while "Waiting for a song..." is up, and stops (a short fade) as it goes.
// Called every loop, on any screen.
void musicBox(bool want);
}
