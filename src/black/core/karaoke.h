#pragma once
#include <cstdint>
#include "../../util/black/karaokemode.h"
// The karaoke switch (util/black/karaokemode) for the whole deck: net/remote's GET /karaoke keeps it
// alive or stops it, the UI asks which screen to draw. Any task.
namespace karaoke {
void keepAlive();
void stop();
karaokemode::Screen screen(lyricstatus::Status s, uint32_t gen);
}
