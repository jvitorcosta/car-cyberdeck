#include "markissue.h"
#include <cstdio>

namespace markissue {

void microDegrees(char* out, size_t n, int32_t v) {
    const uint32_t a = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    snprintf(out, n, "%s%lu.%06lu", v < 0 ? "-" : "", (unsigned long)(a / 1000000), (unsigned long)(a % 1000000));
}

int json(char* out, size_t n, const char* local, const char* utc, int32_t latE6, int32_t lonE6,
         uint16_t course, uint16_t kmh) {
    char lat[16], lon[16];
    microDegrees(lat, sizeof lat, latE6);
    microDegrees(lon, sizeof lon, lonE6);
    return snprintf(out, n,
                    "{\"title\":\"Mark %s\",\"body\":\"lat: %s\\nlon: %s\\nheading: %u\\nkmh: %u\\ntime: %s\\n\\n"
                    "https://www.google.com/maps?q=%s,%s\"}",
                    local[0] ? local : "(clock not set)", lat, lon, (unsigned)course, (unsigned)kmh,
                    utc[0] ? utc : "unknown", lat, lon);
}

}  // namespace markissue
