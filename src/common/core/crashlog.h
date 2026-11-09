#pragma once
#include <cstdint>
// Why the deck restarted, kept across the restart (util/crashrecord). A snapshot of the heap each
// second and the last phase marked live in RTC memory, which a panic or a watchdog reset keeps; the
// next boot reads them, adds the reset reason and stores the record in NVS ("crashlog", the last
// crashrecord::HISTORY runs). Allocations that fail are counted (and logged) as they happen.
namespace crashlog {
// First thing in setup(), after Serial: "[boot] reset reason", the run before this one, and the
// stored history.
void begin();
// From the loop: the heap snapshot, at most once a second.
void tick();
// What the deck is doing now (a short literal, e.g. "lyrics get"): kept if it restarts in it.
// Any task; the last call wins.
void at(const char* phase);
// The allocations that failed since boot.
uint32_t fails();
}
