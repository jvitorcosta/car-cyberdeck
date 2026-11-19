#pragma once
#include <cstdint>
// Night mode schedule: 18:00-06:00 local time. Pure: net/clock turns the SNTP time into a
// local minute of the day; the result drives the black deck's palette and backlight, and the yellow
// deck's backlight only.
namespace daynight {
constexpr int NIGHT_START = 18 * 60;          // 18:00
constexpr int NIGHT_END   = 6 * 60;           // 06:00
constexpr int64_t SYNCED_AFTER = 1704067200;  // 2024-01-01 00:00 UTC
// True from NIGHT_START to midnight and from midnight to NIGHT_END.
bool isNight(int minuteOfDay);
// Quiet window (-D QUIET_START_MIN / QUIET_END_MIN override it): backlight 0 and the yellow deck's
// car LEDs off, 01:00-07:00 local. A touch on the yellow deck wakes it until the window ends.
#ifndef QUIET_START_MIN
#define QUIET_START_MIN (1 * 60)
#endif
#ifndef QUIET_END_MIN
#define QUIET_END_MIN (7 * 60)
#endif
constexpr int QUIET_START = QUIET_START_MIN;
constexpr int QUIET_END   = QUIET_END_MIN;
bool isQuiet(int minuteOfDay);
// False while the clock still counts from 1970 (no SNTP answer yet).
bool synced(int64_t utcSeconds);
}
