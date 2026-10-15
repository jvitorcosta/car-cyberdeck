#include "weather.h"
#include <cstdio>
#include <cstring>
#include "jsonflat.h"

namespace weather {

bool parse(const char* body, Now* out) {
    if (!body || !out) return false;
    const char* cur = strstr(body, "\"current\":{");
    if (!cur) return false;
    cur += strlen("\"current\":");
    double t = 0;
    long code = 0, day = 1;
    if (!jsonflat::real(cur, "temperature_2m", &t) || !jsonflat::num(cur, "weather_code", &code))
        return false;
    jsonflat::num(cur, "is_day", &day);
    *out = {t, (int)code, day != 0};
    return true;
}

Icon iconFor(int c, bool day) {
    if (c >= 95) return Icon::Storm;
    if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return Icon::Rain;
    if (c <= 1) return day ? Icon::Sun : Icon::Moon;
    return Icon::Cloud;   // 2-3 cloudy, 45/48 fog, 71-77/85-86 snow
}

void url(const char* lat, const char* lon, char* out, size_t n) {
    snprintf(out, n,
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,weather_code,is_day",
             lat, lon);
}

}
