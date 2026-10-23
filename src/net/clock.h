#pragma once
// Wall clock from SNTP (UTC). begin() once WiFi is up; SNTP then re-syncs in the background
// (UDP, no TLS). Named netclock: `clock` would collide with the C library's clock().
namespace netclock {
void begin();
// Local minute of the day (0..1439, TIMEZONE in config.h); false until the first SNTP answer.
bool minuteOfDay(int* m);
// Local date (TIMEZONE): year, month 1..12, day 1..31; false until the first SNTP answer.
bool localDate(int* y, int* month, int* day);
}
