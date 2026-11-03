#include "nmea.h"
namespace nmea {
namespace {
int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Unsigned decimal ("57.94156", "32", ".5") -> value * 1e6, extra decimals dropped. False on an empty
// field or any other character, so garbage never turns into a position.
bool parseE6(const char* s, int64_t* out) {
    int64_t whole = 0, frac = 0, scale = 100000;
    bool digits = false, dot = false;
    for (; *s; s++) {
        if (*s == '.' && !dot) { dot = true; continue; }
        if (*s < '0' || *s > '9') return false;
        digits = true;
        if (!dot) {
            if (whole > 100000000) return false;  // absurd; also keeps the multiply below in range
            whole = whole * 10 + (*s - '0');
        } else if (scale > 0) {
            frac += (*s - '0') * scale;
            scale /= 10;
        }
    }
    *out = whole * 1000000 + frac;
    return digits;
}

// NMEA "ddmm.mmmm" (+ hemisphere letter) -> signed degrees * 1e6, rounded to nearest.
bool parseCoord(const char* v, const char* hemi, int maxDeg, char pos, char neg, int32_t* out) {
    int64_t e6;
    if (!parseE6(v, &e6)) return false;
    int64_t deg = e6 / 100000000;  // "ddmm" -> dd
    int64_t minE6 = e6 - deg * 100000000;
    if (deg > maxDeg || minE6 >= 60000000) return false;
    int64_t d = deg * 1000000 + (minE6 + 30) / 60;
    if (hemi[0] == neg && !hemi[1]) d = -d;
    else if (hemi[0] != pos || hemi[1]) return false;
    *out = (int32_t)d;
    return true;
}
}  // namespace

bool Reader::feed(char c, uint32_t ms, cameras::Fix* out) {
    if (c == '$') {  // a new sentence always restarts, so a torn line resyncs at the next one
        inLine_ = true;
        len_ = 0;
        return false;
    }
    if (!inLine_) return false;
    if (c == '\n') {
        inLine_ = false;
        return parse(ms, out);
    }
    if (c == '\r') return false;
    if (len_ >= LINE_MAX - 1) {  // overlong: drop it and wait for the next '$'
        inLine_ = false;
        return false;
    }
    line_[len_++] = c;
    return false;
}

bool Reader::parse(uint32_t ms, cameras::Fix* out) {
    line_[len_] = 0;
    // Checksum: XOR of everything between '$' and '*' must equal the two hex digits after it. GPS 2 IP
    // always sends one, so a sentence without it is treated as damaged.
    char* star = nullptr;
    for (size_t i = 0; i < len_; i++)
        if (line_[i] == '*') star = line_ + i;
    if (!star || line_ + len_ - star != 3) return false;
    int h = hexVal(star[1]), l = hexVal(star[2]);
    if (h < 0 || l < 0) return false;
    uint8_t x = 0;
    for (const char* p = line_; p < star; p++) x ^= (uint8_t)*p;
    if (x != (uint8_t)(h * 16 + l)) return false;
    *star = 0;

    // Split in place on commas: f[0] = talker + "RMC", 1 time, 2 status, 3/4 lat, 5/6 lon, 7 knots, 8 course.
    const char* f[10];  // one past the course, so the course field is cut at its comma
    size_t n = 0;
    f[n++] = line_;
    for (char* p = line_; *p && n < 10; p++)
        if (*p == ',') {
            *p = 0;
            f[n++] = p + 1;
        }
    if (n < 9) return false;
    const char* id = f[0];
    for (int i = 0; i < 2; i++)
        if (!((id[i] >= 'A' && id[i] <= 'Z') || (id[i] >= 'a' && id[i] <= 'z'))) return false;
    if (id[2] != 'R' || id[3] != 'M' || id[4] != 'C' || id[5]) return false;

    cameras::Fix r{};
    r.ms = ms;
    r.course = course_;
    if (f[2][0] == 'V' && !f[2][1]) {  // the phone talks but has no fix
        *out = r;
        return true;
    }
    if (f[2][0] != 'A' || f[2][1]) return false;
    if (!parseCoord(f[3], f[4], 90, 'N', 'S', &r.latE6)) return false;
    if (!parseCoord(f[5], f[6], 180, 'E', 'W', &r.lonE6)) return false;
    int64_t v;
    r.kmh = 0;
    if (f[7][0]) {
        if (!parseE6(f[7], &v)) return false;
        int64_t kmh = v * 1852 / 1000000000;  // knots * 1.852, rounded down
        r.kmh = kmh > 65535 ? 65535 : (uint16_t)kmh;
    }
    if (f[8][0]) {
        if (!parseE6(f[8], &v)) return false;
        course_ = (uint16_t)(((v + 500000) / 1000000) % 360);
        r.course = course_;
    }
    r.valid = true;
    *out = r;
    return true;
}
}  // namespace nmea
