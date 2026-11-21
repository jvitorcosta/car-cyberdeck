#pragma once
// The PS3 controller's presses, each kept until the loop has acted on it. handlePad takes ONE
// shortcut per pass (a lights shortcut, then the screen's own buttons), and it used to mark every
// button's press as seen before it chose: START and X in the same pass, or L1 with O, acted on the
// first and dropped the second for good, a press the owner felt as missed. Here a press stays
// waiting until it is taken, or until a screen change throws the rest away (a button pressed
// before the screen changed must not act on the next one). PURE, host-tested.
#include <cstdint>

namespace padedges {
class Latch {
public:
    // The buttons held now (padguard's bits). Returns the ones that went down since the last call.
    uint32_t feed(uint32_t held) {
        const uint32_t fresh = held & ~prev_;
        prev_ = held;
        waiting_ |= fresh;
        return fresh;
    }
    // Presses not acted on yet, this pass's and any left from an earlier one.
    uint32_t waiting() const { return waiting_; }
    // The loop acted on these.
    void take(uint32_t bits) { waiting_ &= ~bits; }
    // A screen change: whatever still waits was meant for the screen that is gone.
    void drop() { waiting_ = 0; }
    // A new connection, or the pad gone: nothing held, nothing waiting.
    void reset() { *this = Latch(); }

private:
    uint32_t prev_ = 0, waiting_ = 0;
};
}  // namespace padedges
