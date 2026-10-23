#include "weather.h"
#include <cstdio>
#include <cstdlib>
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

// The first element of `key`'s number array inside the daily block; false unless it is a number.
static bool firstOf(const char* daily, const char* key, double* out) {
    char pat[40];
    snprintf(pat, sizeof pat, "\"%s\":[", key);
    const char* p = strstr(daily, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ') ++p;
    if (*p != '-' && (*p < '0' || *p > '9')) return false;   // ] or null
    char* end = nullptr;
    const double v = strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

bool parseDaily(const char* body, double* maxC, double* minC) {
    if (!body || !maxC || !minC) return false;
    const char* daily = strstr(body, "\"daily\":{");
    if (!daily) return false;
    double hi = 0, lo = 0;
    if (!firstOf(daily, "temperature_2m_max", &hi) || !firstOf(daily, "temperature_2m_min", &lo)) return false;
    *maxC = hi;
    *minC = lo;
    return true;
}

const char* words(int c) {
    if (c == 0) return "CLEAR";
    if (c == 1 || c == 2) return "PARTLY CLOUDY";
    if (c == 3) return "OVERCAST";
    if (c == 45 || c == 48) return "FOG";
    if (c >= 51 && c <= 57) return "DRIZZLE";
    if (c >= 61 && c <= 67) return "RAIN";
    if (c >= 71 && c <= 77) return "SNOW";
    if (c >= 80 && c <= 82) return "SHOWERS";
    if (c == 85 || c == 86) return "SNOW SHOWERS";
    if (c >= 95 && c <= 99) return "THUNDERSTORM";
    return "UNKNOWN";
}

Icon iconFor(int c, bool day) {
    if (c >= 95) return Icon::Storm;
    if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return Icon::Rain;
    if (c <= 1) return day ? Icon::Sun : Icon::Moon;
    return Icon::Cloud;   // 2-3 cloudy, 45/48 fog, 71-77/85-86 snow
}

void url(const char* lat, const char* lon, char* out, size_t n) {
    snprintf(out, n,
             "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,weather_code,is_day"
             "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
             lat, lon);
}

}
