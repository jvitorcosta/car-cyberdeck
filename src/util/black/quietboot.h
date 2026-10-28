#pragma once
#include <cstdint>
// The S3's self-heal restart (core/nettask: heap starved, or WiFi down for long) leaves a note in RTC
// memory, which survives a software restart but not a power-on. The boot after it is quiet: no
// opening video or greeting, straight to the screens, so a deck without its hotspot doesn't replay
// the opening mid-drive. The note also carries how many WiFi-down restarts came in a row
// (netplan::Health waits longer before each). Any other boot (power-on, crash, an update's restart)
// finds no note and plays the opening. setup() reads the note once and clears it. PURE, host-tested.
namespace quietboot {
constexpr uint32_t MAGIC = 0x51E7B007u;   // the note, not power-on noise
constexpr uint32_t MAX_COUNT = 1000;      // a WiFi-down count above this is noise
// This boot follows the self-heal restart: the note is there and the reset was a software one.
inline bool quiet(uint32_t magic, bool softwareReset) { return softwareReset && magic == MAGIC; }
// WiFi-down restarts in a row before this boot (0 after any other boot).
inline uint32_t wifiRestarts(uint32_t magic, uint32_t count, bool softwareReset) {
    return quiet(magic, softwareReset) && count <= MAX_COUNT ? count : 0;
}
}
