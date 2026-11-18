#include "gaugehud.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gaugehud {
int lit(float rpm) {
    if (!(rpm > 0.0f)) return 0;
    const int n = (int)(rpm * (float)SEGS / RPM_MAX);
    return n > SEGS ? SEGS : n;
}

Zone zone(int block) {
    const float lo = (float)block * RPM_MAX / (float)SEGS;
    if (lo >= REDLINE) return Zone::Red;
    if (lo >= AMBER_FROM) return Zone::Amber;
    return Zone::Green;
}

int height(int block, int hMin, int hMax) {
    if (block <= 0) return hMin;
    if (block >= SEGS - 1) return hMax;
    return hMin + (hMax - hMin) * block / (SEGS - 1);
}

int Peak::update(int litNow, uint32_t nowMs) {
    int shown = peak_;
    const uint32_t age = nowMs - atMs_;
    if (age > HOLD_MS) {
        const uint32_t falls = (age - HOLD_MS) / FALL_MS;
        shown = falls >= (uint32_t)peak_ ? 0 : peak_ - (int)falls;
    }
    if (litNow >= shown) {   // a new high, or the marker came down to the blocks lit
        peak_ = litNow;
        atMs_ = nowMs;
        return litNow;
    }
    return shown;
}

bool shiftOn(float rpm, uint32_t nowMs) { return obdplan::rpmWarn(rpm, REDLINE) && (nowMs / 125) % 2 == 0; }

void text(float v, int decimals, char* out, size_t n) {
    if (!out || n == 0) return;
    if (std::isfinite(v)) {
        // half away from zero (printf alone rounds 62.5 to "62"), and never "-0"
        const double scale = std::pow(10.0, decimals);
        double r = std::round((double)v * scale) / scale;
        if (r == 0.0) r = 0.0;
        const int w = snprintf(out, n, "%.*f", decimals, r);
        if (w > 0 && (size_t)w < n) return;
    }
    snprintf(out, n, "--");
}

int cells(float pct, int n) {
    if (!(pct > 0.0f)) return 0;
    const int c = (int)std::lround(pct * (float)n / 100.0f);
    return c > n ? n : c;
}

bool coolWarn(float c) { return std::isfinite(c) && obdplan::coolantWarn(c); }

bool battWarn(float volts, float rpm) { return std::isfinite(volts) && obdplan::batteryWarn(volts, rpm > 0.0f); }

const char* linkTag(obdplan::Link l) {
    switch (l) {
        case obdplan::Link::Connecting: return "SEARCH";
        case obdplan::Link::NoEngine: return "KEY ON";
        case obdplan::Link::Live: return "LIVE";
        default: return "OFF";
    }
}
uint8_t segments(char c) {
    static const uint8_t DIGITS[10] = {
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,           // 0
        SEG_B | SEG_C,                                           // 1
        SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,                   // 2
        SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,                   // 3
        SEG_F | SEG_G | SEG_B | SEG_C,                           // 4
        SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,                   // 5
        SEG_A | SEG_F | SEG_G | SEG_E | SEG_D | SEG_C,           // 6
        SEG_A | SEG_B | SEG_C,                                   // 7
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,   // 8
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,           // 9
    };
    if (c >= '0' && c <= '9') return DIGITS[c - '0'];
    return c == '-' ? SEG_G : 0;
}

void speedCells(float kmh, char out[4]) {
    if (!std::isfinite(kmh)) {
        memcpy(out, " --", 4);
        return;
    }
    const long v = kmh < 0.0f ? 0 : std::lround(kmh);
    if (v > 999) {
        memcpy(out, "---", 4);
        return;
    }
    out[0] = v >= 100 ? (char)('0' + v / 100) : ' ';
    out[1] = v >= 10 ? (char)('0' + v / 10 % 10) : ' ';
    out[2] = (char)('0' + v % 10);
    out[3] = 0;
}
}  // namespace gaugehud
