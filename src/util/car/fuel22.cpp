#include "fuel22.h"
#include <cctype>
#include <cmath>
#include <cstring>

namespace fuel22 {
namespace {
bool isHex(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }
int hexVal(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }

// The reply's hex bytes into out (at most n): per line, an "N:" multi-frame prefix is dropped and a line
// that is only a 3-digit length is skipped; anything that isn't a two-digit hex token is ignored.
int bytesOf(const char* raw, uint8_t* out, int n) {
    int count = 0;
    const char* p = raw;
    while (*p && count < n) {
        const char* eol = std::strchr(p, '\n');
        const char* end = eol ? eol : p + std::strlen(p);
        const char* s = p;
        while (s < end && (*s == ' ' || *s == '\r')) ++s;
        // "0: " / "1: " ...: one or two hex digits and a colon
        const char* t = s;
        while (t < end && isHex(*t)) ++t;
        if (t < end && *t == ':' && t - s >= 1 && t - s <= 2) s = t + 1;
        else {
            int digits = 0;
            const char* u = s;
            while (u < end && isHex(*u)) { ++digits; ++u; }
            while (u < end && (*u == ' ' || *u == '\r')) ++u;
            if (digits == 3 && u == end) { p = eol ? eol + 1 : end; continue; }   // the length line
        }
        while (s < end && count < n) {
            while (s < end && (*s == ' ' || *s == '\r')) ++s;
            const char* tok = s;
            while (s < end && *s != ' ' && *s != '\r') ++s;
            if (s - tok == 2 && isHex(tok[0]) && isHex(tok[1]))
                out[count++] = static_cast<uint8_t>(hexVal(tok[0]) * 16 + hexVal(tok[1]));
        }
        p = eol ? eol + 1 : end;
    }
    return count;
}
}  // namespace

bool protocol29(const char* atdpnRaw) {
    if (!atdpnRaw) return false;
    // the last token of the reply, past any echo or newline: "A7" or "7"
    const char* last = nullptr;
    for (const char* p = atdpnRaw; *p;) {
        while (*p == ' ' || *p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        last = p;
        while (*p && *p != ' ' && *p != '\r' && *p != '\n') ++p;
    }
    if (!last) return false;
    const char* e = last;
    while (*e && *e != ' ' && *e != '\r' && *e != '\n') ++e;
    const size_t n = static_cast<size_t>(e - last);
    if (n == 1) return last[0] == '7';
    return n == 2 && last[0] == 'A' && last[1] == '7';
}

bool parse(const char* raw, float* litres) {
    if (!raw || !litres) return false;
    uint8_t b[96];
    const int n = bytesOf(raw, b, static_cast<int>(sizeof b));
    for (int i = 0; i + 2 < n; ++i) {
        if (b[i] != 0x62 || b[i + 1] != 0x70 || b[i + 2] != 0x29) continue;
        const int body = i + 3;                 // the first data byte
        if (n < body + 14) return false;        // cut off before bytes 12-13
        const float v = static_cast<float>(b[body + 12] * 256 + b[body + 13]) / 100.0f;
        if (!(v >= 0.0f && v <= MAX_L)) return false;
        *litres = v;
        return true;
    }
    return false;
}

float freshLitres(float litres, uint32_t atMs, uint32_t nowMs) {
    return std::isfinite(litres) && nowMs - atMs < FRESH_MS ? litres : NAN;
}

void Plan::start(uint32_t nowMs, bool bus29) {
    started_ = true;
    off_ = !bus29;
    fails_ = 0;
    nextAt_ = nowMs + FIRST_MS;
}

bool Plan::due(uint32_t nowMs) const {
    return started_ && !off_ && static_cast<int32_t>(nowMs - nextAt_) >= 0;
}

void Plan::done(uint32_t nowMs, bool ok) {
    if (ok) fails_ = 0;
    else if (++fails_ >= MAX_FAILS) off_ = true;
    nextAt_ = nowMs + EVERY_MS;
}
}  // namespace fuel22
