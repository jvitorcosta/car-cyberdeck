#include "drivedash.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace drivedash {

static const char* const NAMES[(int)Screen::COUNT] = {"suki", "drive", "trip", "ride", "cockpit"};

const char* name(Screen s) { return (int)s < (int)Screen::COUNT ? NAMES[(int)s] : "?"; }

bool parse(const char* text, Screen* out) {
    if (!text || !out) return false;
    char low[8];
    size_t i = 0;
    for (; text[i] && i < sizeof low - 1; ++i) low[i] = (char)tolower((unsigned char)text[i]);
    if (text[i]) return false;   // too long to be one
    low[i] = '\0';
    for (int s = 0; s < (int)Screen::COUNT; ++s) {
        const char num[2] = {(char)('1' + s), '\0'};
        if (strcmp(low, NAMES[s]) == 0 || strcmp(low, num) == 0) {
            *out = (Screen)s;
            return true;
        }
    }
    return false;
}

bool canSwitch(float kmh) { return !(kmh > MOVING_KMH); }   // NAN: parked

uint8_t segments(char c) {
    // a b c d e f g = bits 0..6
    static const uint8_t DIGIT[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    if (c >= '0' && c <= '9') return DIGIT[c - '0'];
    if (c == '-') return 0x40;
    return 0;
}

Box segment(int k, int x, int y, int w, int h, int t) {
    const float H = h / 2.0f, side = H - t * 1.2f;
    auto box = [](float bx, float by, float bw, float bh) {
        return Box{(int16_t)lroundf(bx), (int16_t)lroundf(by), (int16_t)lroundf(bw), (int16_t)lroundf(bh)};
    };
    switch (k) {
        case 0: return box(x + t, y, w - 2 * t, t);                  // a
        case 1: return box(x + w - t, y + t, t, side);               // b
        case 2: return box(x + w - t, y + H + t * 0.2f, t, side);    // c
        case 3: return box(x + t, y + h - t, w - 2 * t, t);          // d
        case 4: return box(x, y + H + t * 0.2f, t, side);            // e
        case 5: return box(x, y + t, t, side);                       // f
        case 6: return box(x + t, y + H - t / 2.0f, w - 2 * t, t);   // g
        default: return Box{0, 0, 0, 0};
    }
}

int arcLit(float rpm) {
    if (!(rpm > 0.0f)) return 0;   // NAN too
    const int n = (int)ceilf(rpm * ARC_N / ARC_FULL);   // multiply first: 2100 is exactly 6
    return n > ARC_N ? ARC_N : n;
}

bool arcHot(int i) { return i >= HOT_FROM; }

Quad arcQuad(int i) {
    const float pi = 3.14159265f;
    const float a0 = pi * (1.05f + 0.9f * i / ARC_N), a1 = pi * (1.05f + 0.9f * (i + 0.82f) / ARC_N);
    const float ro = ARC_R + ARC_W / 2.0f, ri = ARC_R - ARC_W / 2.0f;
    const float ang[4] = {a0, a1, a1, a0}, rad[4] = {ro, ro, ri, ri};
    Quad q;
    for (int k = 0; k < 4; ++k) {
        q.x[k] = (int16_t)lroundf(ARC_CX + rad[k] * cosf(ang[k]));
        q.y[k] = (int16_t)lroundf(ARC_CY + rad[k] * sinf(ang[k]));
    }
    return q;
}

int ladderLit(float v, float max, int cells) {
    if (!(v > 0.0f) || !(max > 0.0f) || cells <= 0) return 0;
    const float f = v >= max ? 1.0f : v / max;
    return (int)lroundf(f * cells);
}

float warmup(float coolC) {
    if (std::isnan(coolC)) return 0.0f;
    const float f = (coolC - 40.0f) / 45.0f;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

void Alerts::update(float coolC, float volts, float rpm, uint32_t nowMs) {
    if (std::isfinite(coolC)) {
        if (!cool_ && coolC > COOL_ON) cool_ = true;
        else if (cool_ && coolC < COOL_OFF) cool_ = false;
    }
    if (!std::isfinite(volts)) return;
    const bool running = rpm > 400.0f;   // NAN: not known to run
    if (running && volts < VOLT_ON) {
        if (!low_) {
            low_ = true;
            lowSince_ = nowMs;
        }
        if (nowMs - lowSince_ >= VOLT_MS) volt_ = true;
        return;
    }
    low_ = false;
    if (volt_ && volts >= VOLT_OFF) volt_ = false;
}

void rpmText(float rpm, char* out, size_t n) {
    if (!out || n == 0) return;
    if (!std::isfinite(rpm) || rpm < 0.0f) {
        snprintf(out, n, "--");
        return;
    }
    const int tenths = (int)lroundf(rpm / 100.0f);   // 2450 -> 25 -> "2.5"
    snprintf(out, n, "%d.%d", tenths / 10, tenths % 10);
}
}  // namespace drivedash
