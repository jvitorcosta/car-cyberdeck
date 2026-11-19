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
    if (!blob || !out || len < HEAD || memcmp(blob, "SKR2", 4) != 0) return false;
    if (u16(blob + 4) != ART_X || u16(blob + 6) != ART_W || u16(blob + 8) != ART_H || u16(blob + 10) != CELS ||
        u16(blob + 12) != FONTS || u16(blob + 14) != LOGO_W || u16(blob + 16) != LOGO_H)
        return false;
    Art a;
    a.blob = blob;
    a.len = len;
    size_t at = HEAD;
    a.pal = blob + at;
    at += COLOURS * 3;
    a.cels = blob + at;
    at += (size_t)CELS * ART_W * ART_H;
    a.logo = blob + at;
    at += (size_t)LOGO_W * LOGO_H;
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

static constexpr uint32_t INK = 0x0D0008, WHITE = 0xFFFFFF;

static uint32_t scale(uint32_t c, float k, float toWhite) {   // x k toward black, then toWhite toward white
    auto ch = [&](int sh) {
        const float v = ((c >> sh) & 255) * k;
        return (uint32_t)lroundf(fminf(255.0f, v + (255.0f - v) * toWhite)) << sh;
    };
    return ch(16) | ch(8) | ch(0);
}

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
    out->onAlert = be565(contrast(INK, al) >= contrast(WHITE, al) ? INK : WHITE);
    out->pipOff = be565(scale(dm, 0.35f, 0));
    // the panel: the theme's dim at 10 %, 3.5 % toward white at the top, 25 % darker at the bottom
    const uint32_t base = scale(dm, 0.10f, 0);
    for (int y = 0; y < H; ++y) {
        const float f = (float)y / (H - 1);
        out->bg[y] = be565(scale(scale(base, 1.0f, 0.035f * (1 - f)), 1.0f - 0.25f * f, 0));
    }
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

