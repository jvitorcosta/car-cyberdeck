#pragma once
#include <cstdint>
// A made-up song on the battle screen, until the Pokédeck talks to Spotify: it plays by the clock
// (the bar drains, the CD and the ball spin) so the layout can be judged on the panel.
namespace sample {
bool begin();                // draws the whole screen; false: no memory for the frame
void tick(uint32_t now);     // advances and presents it; call every loop
void redraw();               // the whole screen again (after another screen covered it)
}
