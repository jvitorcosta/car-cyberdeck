#include "geoip.h"
#include <cstring>
#include "jsonflat.h"
#include "jsonpath.h"

namespace geoip {

// Copies the number at `v` as text, at most 4 digits after the point; false if it isn't one.
static bool numberText(const char* v, char* out, size_t n) {
    if (!v) return false;
    size_t len = 0;
    const char* p = v;
    if (*p == '-') out[len++] = *p++;
    if (*p < '0' || *p > '9') return false;
    int decimals = -1;   // -1: before the point
    for (; *p && len + 1 < n; ++p) {
        if (*p >= '0' && *p <= '9') {
            if (decimals >= 4) continue;   // cut, keep scanning past the rest
            if (decimals >= 0) decimals++;
            out[len++] = *p;
        } else if (*p == '.' && decimals < 0) {
            decimals = 0;
            out[len++] = *p;
        } else {
            break;
        }
    }
    if (len > 0 && out[len - 1] == '.') len--;
    out[len] = '\0';
    return true;
}

bool parse(const char* body, Place* out) {
    if (!body || !out) return false;
    char status[12];
    if (!jsonflat::str(body, "status", status, sizeof status) || strcmp(status, "success") != 0) return false;
    Place p{};
    if (!numberText(jsonpath::find(body, "lat"), p.lat, sizeof p.lat) ||
        !numberText(jsonpath::find(body, "lon"), p.lon, sizeof p.lon))
        return false;
    jsonflat::str(body, "city", p.city, sizeof p.city);   // "" when absent
    *out = p;
    return true;
}

}
