#pragma once
#include <cstdint>
#include "cameras.h"
#include "camlist.h"
// The sign panel's numbers (black/ui/camband): the band's countdown bar, how far over the limit an
// alarm is (`+N`) and the alarm's 2 Hz blink. Only the maths: camband draws.
// PURE, host-tested.
namespace camsign {
constexpr uint32_t BLINK_MS = 250;   // red face 250 ms, normal 250 ms: 2 Hz (as the yellow deck's)
constexpr uint16_t MAX_OVER = 999;   // `+N` never needs more than three digits

// The countdown bar along the band's top: full at the point's alert distance as it was when the
// alert began (cameras::alertDistM at that fix's speed), empty at the point. It never grows back
// while the same alert lasts (GPS jitter, braking): start() again only for a new alert.
class Countdown {
public:
    void start(camlist::Kind kind, uint16_t kmh, int px);  // a new alert: full width (px)
    int update(uint32_t distM);  // the width now: px x dist / full, clamped, never more than before
    int width() const { return w_; }
private:
    uint32_t fullM_ = 0;
    int px_ = 0, w_ = 0;
};

// The `+N` under an alarm's sign: kmh - limit, at least 1 (the speed may have dropped since the
// stage was worked out), at most MAX_OVER; 0 with no limit (never an alarm).
uint16_t overBy(uint16_t kmh, uint8_t limit);
// The alarm's sign shows its red face now (2 Hz, the red face first), `sinceMs` the alarm's start.
bool redFace(uint32_t sinceMs, uint32_t nowMs);
}  // namespace camsign
