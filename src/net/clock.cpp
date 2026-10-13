#include "clock.h"
#include <Arduino.h>
#include <time.h>
#include "../config.h"
#include "../util/daynight.h"

// POSIX TZ string, e.g. "<-04>4" (Manaus), "WET0WEST,M3.5.0/1,M10.5.0" (Lisbon),
// "EST5EDT,M3.2.0,M11.1.0" (New York); see include/config.example.h.
#ifndef TIMEZONE
#warning "TIMEZONE is not set in src/config.h (see include/config.example.h): night mode uses UTC"
#define TIMEZONE "UTC0"
#endif

namespace netclock {

void begin() {
    static bool started = false;
    if (started) return;
    started = true;
    configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com");
    Serial.printf("[clock] SNTP started, TZ %s\n", TIMEZONE);
}

bool minuteOfDay(int* m) {
    time_t now = time(nullptr);
    if (!daynight::synced((int64_t)now)) return false;
    struct tm local;
    localtime_r(&now, &local);   // applies TIMEZONE, daylight saving included
    *m = local.tm_hour * 60 + local.tm_min;
    return true;
}

}
