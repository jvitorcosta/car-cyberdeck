#pragma once
// The yellow deck's guard on its asks to the black deck (/link, /music, /karaoke, a playlist): each
// one is a TCP connection, a few KB of heap for a second. With the PS3 controller and both lights
// on Bluedroid's dual mode the heap ran down to ~15 KB, its largest block to ~100 B, and the next
// small allocation in the Bluetooth stack failed and panicked the deck (the crash log's "panic ...
// at link ask", 2026-11-15). Below MIN_FREE free or MIN_BLOCK in one piece the asks wait; they go
// again once the heap is back RESUME_MARGIN above MIN_FREE (no flapping on the edge). PURE,
// host-tested.
#include <cstdint>

namespace heapguard {
constexpr uint32_t MIN_FREE = 18000;     // the crashes came at 15.3-16.4 KB lowest free
constexpr uint32_t MIN_BLOCK = 6000;     // ... and with the largest block at 4 KB or less
constexpr uint32_t RESUME_MARGIN = 4000;

struct Gate {
    bool low = false;  // asks held back

    // True when an ask may go now. Call with ESP.getFreeHeap() and the largest free block.
    bool room(uint32_t freeBytes, uint32_t largest) {
        if (low) {
            if (freeBytes >= MIN_FREE + RESUME_MARGIN && largest >= MIN_BLOCK) low = false;
        } else if (freeBytes < MIN_FREE || largest < MIN_BLOCK) {
            low = true;
        }
        return !low;
    }
};
}  // namespace heapguard
