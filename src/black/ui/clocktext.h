#pragma once
#include <cstring>
#include <ctime>

namespace ui {
// The wall clock as "HH:MM" for the gauge headers, "--:--" until the clock is set.
inline void clockText(char out[6]) {
    const time_t now = time(nullptr);
    struct tm lt;
    if (now < 1600000000 || !localtime_r(&now, &lt)) {   // not synced yet
        strcpy(out, "--:--");
        return;
    }
    const int h = lt.tm_hour % 24, m = lt.tm_min % 60;   // by hand: -Werror=format-truncation
    out[0] = (char)('0' + h / 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + m / 10);
    out[4] = (char)('0' + m % 10);
    out[5] = 0;
}
}  // namespace ui
