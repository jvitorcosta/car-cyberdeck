#include "camlist.h"
#include <cstring>

namespace camlist {
namespace {
const char* const HEADER = "id,kind,lat,lon,heading,limit,label,source,checked";
constexpr int COLS = 9;

struct Field {
    const char* p;
    size_t n;
};

bool eq(const Field& f, const char* s) { return strlen(s) == f.n && memcmp(f.p, s, f.n) == 0; }

// Plain decimal digits only (no sign, no spaces); false when empty or above `maxVal`.
bool parseUInt(const Field& f, int maxVal, int* out) {
    if (f.n == 0) return false;
    int v = 0;
    for (size_t i = 0; i < f.n; i++) {
        if (f.p[i] < '0' || f.p[i] > '9') return false;
        v = v * 10 + (f.p[i] - '0');
        if (v > maxVal) return false;
    }
    *out = v;
    return true;
}

// Degrees ("-2.9656927") -> micro-degrees, in integers: float parsing would drift on the last digit.
// Rounds half away from zero on the 7th decimal (digits past it can only push further away, so the
// 7th alone decides). Out of +-maxDeg is refused.
bool parseDegrees(const Field& f, int maxDeg, int32_t* out) {
    size_t i = 0;
    bool neg = false;
    if (i < f.n && (f.p[i] == '-' || f.p[i] == '+')) neg = f.p[i++] == '-';
    int64_t whole = 0;
    size_t digits = 0;
    for (; i < f.n && f.p[i] >= '0' && f.p[i] <= '9'; i++, digits++) {
        whole = whole * 10 + (f.p[i] - '0');
        if (whole > maxDeg) return false;
    }
    int64_t frac = 0;  // first 6 decimals
    int roundDigit = 0;
    if (i < f.n && f.p[i] == '.') {
        i++;
        size_t k = 0;
        for (; i < f.n && f.p[i] >= '0' && f.p[i] <= '9'; i++, k++, digits++) {
            int d = f.p[i] - '0';
            if (k < 6) frac = frac * 10 + d;
            else if (k == 6) roundDigit = d;
        }
        for (size_t pad = k < 6 ? k : 6; pad < 6; pad++) frac *= 10;
    }
    if (i != f.n || digits == 0) return false;
    int64_t v = whole * 1000000 + frac + (roundDigit >= 5 ? 1 : 0);
    if (v > (int64_t)maxDeg * 1000000) return false;
    *out = (int32_t)(neg ? -v : v);
    return true;
}

// Copies at most n-1 bytes and never ends inside a UTF-8 character: a cut that would leave a lead
// byte or continuation bytes without the rest backs up to the character's start.
void copyCut(char* dst, size_t n, const Field& f) {
    size_t len = f.n < n - 1 ? f.n : n - 1;
    if (len < f.n) {  // cut: byte `len` is the first one dropped; if it continues a character, drop that too
        while (len > 0 && ((unsigned char)f.p[len] & 0xC0) == 0x80) len--;
    }
    memcpy(dst, f.p, len);
    dst[len] = '\0';
}

bool parseRow(const char* s, size_t n, Point* p) {
    Field f[COLS];
    int c = 0;
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || s[i] == ',') {
            if (c == COLS) return false;  // one column too many
            f[c++] = Field{s + start, i - start};
            start = i + 1;
        }
    }
    if (c != COLS) return false;

    if (eq(f[1], "speed")) p->kind = Kind::Speed;
    else if (eq(f[1], "bump")) p->kind = Kind::Bump;
    else if (eq(f[1], "redlight")) p->kind = Kind::RedLight;
    else return false;

    if (!parseDegrees(f[2], 90, &p->latE6)) return false;
    if (!parseDegrees(f[3], 180, &p->lonE6)) return false;

    int v = 0;
    if (!parseUInt(f[4], 359, &v)) return false;  // required: a camera without a direction would alert both lanes
    p->heading = (uint16_t)v;

    if (f[5].n == 0) {
        p->limit = NO_LIMIT;
    } else {
        if (p->kind == Kind::RedLight) return false;
        if (!parseUInt(f[5], 199, &v) || v < 1) return false;
        p->limit = (uint8_t)v;
    }

    copyCut(p->id, sizeof p->id, f[0]);
    copyCut(p->label, sizeof p->label, f[6]);
    return true;
}
}  // namespace

int parse(const char* csv, size_t len, Point* out, int max, int* badLine) {
    int dummy = 0;
    if (!badLine) badLine = &dummy;
    *badLine = 0;
    size_t pos = 0;
    if (len >= 3 && memcmp(csv, "\xEF\xBB\xBF", 3) == 0) pos = 3;  // a BOM from a Windows editor
    int line = 0;
    int count = 0;
    while (pos < len) {
        size_t end = pos;
        while (end < len && csv[end] != '\n') end++;
        size_t n = end - pos;
        if (n > 0 && csv[pos + n - 1] == '\r') n--;
        line++;
        const char* s = csv + pos;
        pos = end < len ? end + 1 : len;

        if (n == 0) {
            // Blank lines are fine at the very end, but not before a row or the header.
            size_t q = pos;
            while (q < len && (csv[q] == '\n' || csv[q] == '\r')) q++;
            if (line > 1 && q >= len) break;
            *badLine = line;
            return -1;
        }
        if (line == 1) {
            if (n != strlen(HEADER) || memcmp(s, HEADER, n) != 0) { *badLine = 1; return -1; }
            continue;
        }
        if (count >= max || !parseRow(s, n, &out[count])) { *badLine = line; return -1; }
        count++;
    }
    if (line == 0) { *badLine = 1; return -1; }  // empty input: no header
    return count;
}

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Speed: return "speed";
        case Kind::Bump: return "bump";
        case Kind::RedLight: return "redlight";
    }
    return "speed";
}
}  // namespace camlist
