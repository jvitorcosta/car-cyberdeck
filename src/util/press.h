#pragma once
#include <cstdint>
// Touch press tracking for the companion's buttons. PURE, host-tested.
// The button is chosen where the finger goes down. A normal button fires on release if the
// finger is still on it; a hold button fires once, HOLD_MS after the finger is (back) on it.
// Sliding onto another button never fires that one. A lift shorter than RELEASE_MS is a
// touch bounce: the same press goes on.
// Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Touch and navigation").
namespace press {
constexpr uint32_t HOLD_MS = 1000;
constexpr uint32_t RELEASE_MS = 60;   // up this long = released; shorter = a touch bounce
class Press {
public:
    // down: a finger is on the screen; under: the button id under it (0 = none; ignored when
    // !down); hold: whether `under` is a hold button (read at touch-down). Returns the button
    // that fires now, or 0.
    int update(bool down, int under, bool hold, uint32_t nowMs);
    // 0..1 while a hold button is held on, for its fill; 0 otherwise.
    float holdProgress(uint32_t nowMs) const;
    // The button showing pressed (chosen at touch-down, finger still down), or 0.
    int pressed() const { return down_ ? btn_ : 0; }

private:
    bool down_ = false, fired_ = false, hold_ = false;
    int btn_ = 0;        // chosen at touch-down
    int last_ = 0;       // under the finger at the latest sample
    uint32_t t0_ = 0;    // when the finger was last put (back) on btn_
    bool lifting_ = false;
    uint32_t upAt_ = 0;  // when the finger was first seen up (a release once RELEASE_MS old)
};
}
