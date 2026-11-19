#include "sukiride.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../common/termpal.h"
#include "drivedash.h"

namespace sukiride {

static uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool parse(const uint8_t* blob, size_t len, Art* out) {
    if (!blob || !out || len < HEAD || memcmp(blob, "SKR1", 4) != 0) return false;
    if (u16(blob + 4) != ART_W || u16(blob + 6) != ART_H || u16(blob + 8) != CEL_X || u16(blob + 10) != CEL_W ||
        u16(blob + 12) != CELS || u16(blob + 14) != FONTS)
        return false;
    Art a;
    a.blob = blob;
    a.len = len;
    size_t at = HEAD;
    a.pal = blob + at;
    at += COLOURS * 3;
    a.bg = blob + at;
    at += (size_t)ART_W * ART_H;
    a.cels = blob + at;
    at += (size_t)CELS * CEL_W * ART_H;
    if (at > len) return false;
    for (int f = 0; f < FONTS; ++f) {
        if (at + FONT_HEAD > len) return false;
        Font& ft = a.font[f];
        ft.h = blob[at];
        ft.track = (int8_t)blob[at + 1];
        ft.n = blob[at + 2];
        ft.cap = blob[at + 3];
        at += FONT_HEAD;
        if (ft.h == 0 || ft.cap == 0 || ft.cap > ft.h || at + (size_t)ft.n * GLYPH > len) return false;
        ft.entries = blob + at;
        for (int g = 0; g < ft.n; ++g) {
            const uint8_t* e = ft.entries + g * GLYPH;
            const size_t off = u32(e + 4), size = (size_t)e[1] * ft.h;
            if (off > len || size > len - off) return false;
        }
        at += (size_t)ft.n * GLYPH;
    }
    *out = a;
    return true;
}

static const uint8_t* glyph(const Art& a, FontId f, char c, int* w) {
    const Font& ft = a.font[f];
    for (int g = 0; g < ft.n; ++g) {
        const uint8_t* e = ft.entries + g * GLYPH;
        if ((char)e[0] == c) {
            *w = e[1];
            return a.blob + u32(e + 4);
        }
    }
    return nullptr;
}

int width(const Art& a, FontId f, const char* s) {
    int w = 0, n = 0, gw = 0;
    for (; s && *s; ++s) {
        if (!glyph(a, f, *s, &gw)) continue;
        w += gw;
        ++n;
    }
    return n ? w + (n - 1) * a.font[f].track : 0;
}

// ---- colours ----
static void toHsl(uint32_t c, float* h, float* s, float* l) {
    const float r = ((c >> 16) & 255) / 255.0f, g = ((c >> 8) & 255) / 255.0f, b = (c & 255) / 255.0f;
    const float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
    *l = (mx + mn) / 2;
    *h = 0;
    *s = 0;
    if (mx == mn) return;
    const float d = mx - mn;
    *s = *l > 0.5f ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == r) *h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) *h = (b - r) / d + 2;
    else *h = (r - g) / d + 4;
    *h *= 60;
}
static uint32_t fromHsl(float h, float s, float l) {
    const float a = s * fminf(l, 1 - l);
    auto f = [&](float n) {
        const float k = fmodf(n + h / 30.0f, 12.0f);
        return l - a * fmaxf(-1.0f, fminf(k - 3, fminf(9 - k, 1.0f)));
    };
    auto ch = [](float v) { return (uint32_t)lroundf(fminf(1.0f, fmaxf(0.0f, v)) * 255.0f); };
    return (ch(f(0)) << 16) | (ch(f(8)) << 8) | ch(f(4));
}

float hueOf(uint32_t rgb) {
    float h, s, l;
    toHsl(rgb, &h, &s, &l);
    return h;
}

