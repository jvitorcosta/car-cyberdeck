#pragma once
#include <cstdint>
// The Pokédeck's music screen, run by the loop: the black deck's loop (black/main.cpp) copied and
// cut to the battle screen. It copies the network task's state (black/core/shared), redraws what
// changed into board/canvas and presents it. Offline, or with nothing playing, the same screen says
// why (the black deck's status scenes aren't ported), as don't the karaoke view and the cameras.
namespace screen {
bool begin();               // the frame and the first screen; false: no memory
void tick(uint32_t now);    // call every loop
void redraw();              // the whole screen again (after another screen covered it)
}