static void whole(float v, int maxv, char* out, size_t n) {   // by hand: -Werror=format-truncation
    if (!std::isfinite(v)) {
        strncpy(out, "--", n);
        out[n - 1] = 0;
        return;
    }
    int x = (int)lroundf(fminf(fmaxf(v, 0.0f), (float)maxv));
    char tmp[12];
    int k = 0;
    do {
        tmp[k++] = (char)('0' + x % 10);
        x /= 10;
    } while (x && k < (int)sizeof tmp);
    size_t i = 0;
    while (k && i + 1 < n) out[i++] = tmp[--k];
    out[i] = 0;
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
    whole(s.live ? l.rpm : NAN, 99999, s.rpm, sizeof s.rpm);
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

// ---- geometry (screen pixels: the v19 mock's 640 x 480, halved) ----
static constexpr float PX = 5, PW = 174;                       // the panel's cards: x 5..179
static constexpr float LOGO_X = 5, LOGO_Y = 5, SHADOW = 2;     // SUKI, its shadow offset
static constexpr float TAG_Y = 11, TAG_H = 11;                 // the state tag (RIDE, DEMO, NO LINK)
static constexpr float SY = 31, SH = 68;                       // the speed card
static constexpr float DW = 23, DH = 43, DT = 5, DGAP = 4, DX = 11, DY = 48;   // the speed's cells
static constexpr float RX = 102, RX1 = 173;                    // the RPM trace
static constexpr float TY = 103, TH = 31, TW = 40.5f, TGAP = 4; // the four tiles
static constexpr float BY = 138, BGAP = 4;                     // the bars share the rest
static constexpr float BH = (H - 5 - BY - 2 * BGAP) / 3;
static float tileX(int i) { return PX + i * (TW + TGAP); }
static float barY(int i) { return BY + i * (BH + BGAP); }

static Rect bounds(float x, float y, float w, float h, int pad) {
    const int x0 = (int)floorf(x) - pad, y0 = (int)floorf(y) - pad, x1 = (int)ceilf(x + w) + pad,
              y1 = (int)ceilf(y + h) + pad;
    return Rect{(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

Rect region(Region r) {
    switch (r) {
        case R_CEL: return Rect{CEL_REGION_X, 0, W - CEL_REGION_X, H};
        case R_SPEED: return bounds(DX, DY, 3 * DW + 2 * DGAP, DH, 0);
        case R_TRACE: return bounds(RX - 1, SY + 1, RX1 + 1 - (RX - 1), SH - 2, 0);
        case R_GEAR: return bounds(tileX(3), TY, TW, TH, 1);
        case R_CHIP0: case R_CHIP1: case R_CHIP2: return bounds(tileX(r - R_CHIP0), TY, TW, TH, 1);
        case R_RIB0: case R_RIB1: case R_RIB2: return bounds(PX, barY(r - R_RIB0), PW, BH, 1);
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
        case R_CEL: h = fnv(h, s.cel); h = fnv(h, s.shake); break;
        case R_SPEED: h = fnv(h, s.speed); break;
        case R_TRACE: h = fnv(h, s.trace); h = fnv(h, s.rpm); break;
        case R_GEAR: h = fnv(h, s.gear); break;
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

// An alpha mask (w x h bytes) at (x, y) in colour c.
static void mask(Canvas& cv, const uint8_t* m, int w, int h, int x, int y, uint16_t c) {
    for (int my = 0; my < h; ++my) {
        const int by = y + my - cv.r.y;
        if (by < 0 || by >= cv.r.h) continue;
        for (int mx = 0; mx < w; ++mx) {
            const int bx = x + mx - cv.r.x;
            if (bx < 0 || bx >= cv.r.w) continue;
            const uint8_t al8 = m[my * w + mx];
            if (!al8) continue;
            uint16_t& d = cv.buf[by * cv.r.w + bx];
            d = mix(d, c, al8 + (al8 >> 7));
        }
    }
}

enum class Align : uint8_t { Left, Right, Center };
// s in font f, colour c, x the left / right edge / centre, base the caps' bottom; extra: more tracking.
static void text(Canvas& cv, const Art& a, FontId f, float x, float base, const char* s, Align al, uint16_t c,
                 int extra = 0) {
    const Font& ft = a.font[f];
    int tw = width(a, f, s), n = 0, gw = 0;
    for (const char* q = s; q && *q; ++q) n += glyph(a, f, *q, &gw) ? 1 : 0;
    if (n > 1) tw += (n - 1) * extra;
    const int top = (int)lroundf(base) - ft.cap;
    int px = (int)lroundf(al == Align::Right ? x - tw : al == Align::Center ? x - tw / 2.0f : x);
    for (; s && *s; ++s) {
        const uint8_t* g = glyph(a, f, *s, &gw);
        if (!g) continue;
        mask(cv, g, gw, ft.h, px, top, c);
        px += gw + ft.track + extra;
    }
}

static bool hits(const Rect& r, float x, float y, float w, float h) {
    return x - 2 < r.x + r.w && x + w + 2 > r.x && y - 2 < r.y + r.h && y + h + 2 > r.y;
}

// The panel's background on the left, the girl on the right, the one fading into the other.
static void ground(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    const Rect& r = cv.r;
    for (int y = r.y; y < r.y + r.h; ++y) {
        uint16_t* row = cv.buf + (y - r.y) * r.w;
        const uint8_t* cel = a.cels + ((size_t)s.cel * ART_H + y) * ART_W;
        for (int x = r.x; x < r.x + r.w; ++x) {
            uint16_t& p = row[x - r.x];
            if (x < PANEL_W) {
                p = k.bg[y];
                continue;
            }
            int cx = x - ART_X - s.shake;
            cx = cx < 0 ? 0 : cx >= ART_W ? ART_W - 1 : cx;
            p = k.art[cel[cx]];
            if (x < PANEL_W + FADE_W) p = mix(p, k.bg[y], 256 - (x - PANEL_W) * 256 / FADE_W);
        }
    }
}

// One card: dark glass over the panel, the theme's dim border and its mid tick at the top left; an
// alert card has the alert colour's border instead (white on the pulse).
static void card(Canvas& cv, const Look& k, float x, float y, float w, float h, bool bad = false,
                 bool pulse = false) {
    if (!hits(cv.r, x, y, w, h)) return;
    rrect(cv, x, y, w, h, 4, 0, 0.55f);
    if (bad) {
        rrect(cv, x, y, w, h, 4, pulse ? be565(WHITE) : k.al, 1.0f, 1.5f);
        return;
    }
    rrect(cv, x, y, w, h, 4, k.dm, 1.0f, 0.75f);
    rrect(cv, x + 5, y, 14, 1.5f, 0, k.md);
}

static void head(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    if (hits(cv.r, LOGO_X, LOGO_Y, LOGO_W + SHADOW, LOGO_H + SHADOW)) {
        mask(cv, a.logo, LOGO_W, LOGO_H, (int)(LOGO_X + SHADOW), (int)(LOGO_Y + SHADOW), k.dm);
        mask(cv, a.logo, LOGO_W, LOGO_H, (int)LOGO_X, (int)LOGO_Y, k.tx);
    }
    const char* tag = !s.live ? "NO LINK" : s.demo ? "DEMO" : "RIDE";
    const bool warn = !s.live || s.demo;
    const float tw = width(a, F_SMALL, tag) + (float)strlen(tag) + 8, tx = PX + PW - tw;
    if (!hits(cv.r, tx, TAG_Y, tw, TAG_H)) return;
    // "NO LINK" has a space the font hasn't: two words
    const uint16_t c = warn ? k.al : k.md;
    rrect(cv, tx, TAG_Y, tw, TAG_H, TAG_H / 2, c, 1.0f, 0.75f);
    const float base = TAG_Y + TAG_H / 2 + a.font[F_SMALL].cap / 2.0f;
    if (!s.live) {
        const float w1 = width(a, F_SMALL, "NO") + 1, w2 = width(a, F_SMALL, "LINK") + 3, gap = 3;
        const float x0 = tx + (tw - (w1 + gap + w2)) / 2;
        text(cv, a, F_SMALL, x0, base, "NO", Align::Left, c, 1);
        text(cv, a, F_SMALL, x0 + w1 + gap, base, "LINK", Align::Left, c, 1);
    } else {
        text(cv, a, F_SMALL, tx + tw / 2 + 0.5f, base, tag, Align::Center, c, 1);
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

static void speedCard(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    if (!hits(cv.r, PX, SY, PW, SH)) return;
    card(cv, k, PX, SY, PW, SH);
    const float lb = SY + 10.5f;
    text(cv, a, F_SMALL, DX, lb, "SPEED", Align::Left, k.md, 1);
    text(cv, a, F_SMALL, DX + width(a, F_SMALL, "SPEED") + 4 + 6, lb, "KM/H", Align::Left, k.md, 1);
    text(cv, a, F_SMALL, RX, lb, "RPM", Align::Left, k.md, 1);
    text(cv, a, F_SMALL, RX1, lb, s.rpm, Align::Right, s.live ? k.tx : k.dm);
    // the speed: three seven-segment cells over a faint 888
    for (int i = 0; i < 3; ++i) {
        const float px = DX + i * (DW + DGAP);
        if (!hits(cv.r, px, DY, DW, DH)) continue;
        digit(cv, '8', px, DY, DW, DH, DT, k.tx, 0.08f);
        if (s.speed[i] && s.speed[i] != ' ') digit(cv, s.speed[i], px, DY, DW, DH, DT, s.live ? k.tx : k.dm, 1.0f);
    }
    // the RPM trace: TRACE_N bars standing on the cells' baseline, the alert colour over TRACE_RED
    const float gap = (RX1 - RX) / TRACE_N, bw = fminf(4.5f, gap * 0.6f), base = DY + DH;
    for (int i = 0; i < TRACE_N; ++i) {
        const uint16_t v = s.trace[i];
        const float bx = RX + i * gap + (gap - bw) / 2;
        if (!v) {
            rrect(cv, bx, base - 2.5f, bw, 2.5f, bw / 2, k.dm, 0.5f);
            continue;
        }
        const float fr = fminf(1.0f, fmaxf(0.08f, (v - 700.0f) / 6800.0f)), hh = fmaxf(2.5f, fr * (DH - 3));
        const bool red = v > TRACE_RED;
        rrect(cv, bx, base - hh, bw, hh, bw / 2, red ? k.al : k.md);
        rrect(cv, bx + bw * 0.35f, base - hh + 1, bw * 0.3f, fmaxf(0.5f, hh - 2), 0, red ? be565(0xFFD0D0) : k.tx, 0.6f);
    }
}

static void tiles(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    for (int i = 0; i < 3; ++i) {
        const float x = tileX(i);
        if (!hits(cv.r, x, TY, TW, TH)) continue;
        const Chip& c = s.chip[i];
        card(cv, k, x, TY, TW, TH);
        text(cv, a, F_SMALL, x + 5, TY + 10.5f, c.label, Align::Left, k.md);
        // the unit on the label's line, so the value has the tile's whole width
        if (c.unit[0]) text(cv, a, F_SMALL, x + TW - 4, TY + 10.5f, c.unit, Align::Right, k.dm);
        const bool shown = s.live || i == 2;   // the trip stays
        text(cv, a, F_VALUE, x + TW - 4, TY + TH - 5.5f, c.val, Align::Right, shown ? k.tx : k.dm);
    }
    const float x = tileX(3);
    if (!hits(cv.r, x, TY, TW, TH)) return;
    card(cv, k, x, TY, TW, TH);
    text(cv, a, F_SMALL, x + 5, TY + 10.5f, "GEAR", Align::Left, k.md);
    text(cv, a, F_SMALL, x + 21, TY + TH - 12, "EST", Align::Left, k.dm);
    const uint16_t col = s.live ? k.tx : k.dm;
    const char g[2] = {s.gear ? (char)('0' + s.gear) : '-', 0};
    text(cv, a, F_GEAR, x + 5, TY + TH - 4, g, Align::Left, col);
    const float px0 = x + 21, pw = (x + TW - 4 - px0) / 7;
    for (int p = 1; p <= 7; ++p)
        rrect(cv, px0 + (p - 1) * pw, TY + TH - 8, pw - 1, 2.5f, 1, s.live && p <= s.gear ? k.md : k.pipOff);
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

static void bars(const Art& a, const Look& k, const Scene& s, Canvas& cv) {
    for (int i = 0; i < 3; ++i) {
        const float y = barY(i);
        if (!hits(cv.r, PX, y, PW, BH)) continue;
        const Ribbon& b = s.rib[i];
        const bool pulse = b.bad && s.pulse;
        card(cv, k, PX, y, PW, BH, b.bad, pulse);
        const float lb = y + 10.5f;
        if (b.bad) {   // the label in a box of the alert colour (white on the pulse)
            const uint16_t box = pulse ? be565(WHITE) : k.al;
            rrect(cv, PX + 4, y + 3.5f, 31, 9, 2, box);
            text(cv, a, F_SMALL, PX + 7, lb, b.label, Align::Left, pulse ? be565(INK) : k.onAlert, 1);
        } else {
            text(cv, a, F_SMALL, PX + 6, lb, b.label, Align::Left, k.md, 1);
        }
        const float vb = y + 13.5f;
        float uw = 0;
        if (b.unit[0]) {
            text(cv, a, F_SMALL, PX + PW - 6, vb, b.unit, Align::Right, b.bad ? k.al : k.md);
            uw = width(a, F_SMALL, b.unit) + 2;
        }
        text(cv, a, F_BIG, PX + PW - 6 - uw, vb, b.val, Align::Right, b.bad ? k.al : s.live ? k.tx : k.dm);
        // the track
        const float x0 = PX + 6, gw = (PW - 12) / PILLS, ph = 5.5f, py = y + BH - ph - 4.5f;
        uint16_t zc[3];
        zoneColours(k, b.zone, zc);
        for (int p = 0; p < PILLS; ++p) {
            const float px = x0 + p * gw + 1, pw = gw - 2;
            if (p < b.lit) {
                rrect(cv, px, py, pw, ph, 1.5f, zc[1]);
                rrect(cv, px + 1, py + 1, pw - 2, 1, 0, zc[2], 0.55f);
            } else {
                uint16_t zz[3];
                zoneColours(k, b.pill[p], zz);
                rrect(cv, px, py, pw, ph, 1.5f, zz[0], 0.28f);
            }
        }
    }
}

void compose(const Art& a, const Look& k, const Scene& s, Rect r, uint16_t* buf) {
    if (r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0 || r.x + r.w > W || r.y + r.h > H) return;   // the screen only
    Canvas cv{r, buf};
    ground(a, k, s, cv);
    head(a, k, s, cv);
    speedCard(a, k, s, cv);
    tiles(a, k, s, cv);
    bars(a, k, s, cv);
    if (!s.live) {   // the link is lost: everything at 65 %
        const uint16_t black = 0;
        for (int i = 0; i < r.w * r.h; ++i) buf[i] = mix(buf[i], black, 90);   // 90 / 256 ~ 35 % black
    }
}

}  // namespace sukiride
