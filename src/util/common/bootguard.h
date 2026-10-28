#pragma once
#include <cstdint>
// Both decks' boot-loop guard for firmware that arrived over WiFi (net/ota). Two layers:
//  - IDF's rollback (the bootloader is built with it): a new image boots once on probation, and
//    any reset before the app confirms it, from a crash in a global constructor to the ignition,
//    boots the previous one. net/ota confirms the image once it is proven (below).
//  - this counter, for firmware that got past that (a USB flash, or a confirmed image): a counter
//    in RTC memory (it survives a crash reset, not a power-on) goes up after each panic or watchdog
//    reset. LIMIT crashes in a row on firmware not yet good: boot the other slot.
// Proven: the deck has run HEALTHY_MS with its sign of working (the S3: a good Spotify poll; the
// yellow deck's LED panel: online) and can take the next update (its listener is up, when updates
// are on). Then the running firmware counts as good (remembered in nvs) and the counter clears.
// A power-on, a self-heal restart or an update's restart clears the count, so a deck with no
// signal for hours never rolls back. PURE, host-tested.
namespace bootguard {
constexpr uint32_t MAGIC = 0xCDB0071Eu;   // RTC memory holds a count, not power-on noise
constexpr uint32_t LIMIT = 3;
constexpr uint32_t HEALTHY_MS = 60000;
// The crash count after this boot, from what RTC memory held.
uint32_t afterBoot(uint32_t magic, uint32_t crashes, bool crashed);
// Boot the other slot: LIMIT crashes in a row, firmware not yet good, and the other slot holds
// a valid app.
bool rollBack(uint32_t crashes, bool good, bool otherValid);
// The running firmware works: up HEALTHY_MS and a good poll since boot.
bool healthy(uint32_t upMs, bool goodPoll);
// The running firmware has proven itself: healthy, and its update listener is up (when updates are
// on), so the next update can always reach a firmware called good.
bool proven(uint32_t upMs, bool goodPoll, bool updatesOn, bool listening);
}
