#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
// What a run left behind when it ended (common/core/crashlog): why it ended, how long it ran, the
// heap at its last second, the allocations that failed and what it was doing. The last HISTORY of
// them stay in NVS, newest first, so a restart in the car can still be read at home over USB.
namespace crashrecord {
constexpr int HISTORY = 6;
constexpr size_t AT_LEN = 16;  // the phase's name, cut to fit

// Plain data, no initialisers: crashlog keeps one in RTC memory that startup must not zero
// (Record{} is all zeros).
struct Record {
    uint8_t reason;        // esp_reset_reason_t of the boot that ended this run
    uint32_t upS;          // seconds the run lasted (to its last second's snapshot)
    uint32_t heap, minHeap, largest;  // free, lowest free, largest free block
    uint32_t fails;        // allocations that failed during the run
    uint32_t failSize;     // the last one's size
    char at[AT_LEN];       // the last phase the run marked
};

// Puts r in front, the oldest falls off the end.
inline void push(Record (&ring)[HISTORY], const Record& r) {
    for (int i = HISTORY - 1; i > 0; i--) ring[i] = ring[i - 1];
    ring[0] = r;
}

// The reset reason's name (esp_reset_reason_t's values, ESP-IDF 4.4).
inline const char* reasonName(uint8_t r) {
    switch (r) {
        case 1: return "power-on";
        case 2: return "external pin";
        case 3: return "software";
        case 4: return "panic";
        case 5: return "interrupt watchdog";
        case 6: return "task watchdog";
        case 7: return "other watchdog";
        case 8: return "deep sleep";
        case 9: return "brownout";
        case 10: return "SDIO";
        default: return "unknown";
    }
}

// A reset the firmware caused by crashing: a panic or a watchdog.
inline bool crashed(uint8_t r) { return r == 4 || r == 5 || r == 6 || r == 7; }

// One log line for r, always terminated.
inline void format(const Record& r, char* out, size_t cap) {
    if (!cap) return;
    snprintf(out, cap, "%s after %lus: heap %lu (min %lu, block %lu), %lu failed allocs (last %lu B), at %s",
             reasonName(r.reason), static_cast<unsigned long>(r.upS), static_cast<unsigned long>(r.heap),
             static_cast<unsigned long>(r.minHeap), static_cast<unsigned long>(r.largest),
             static_cast<unsigned long>(r.fails), static_cast<unsigned long>(r.failSize), r.at[0] ? r.at : "-");
}

// Copies name into at (cut to AT_LEN - 1, always terminated).
inline void mark(char (&at)[AT_LEN], const char* name) {
    strncpy(at, name ? name : "", AT_LEN - 1);
    at[AT_LEN - 1] = 0;
}
}  // namespace crashrecord