uint32_t recolour(uint32_t rgb, float h0, float hAcc, float sat, float dim) {
    float h, s, l;
    toHsl(rgb, &h, &s, &l);
    const bool skin = h >= 8 && h <= 55 && l > 0.72f && s > 0.2f;
    uint32_t c = rgb;
    if (!skin && s >= 0.12f) {
        float hn;
        if (h >= 225 && h < 290) hn = hAcc + (h - 258) * 0.3f;
        else hn = h0 + ((h >= 290 ? h - 360 : h) + 15) * 0.5f;
        hn = fmodf(fmodf(hn, 360.0f) + 360.0f, 360.0f);
        c = fromHsl(hn, s * sat, l);
    }
    if (dim < 1.0f) {
        auto d = [&](int sh) { return (uint32_t)lroundf(((c >> sh) & 255) * dim) << sh; };
        c = d(16) | d(8) | d(0);
    }
    return c;
}

static uint16_t be565(uint32_t c) {
    const uint16_t n = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c & 0xFF) >> 3));
    return (uint16_t)((n >> 8) | (n << 8));
}

static double lum(uint32_t c) {
    auto ch = [](double v) { v /= 255.0; return v <= 0.03928 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch((c >> 16) & 255) + 0.7152 * ch((c >> 8) & 255) + 0.0722 * ch(c & 255);
}
static double contrast(uint32_t a, uint32_t b) {
    const double x = lum(a), y = lum(b);
    return (fmax(x, y) + 0.05) / (fmin(x, y) + 0.05);
}

static constexpr uint32_t INK = 0x0D0008, DARK = 0x14000B, WINDOW = 0x12000B, WHITE = 0xFFFFFF;

void makeLook(const Art& a, int theme, bool night, Look* out) {
    const uint32_t tx = termpal::rgb(termpal::TEXT, theme, night), md = termpal::rgb(termpal::MID, theme, night),
                   dm = termpal::rgb(termpal::DIM, theme, night), al = termpal::rgb(termpal::ALERT, theme, night);
    const bool white = theme == 6;   // termpal's white: the art near greyscale
    const float h0 = hueOf(md), hAcc = hueOf(tx), sat = white ? 0.06f : 1.0f, dim = night ? 0.78f : 1.0f;
    for (int i = 0; i < COLOURS; ++i) {
        const uint8_t* p = a.pal + i * 3;
        out->art[i] = be565(recolour(((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2], h0, hAcc, sat, dim));
    }
    out->tx = be565(tx);
    out->md = be565(md);
    out->dm = be565(dm);
    out->al = be565(al);
    out->onTag = be565(contrast(INK, md) >= contrast(WHITE, md) ? INK : WHITE);
}

// ---- the values ----
Zone coolZone(float c) { return c < 77 ? Zone::Cold : c < 95 ? Zone::Norm : c < 105 ? Zone::Warn : Zone::Alert; }
Zone battZone(float v) { return v < 12 ? Zone::Alert : v <= 14.7f ? Zone::Norm : Zone::Warn; }
Zone pedalZone(float p) { return p < 85 ? Zone::Norm : Zone::Warn; }
static float clamp01(float f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
float coolFrac(float c) { return clamp01((c - 60) / 70); }
float battFrac(float v) { return clamp01((v - 11) / 4); }
float pedalFrac(float p) { return clamp01(p / 100); }
int pillsLit(float frac) { return std::isnan(frac) ? 0 : (int)lroundf(clamp01(frac) * PILLS); }

int gearEst(float rpm, float kmh) {
    if (!(kmh >= GEAR_MIN_KMH) || !(rpm > 0)) return 0;   // NAN too
    // the seven ratios (rpm per km/h): 125 * (20.4 / 125)^(k / 6); a gear's band reaches halfway (in
    // the log) to its neighbours'
    static const float EDGE[8] = {170.0f, 107.5f, 79.4f, 58.7f, 43.4f, 32.1f, 23.7f, 15.0f};
    const float r = rpm / kmh;
    if (r > EDGE[0] || r < EDGE[7]) return 0;
    for (int g = 1; g <= 7; ++g)
        if (r >= EDGE[g]) return g;
    return 7;
}

static const uint8_t CALM[5] = {5, 6, 7, 8, 9}, MID[2] = {10, 11};
static const uint8_t TENSE[13] = {0, 1, 2, 3, 4, 11, 12, 13, 14, 15, 16, 17, 18};
static uint32_t mix32(uint32_t x) {   // an integer hash: the shuffle's "random" pick per eighth of a second
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

int celIndex(float kmh, bool live, uint32_t nowMs) {
    if (!live || !(kmh >= 3)) return 7;
    const uint32_t q = nowMs / 250;   // a quarter second
    if (kmh < 80) {
        static const uint8_t PING[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        return CALM[PING[q % 8]];
    }
    if (kmh < 110) {
        const uint8_t A[6] = {CALM[4], MID[0], MID[1], MID[0], CALM[3], CALM[4]};
        return A[q % 6];
    }
    return TENSE[mix32(nowMs / 125) % 13];
}

int shake(float kmh, bool live, uint32_t nowMs) {
    if (!live || !(kmh >= 110)) return 0;
    return (nowMs * 14 / 1000) % 2 ? 1 : -1;
}

void Trace::update(float rpm, uint32_t nowMs) {
    if (any_ && nowMs - at_ < SAMPLE_MS) return;
    any_ = true;
    at_ = nowMs;
    memmove(v_, v_ + 1, sizeof v_ - sizeof v_[0]);
    v_[TRACE_N - 1] = std::isfinite(rpm) && rpm > 0 ? (uint16_t)fminf(rpm, 20000.0f) : 0;
    ++n_;
}

static void num(float v, int decimals, char* out, size_t n) {
    if (!std::isfinite(v)) {
        strncpy(out, "--", n);
        out[n - 1] = 0;
        return;
    }
    if (decimals) snprintf(out, n, "%.*f", decimals, (double)v);
    else snprintf(out, n, "%ld", lroundf(v));
}

static void ribbon(Ribbon& r, const char* label, float v, bool live, int decimals, const char* unit, float frac,
                   Zone (*zone)(float), float lo, float hi, bool bad) {
    strncpy(r.label, label, sizeof r.label);
    r.label[sizeof r.label - 1] = 0;
    const bool have = live && std::isfinite(v);
    num(have ? v : NAN, decimals, r.val, sizeof r.val);
    strncpy(r.unit, have ? unit : "", sizeof r.unit);
    r.unit[sizeof r.unit - 1] = 0;
    r.lit = (uint8_t)(have ? pillsLit(frac) : 0);
    r.zone = have ? zone(v) : Zone::Norm;
    for (int k = 0; k < PILLS; ++k) r.pill[k] = zone(lo + (hi - lo) * (k + 0.5f) / PILLS);
    r.bad = have && bad;
}

static void chip(Chip& c, const char* label, float v, int decimals, const char* unit) {
    strncpy(c.label, label, sizeof c.label);
    c.label[sizeof c.label - 1] = 0;
    num(v, decimals, c.val, sizeof c.val);
    strncpy(c.unit, std::isfinite(v) ? unit : "", sizeof c.unit);
    c.unit[sizeof c.unit - 1] = 0;
}

void scene(const obdwire::Live& l, const Trace& t, bool coolBad, bool battBad, uint32_t nowMs, Scene* out) {
    Scene s;
    s.live = l.link == obdplan::Link::Live;
    const float kmh = s.live ? l.kmh : NAN;
    if (std::isfinite(kmh)) {
        int v = (int)lroundf(fminf(fmaxf(kmh, 0.0f), 999.0f));   // by hand: -Werror=format-truncation
        for (int i = 2; i >= 0; --i) {
            s.speed[i] = (i == 2 || v) ? (char)('0' + v % 10) : ' ';
            v /= 10;
        }
        s.speed[3] = 0;
    }
    for (int i = 0; i < TRACE_N; ++i) s.trace[i] = t.at(i);
    s.gear = (uint8_t)(s.live ? gearEst(l.rpm, l.kmh) : 0);
    chip(s.chip[0], "KM/L", s.live ? l.kml : NAN, 1, "");
    chip(s.chip[1], "IAT", s.live ? l.intakeC : NAN, 0, "C");
    chip(s.chip[2], "TRIP", l.tripKm, 1, "KM");   // the trip stays shown without the link
    ribbon(s.rib[0], "PEDAL", l.throttle, s.live, 0, "%", pedalFrac(l.throttle), pedalZone, 0, 100, false);
    ribbon(s.rib[1], "COOL", l.coolC, s.live, 0, "C", coolFrac(l.coolC), coolZone, 60, 130, coolBad);
    ribbon(s.rib[2], "BATT", l.volts, s.live, 1, "V", battFrac(l.volts), battZone, 11, 15, battBad);
    s.pulse = (nowMs / 333) % 2 == 1;
    s.cel = (uint8_t)celIndex(kmh, s.live, nowMs);
    s.shake = (int8_t)shake(kmh, s.live, nowMs);
    *out = s;
}

// ---- geometry (the mock's 640 x 365 art coordinates, halved; y below the letterbox) ----
static float X(float v) { return v * 0.5f; }
static float Y(float v) { return v * 0.5f + ART_Y; }
static float L(float v) { return v * 0.5f; }   // a length

struct Rib { float ix0, ix1, iy, ih, tx0, tw, nx, trx0, trx1; };
static Rib rib(int i) {   // ribbons start at (3,248), (28,286), (54,324) and end at x 332, 357, 382
    static const float X0[3] = {3, 28, 54}, X1[3] = {332, 357, 382}, YY[3] = {248, 286, 324};
    const float ix0 = X0[i] + 26, ix1 = X1[i] - 14, iy = YY[i] + 4;
    return Rib{X(ix0), X(ix1), Y(iy), L(23), X(ix1 - 60), L(60), X(ix0 + 6), X(ix0 + 74), X(ix1 - 68)};
}
static constexpr float WX = 11, WY = 100, WW = 357, WH = 95;   // the bezel window (art coordinates)
static constexpr float GX = 287, GY = 210, GW = 108, GH = 38;  // the gear block
static constexpr float CY = 209, CH = 37, CW = 82;             // the chips: x 24 + i * 86

static Rect bounds(float x, float y, float w, float h, int pad) {
    const int x0 = (int)floorf(x) - pad, y0 = (int)floorf(y) - pad, x1 = (int)ceilf(x + w) + pad,
              y1 = (int)ceilf(y + h) + pad;
    return Rect{(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

Rect region(Region r) {
    switch (r) {
        case R_CEL: return Rect{CEL_X - 1, ART_Y, W - (CEL_X - 1), ART_H};
        case R_SPEED: return bounds(X(WX + 12), Y(WY + 10), L(3 * 34 + 2 * 7 + 4), L(WH - 12), 0);
        case R_TRACE: return bounds(X(WX + 148), Y(WY + 1), L(WW - 156), L(WH - 2), 0);
        case R_GEAR: return bounds(X(GX), Y(GY), L(GW), L(GH), 1);
        case R_CHIP0: case R_CHIP1: case R_CHIP2:
            return bounds(X(24 + (r - R_CHIP0) * 86), Y(CY), L(CW), L(CH), 1);
        case R_RIB0: case R_RIB1: case R_RIB2: {
            const Rib g = rib(r - R_RIB0);
            return bounds(g.ix0, g.iy, g.ix1 - g.ix0, g.ih, 1);
        }
        default: return Rect{0, 0, 0, 0};
    }
}

static uint32_t fnv(uint32_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
    return h;
}
template <class T>
static uint32_t fnv(uint32_t h, const T& v) { return fnv(h, &v, sizeof v); }

uint32_t key(const Scene& s, Region r) {
    uint32_t h = fnv(2166136261u, s.live);
    switch (r) {
        case R_CEL: h = fnv(h, s.cel); h = fnv(h, s.shake); h = fnv(h, s.gear); break;   // the gear block overlaps it
        case R_SPEED: h = fnv(h, s.speed); break;
        case R_TRACE: h = fnv(h, s.trace); break;
        case R_GEAR: h = fnv(h, s.gear); h = fnv(h, s.mode); h = fnv(h, s.cel); h = fnv(h, s.shake); break;
        case R_CHIP0: case R_CHIP1: case R_CHIP2: h = fnv(h, s.chip[r - R_CHIP0]); break;
        case R_RIB0: case R_RIB1: case R_RIB2: {
            const Ribbon& b = s.rib[r - R_RIB0];
            h = fnv(h, b);
            if (b.bad) h = fnv(h, s.pulse);
            break;
        }
        default: break;
    }
    return h;
}

// ---- drawing ----
static uint16_t swap(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }
static uint16_t mix(uint16_t dstBe, uint16_t srcBe, int alpha) {   // alpha 0..256
    if (alpha >= 256) return srcBe;
    if (alpha <= 0) return dstBe;
    const uint16_t d = swap(dstBe), s = swap(srcBe);
    const int dr = d >> 11, dg = (d >> 5) & 63, db = d & 31, sr = s >> 11, sg = (s >> 5) & 63, sb = s & 31;
    const int r = dr + (((sr - dr) * alpha) >> 8), g = dg + (((sg - dg) * alpha) >> 8), b = db + (((sb - db) * alpha) >> 8);
    return swap((uint16_t)((r << 11) | (g << 5) | b));
}

struct Canvas { Rect r; uint16_t* buf; };

// A rounded rectangle (x, y, w, h, corner rad) in screen pixels, anti-aliased by its signed distance:
// filled (lw 0) or its inner rim lw thick; colour c at opacity op (0..1).
static void rrect(Canvas& cv, float x, float y, float w, float h, float rad, uint16_t c, float op = 1.0f,
                  float lw = 0) {
    if (w <= 0 || h <= 0) return;
    rad = fminf(rad, fminf(w, h) / 2);
    const float cx = x + w / 2, cy = y + h / 2, hx = w / 2 - rad, hy = h / 2 - rad;
    const int x0 = (int)floorf(x) > cv.r.x ? (int)floorf(x) : cv.r.x;
    const int y0 = (int)floorf(y) > cv.r.y ? (int)floorf(y) : cv.r.y;
    const int x1 = (int)ceilf(x + w) < cv.r.x + cv.r.w ? (int)ceilf(x + w) : cv.r.x + cv.r.w;
    const int y1 = (int)ceilf(y + h) < cv.r.y + cv.r.h ? (int)ceilf(y + h) : cv.r.y + cv.r.h;
    for (int py = y0; py < y1; ++py) {
        const float qy = fabsf(py + 0.5f - cy) - hy;
        for (int px = x0; px < x1; ++px) {
            const float qx = fabsf(px + 0.5f - cx) - hx;
            const float ox = fmaxf(qx, 0.0f), oy = fmaxf(qy, 0.0f);
            const float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.0f) - rad;   // < 0 inside
            float cov = fminf(1.0f, fmaxf(0.0f, 0.5f - d));
            if (lw > 0) cov -= fminf(1.0f, fmaxf(0.0f, 0.5f - (d + lw)));
            if (cov <= 0) continue;
            uint16_t& p = cv.buf[(py - cv.r.y) * cv.r.w + (px - cv.r.x)];
            p = mix(p, c, (int)(cov * op * 256.0f + 0.5f));
        }
    }
}

enum class Align : uint8_t { Left, Right, Center };
// s in font f, colour c, x the left / right edge / centre, base the caps' bottom.
static void text(Canvas& cv, const Art& a, FontId f, float x, float base, const char* s, Align al, uint16_t c) {
    const Font& ft = a.font[f];
    const int tw = width(a, f, s), top = (int)lroundf(base) - ft.cap;
    int px = (int)lroundf(al == Align::Right ? x - tw : al == Align::Center ? x - tw / 2.0f : x);
    int gw = 0;
    for (; s && *s; ++s) {
        const uint8_t* g = glyph(a, f, *s, &gw);
        if (!g) continue;
        for (int gy = 0; gy < ft.h; ++gy) {
            const int by = top + gy - cv.r.y;
            if (by < 0 || by >= cv.r.h) continue;
            for (int gx = 0; gx < gw; ++gx) {
                const int bx = px + gx - cv.r.x;
                if (bx < 0 || bx >= cv.r.w) continue;
                const uint8_t al8 = g[gy * gw + gx];
                if (!al8) continue;
                uint16_t& d = cv.buf[by * cv.r.w + bx];
                d = mix(d, c, al8 + (al8 >> 7));
            }
        }
        px += gw + ft.track;
    }
}

static bool hits(const Rect& r, float x, float y, float w, float h) {
    return x - 2 < r.x + r.w && x + w + 2 > r.x && y - 2 < r.y + r.h && y + h + 2 > r.y;
}

static void art(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    const Rect& r = cv.r;
    for (int y = r.y; y < r.y + r.h; ++y) {
        uint16_t* row = cv.buf + (y - r.y) * r.w;
        const int ay = y - ART_Y;
        if (ay < 0 || ay >= ART_H) {
            memset(row, 0, (size_t)r.w * 2);
            continue;
        }
        const uint8_t* bg = a.bg + (size_t)ay * ART_W;
        const uint8_t* cel = a.cels + ((size_t)s.cel * ART_H + ay) * CEL_W;
        const int c0 = CEL_X + s.shake;
        for (int x = r.x; x < r.x + r.w; ++x) {
            const int cx = x - c0;
            row[x - r.x] = k.art[(cx >= 0 && cx < CEL_W) ? cel[cx] : bg[x]];
        }
    }
}

// The film's pill: a dark rim, the colour, a bright core (rounded all round).
static void pill(Canvas& cv, float px, float py, float pw, float ph, const uint16_t z[3], float rim) {
    rrect(cv, px - rim, py - rim, pw + 2 * rim, ph + 2 * rim, pw, z[0]);
    rrect(cv, px, py, pw, ph, pw / 2, z[1]);
    rrect(cv, px + pw * 0.3f, py + L(2), pw * 0.4f, fmaxf(L(2), ph - L(4)), pw * 0.2f, z[2]);
}

static void zoneColours(const Look& k, Zone z, uint16_t out[3]) {
    out[0] = out[1] = out[2] = k.md;
    switch (z) {
        case Zone::Cold: out[0] = k.dm; out[1] = k.dm; out[2] = k.md; break;
        case Zone::Norm: out[0] = k.dm; out[1] = k.md; out[2] = k.tx; break;
        case Zone::Warn: out[0] = k.md; out[1] = k.tx; out[2] = be565(WHITE); break;
        case Zone::Alert: out[0] = be565(0x7A0A0A); out[1] = k.al; out[2] = be565(0xFFD0D0); break;
    }
}

static void digit(Canvas& cv, char ch, float x, float y, float w, float h, float t, uint16_t c, float op) {
    const uint8_t segs = drivedash::segments(ch);
    const float H2 = h / 2, side = H2 - t * 1.2f;
    const float B[7][4] = {{x + t, y, w - 2 * t, t},           {x + w - t, y + t, t, side},
                           {x + w - t, y + H2 + t * 0.2f, t, side}, {x + t, y + h - t, w - 2 * t, t},
                           {x, y + H2 + t * 0.2f, t, side},     {x, y + t, t, side},
                           {x + t, y + H2 - t / 2, w - 2 * t, t}};
    for (int s = 0; s < 7; ++s)
        if (segs & (1 << s)) rrect(cv, B[s][0], B[s][1], B[s][2], B[s][3], 0, c, op);
}

static void window(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    if (!hits(cv.r, X(WX), Y(WY), L(WW), L(WH))) return;
    rrect(cv, X(WX), Y(WY), L(WW), L(WH), L(2), be565(WINDOW));
    // the speed: three seven-segment cells over a faint 888
    const float dw = L(34), dh = L(64), t = L(7), gap = L(7), px0 = X(WX + 14), py = Y(WY + 12);
    const uint16_t col = s.live ? k.tx : be565(0x555555);
    for (int i = 0; i < 3; ++i) {
        const float px = px0 + i * (dw + gap);
        digit(cv, '8', px, py, dw, dh, t, be565(WHITE), 0.07f);
        if (s.speed[i] && s.speed[i] != ' ') digit(cv, s.speed[i], px, py, dw, dh, t, col, 1.0f);
    }
    text(cv, a, F_SMALL, px0, Y(WY + WH - 5), "KM/H", Align::Left, k.md);
    // the RPM trace: TRACE_N pills, red over TRACE_RED
    const float x0 = X(WX + 150), w = L(WW - 160), gapT = w / TRACE_N, cyT = Y(WY + WH / 2);
    for (int i = 0; i < TRACE_N; ++i) {
        const uint16_t v = s.trace[i];
        const float fr = v ? fminf(1.0f, fmaxf(0.06f, (v - 700.0f) / 6800.0f)) : 0.06f;
        const float h = fmaxf(L(8), fr * L(WH - 8)), bw = fminf(L(9), gapT * 0.55f);
        const float px = x0 + i * gapT + gapT / 2 - bw / 2;
        if (!v) {
            rrect(cv, px, cyT - h / 2, bw, h, bw / 2, k.dm, 0.5f);
            continue;
        }
        uint16_t z[3];
        zoneColours(k, v > TRACE_RED ? Zone::Alert : Zone::Norm, z);
        pill(cv, px, cyT - h / 2, bw, h, z, L(2));
    }
}

static void gearBlock(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    if (!hits(cv.r, X(GX), Y(GY), L(GW), L(GH))) return;
    rrect(cv, X(GX), Y(GY), L(GW), L(GH), L(5), be565(DARK));
    rrect(cv, X(GX), Y(GY), L(GW), L(GH), L(5), be565(0x9AA0B0), 1.0f, 1.0f);
    const uint16_t col = s.live ? k.tx : be565(0x6A6A78);
    rrect(cv, X(GX + 6), Y(GY + 5), L(22), L(20), L(4), col);
    const char mode[2] = {s.mode, 0};
    text(cv, a, F_MODE, X(GX + 17), Y(GY + 15) + a.font[F_MODE].cap / 2.0f, mode, Align::Center, be565(INK));
    const char g[2] = {s.gear ? (char)('0' + s.gear) : '-', 0};
    text(cv, a, F_GEAR, X(GX + GW - 8), Y(GY + 17) + a.font[F_GEAR].cap / 2.0f, g, Align::Right, col);
    for (int p = 1; p <= 7; ++p)
        rrect(cv, X(GX + 7 + (p - 1) * 14.4f), Y(GY + GH - 9), L(11), L(4), L(2),
              s.live && p <= s.gear ? col : be565(0x3A2A44));
    text(cv, a, F_SMALL, X(GX + 33), Y(GY + 15) + a.font[F_SMALL].cap / 2.0f, "EST", Align::Left, k.md);
}

static void chips(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    for (int i = 0; i < 3; ++i) {
        const float px = 24 + i * 86;
        if (!hits(cv.r, X(px), Y(CY), L(CW), L(CH))) continue;
        const Chip& c = s.chip[i];
        rrect(cv, X(px), Y(CY), L(CW), L(CH), L(5), be565(DARK));
        rrect(cv, X(px), Y(CY), L(CW), L(CH), L(5), be565(0x9AA0B0), 0.7f, 1.0f);
        rrect(cv, X(px + 8), Y(CY), L(24), 1.0f, 0, k.md);
        text(cv, a, F_SMALL, X(px + 8), Y(CY + 17), c.label, Align::Left, k.md);
        const bool unit = c.unit[0] != 0;
        const uint16_t vc = s.live ? k.tx : be565(0x8A8A99);
        text(cv, a, F_VALUE, X(px + CW - (unit ? 27 : 7)), Y(CY + 33), c.val, Align::Right, vc);
        if (unit) text(cv, a, F_SMALL, X(px + CW - 24), Y(CY + 33), c.unit, Align::Left, k.md);
    }
}

static void ribbons(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    for (int i = 0; i < 3; ++i) {
        const Rib g = rib(i);
        if (!hits(cv.r, g.ix0, g.iy, g.ix1 - g.ix0, g.ih)) continue;
        const Ribbon& b = s.rib[i];
        const bool pulse = b.bad && s.pulse;
        rrect(cv, g.ix0, g.iy, g.ix1 - g.ix0, g.ih, L(6), be565(INK));
        rrect(cv, g.ix0, g.iy, g.ix1 - g.ix0, g.ih, L(6),
              b.bad ? (pulse ? be565(0xFF6A6A) : k.al) : be565(0x6A6A78), 1.0f, b.bad ? 1.5f : 1.0f);
        const float mid = g.iy + g.ih / 2;
        const uint16_t vc = b.bad ? k.al : s.live ? k.tx : be565(0x8A8A99);
        const float vbase = mid + a.font[F_VALUE].cap / 2.0f;
        text(cv, a, F_VALUE, g.nx + L(44), vbase, b.val, Align::Right, vc);
        if (b.unit[0]) text(cv, a, F_SMALL, g.nx + L(47), vbase, b.unit, Align::Left, b.bad ? be565(0xFF9A9A) : k.md);
        // the track
        const float w = (g.trx1 - g.trx0) / PILLS;
        uint16_t zc[3];
        zoneColours(k, b.zone, zc);
        for (int p = 0; p < PILLS; ++p) {
            const bool on = p < b.lit;
            const float pw = w * 0.6f, px = g.trx0 + p * w + w * 0.2f, ph = on ? g.ih - L(8) : g.ih - L(15);
            const float py = g.iy + (g.ih - ph) / 2;
            if (on) {
                pill(cv, px, py, pw, ph, zc, 0.5f);
            } else {
                uint16_t zz[3];
                zoneColours(k, b.pill[p], zz);
                rrect(cv, px, py, pw, ph, pw / 2, zz[0], 0.32f);
            }
        }
        // the tag
        const uint16_t tag = b.bad ? (pulse ? be565(0xE0404A) : be565(0xC01020)) : k.md;
        rrect(cv, g.tx0, g.iy + L(1), g.tw, g.ih - L(2), L(5), tag);
        text(cv, a, F_SMALL, g.tx0 + g.tw / 2 + 0.5f, mid + a.font[F_SMALL].cap / 2.0f, b.label, Align::Center,
             b.bad ? be565(WHITE) : k.onTag);
    }
}

void compose(const Art& a, const Look& k, const Scene& s, Rect r, uint16_t* buf) {
    if (r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0 || r.x + r.w > W || r.y + r.h > H) return;   // the screen only
    Canvas cv{r, buf};
    art(a, k, s, cv);
    window(a, k, s, cv);
    gearBlock(a, k, s, cv);
    chips(a, k, s, cv);
    ribbons(a, k, s, cv);
    if (s.demo) text(cv, a, F_SMALL, 6, ART_Y - 10, "DEMO", Align::Left, k.al);
    if (!s.live) {   // the link is lost: everything at 65 %
        const uint16_t black = 0;
        for (int i = 0; i < r.w * r.h; ++i) buf[i] = mix(buf[i], black, 90);   // 90 / 256 ~ 35 % black
    }
}

}  // namespace sukiride
