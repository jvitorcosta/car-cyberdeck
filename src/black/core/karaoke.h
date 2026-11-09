#pragma once
#include <cstdint>
#include "../../util/black/karaokemode.h"
// The karaoke switch (util/black/karaokemode) for the whole deck: net/remote's GET /karaoke keeps it
// alive or stops it, the UI asks which screen to draw and in which light's colour. Any task.
namespace karaoke {
// light: the yellow deck's INSIDE light (karaokemode::parseLight), kept for light().
void keepAlive(int light);
void stop();
// The light the last keep-alive brought: -1 before any (and after one with none). stop() keeps it.
int light();
karaokemode::Screen screen(lyricstatus::Status s, uint32_t gen);
}
