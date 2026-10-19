#include "idlewait.h"

namespace idlewait {

bool Clock::update(bool paused, bool stopped, uint32_t now) {
    if (stopped) { timing_ = false; return true; }
    if (!paused) { timing_ = false; return false; }
    if (!timing_) { timing_ = true; since_ = now; }
    return now - since_ >= PAUSE_MS;   // unsigned: survives the millis() wrap
}

}
