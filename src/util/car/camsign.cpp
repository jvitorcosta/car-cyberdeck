#include "camsign.h"

namespace camsign {

void Countdown::start(camlist::Kind kind, uint16_t kmh, int px) {
    fullM_ = cameras::alertDistM(kind, kmh);
    px_ = w_ = px;
}

int Countdown::update(uint32_t distM) {
    if (!fullM_) return w_;
    const uint32_t d = distM < fullM_ ? distM : fullM_;
    const int w = (int)((uint64_t)px_ * d / fullM_);
    if (w < w_) w_ = w;
    return w_;
}

uint16_t overBy(uint16_t kmh, uint8_t limit) {
    if (limit == camlist::NO_LIMIT) return 0;
    if (kmh <= limit) return 1;
    const uint32_t n = (uint32_t)kmh - limit;
    return (uint16_t)(n > MAX_OVER ? MAX_OVER : n);
}

bool redFace(uint32_t sinceMs, uint32_t nowMs) { return ((nowMs - sinceMs) / BLINK_MS) % 2 == 0; }

}  // namespace camsign
