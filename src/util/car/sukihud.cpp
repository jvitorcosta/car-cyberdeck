#include "sukihud.h"
#include <cmath>
#include <cstring>

namespace sukihud {

static uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool parse(const uint8_t* blob, size_t len, Art* out) {
    if (!blob || !out || len < HEAD || memcmp(blob, "SUK1", 4) != 0) return false;
    if (u16(blob + 4) != W || u16(blob + 6) != H || u16(blob + 8) != FONTS) return false;
    size_t at = HEAD + (size_t)W * H * 2;
    if (at > len) return false;
    Art a;
    a.blob = blob;
    a.len = len;
    a.bg = blob + HEAD;
    for (int f = 0; f < FONTS; ++f) {
        if (at + FONT_HEAD > len) return false;
        Font& ft = a.font[f];
        ft.h = blob[at];
        ft.track = (int8_t)blob[at + 1];   // negative: the glyphs' dark edges overlap, as the film's text does
        ft.n = blob[at + 2];
        at += FONT_HEAD;
        if (ft.h == 0 || at + (size_t)ft.n * GLYPH > len) return false;
        ft.entries = blob + at;
        for (int g = 0; g < ft.n; ++g) {
            const uint8_t* e = ft.entries + g * GLYPH;
            const size_t off = u32(e + 4), size = (size_t)e[1] * ft.h * 3;
            if (off > len || size > len - off) return false;
        }
        at += (size_t)ft.n * GLYPH;
    }
    *out = a;
    return true;
}

// Glyph c of font f: its width and pixels, or nullptr.
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

void background(const Art& a, Rect r, uint16_t* buf) {
    for (int y = 0; y < r.h; ++y) {
        const int sy = r.y + y;
        if (sy < 0 || sy >= H) continue;
        memcpy(buf + y * r.w, a.bg + ((size_t)sy * W + r.x) * 2, (size_t)r.w * 2);
    }
}

static uint16_t swap(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

// Big-endian src over big-endian dst at alpha 0..255.
static uint16_t mix(uint16_t dstBe, uint16_t srcBe, uint8_t alpha) {
    if (alpha == 255) return srcBe;
    if (alpha == 0) return dstBe;
    const uint16_t d = swap(dstBe), s = swap(srcBe);
    const int dr = d >> 11, dg = (d >> 5) & 63, db = d & 31, sr = s >> 11, sg = (s >> 5) & 63, sb = s & 31;
    const int r = dr + ((sr - dr) * alpha + 127) / 255, g = dg + ((sg - dg) * alpha + 127) / 255,
              b = db + ((sb - db) * alpha + 127) / 255;
    return swap((uint16_t)((r << 11) | (g << 5) | b));
}

void text(const Art& a, FontId f, Rect r, uint16_t* buf, int x, int yMid, const char* s, Align al) {
    const int tw = width(a, f, s), h = a.font[f].h, top = yMid - h / 2;
    if (al == Align::Right) x -= tw;
    else if (al == Align::Center) x -= tw / 2;
    int gw = 0;
    for (; s && *s; ++s) {
        const uint8_t* px = glyph(a, f, *s, &gw);
        if (!px) continue;
        for (int gy = 0; gy < h; ++gy) {
            const int by = top + gy - r.y;
            if (by < 0 || by >= r.h) continue;
            for (int gx = 0; gx < gw; ++gx) {
                const int bx = x + gx - r.x;
                if (bx < 0 || bx >= r.w) continue;
                const uint8_t* p = px + (gy * gw + gx) * 3;
                uint16_t& d = buf[by * r.w + bx];
                d = mix(d, (uint16_t)(p[0] | (p[1] << 8)), p[2]);   // p[0] is the high byte: big-endian in memory
            }
        }
        x += gw + a.font[f].track;
    }
}

void capsule(Rect r, uint16_t* buf, int cx, int y0, int y1, int w, uint16_t rim, uint16_t core) {
    if (w < 1 || y1 < y0) return;
    const float rad = w / 2.0f, left = cx - rad;
    for (int y = y0; y <= y1; ++y) {
        // the ends are half discs: the row's half-width there
        float half = rad;
        const float top = y0 + rad, bot = y1 - rad;
        const float dy = y < top ? top - (y + 0.5f) : (y > bot ? (y + 0.5f) - bot : 0.0f);
        if (dy > 0.0f) half = dy >= rad ? 0.0f : sqrtf(rad * rad - dy * dy);
        if (half <= 0.0f) continue;
        const int xa = (int)lroundf(cx - half), xb = (int)lroundf(cx + half) - 1;
        for (int x = xa; x <= xb; ++x) {
            const int bx = x - r.x, by = y - r.y;
            if (bx < 0 || bx >= r.w || by < 0 || by >= r.h) continue;
            uint16_t c = rim;
            const bool inCore = core && y >= y0 + 2 && y <= y1 - 2 && w >= 3 &&
                                std::fabs((x + 0.5f) - (left + rad)) <= (w >= 5 ? 1.0f : 0.5f);
            if (inCore) c = core;
            buf[by * r.w + bx] = c;
        }
    }
}

int tachLit(float rpm) {
    if (std::isnan(rpm) || rpm <= 0.0f) return 0;
    const int n = (int)lroundf(TACH_N * rpm / TACH_MAX);
    return n > TACH_N ? TACH_N : n;
}
bool tachHot(int i) { return i >= (int)lroundf(TACH_N * SHIFT_RPM / TACH_MAX); }
int tachX(int i) { return 116 + (int)lroundf(i * 7.4f); }
int tachTop(int i) { return TACH_BOTTOM - 10 - (int)lroundf(i * 0.75f); }

int loadFill(float pct) {
    if (std::isnan(pct) || pct <= 0.0f) return 0;
    if (pct >= 100.0f) return LOAD_H;
    return (int)lroundf(LOAD_H * pct / 100.0f);
}

}  // namespace sukihud
