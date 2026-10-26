#include "songpalette.h"
#include "rgb565.h"

namespace songpalette {
namespace {

struct Hsv { int h, s, v; };   // h 0..359, s and v 0..255

Hsv toHsv(Rgb c) {
    int mx = c.r, mn = c.r;
    if (c.g > mx) mx = c.g;
    if (c.b > mx) mx = c.b;
    if (c.g < mn) mn = c.g;
    if (c.b < mn) mn = c.b;
    int d = mx - mn;
    Hsv o{0, mx ? d * 255 / mx : 0, mx};
    if (!d) return o;
    int h;
    if (mx == c.r) h = 60 * (c.g - c.b) / d;
    else if (mx == c.g) h = 120 + 60 * (c.b - c.r) / d;
    else h = 240 + 60 * (c.r - c.g) / d;
    o.h = (h + 360) % 360;
    return o;
}

Rgb fromHsv(int h, int s, int v) {   // s and v 0..255
    h = ((h % 360) + 360) % 360;
    int f = (h % 60) * 255 / 60;
    int hi = 255, up = f, down = 255 - f, lo = 0;
    int r, g, b;
    switch (h / 60) {
        case 0: r = hi; g = up; b = lo; break;
        case 1: r = down; g = hi; b = lo; break;
        case 2: r = lo; g = hi; b = up; break;
        case 3: r = lo; g = down; b = hi; break;
        case 4: r = up; g = lo; b = hi; break;
        default: r = hi; g = lo; b = down; break;
    }
    // desaturate toward white by (255 - s), then scale by v
    r = (255 - (255 - r) * s / 255) * v / 255;
    g = (255 - (255 - g) * s / 255) * v / 255;
    b = (255 - (255 - b) * s / 255) * v / 255;
    return {(uint8_t)r, (uint8_t)g, (uint8_t)b};
}

Rgb fromHue(int h) { return fromHsv(h, 255, 255); }

int hueDist(int a, int b) {   // circular distance 0..180
    int d = ((a - b) % 360 + 360) % 360;
    return d > 180 ? 360 - d : d;
}

// Signed shortest step from a to b, -180..180.
int hueDelta(int a, int b) {
    int d = ((b - a) % 360 + 360) % 360;
    return d > 180 ? d - 360 : d;
}

constexpr int BUCKETS = 24;           // 15 deg each
constexpr int MIN_SHARE_PCT = 3;      // a hue must cover this much of the picture to count
constexpr Rgb AMBER = {255, 176, 0};  // what a white turns into after dark

constexpr Rgb RED = {255, 24, 24}, ORANGE = {255, 110, 0}, DEEP_ORANGE = {255, 60, 0};
constexpr Rgb BLUE = {0, 80, 255}, PURPLE = {160, 32, 255}, CYAN = {0, 220, 255};
constexpr Rgb PINK = {255, 64, 160}, MAGENTA = {255, 0, 200}, VIOLET = {140, 60, 255};
constexpr Rgb GOLD = {255, 190, 0}, TEAL = {0, 200, 170}, WARM_WHITE = {255, 214, 160};
constexpr Rgb SOFT_AMBER = {255, 170, 40}, GREEN = {0, 220, 60}, YELLOW = {255, 235, 0};

}  // namespace

int fromCover(const uint16_t* bmpBe, int w, int h, Rgb out[3]) {
    if (!bmpBe || w <= 0 || h <= 0) return 0;
    int count[BUCKETS] = {};
    long hueSum[BUCKETS] = {};
    for (long i = 0, n = (long)w * h; i < n; ++i) {
        uint16_t p = rgb565::be(bmpBe[i]);
        Rgb c = {(uint8_t)((p >> 8) & 0xF8), (uint8_t)((p >> 3) & 0xFC), (uint8_t)((p << 3) & 0xF8)};
        Hsv v = toHsv(c);
        if (v.s * 100 < 25 * 255 || v.v * 100 < 15 * 255) continue;
        if (v.v * 100 > 95 * 255 && v.s * 100 < 50 * 255) continue;
        ++count[v.h / 15];
        hueSum[v.h / 15] += v.h;
    }
    long minCount = ((long)w * h * MIN_SHARE_PCT + 99) / 100;
    int picked = 0, hues[3] = {};
    bool used[BUCKETS] = {};
    while (picked < 3) {
        int best = -1;
        for (int b = 0; b < BUCKETS; ++b) {
            if (used[b] || count[b] < minCount || !count[b]) continue;
            if (best < 0 || count[b] > count[best]) best = b;
        }
        if (best < 0) break;
        used[best] = true;
        int hue = (int)(hueSum[best] / count[best]);
        bool apart = true;
        for (int k = 0; k < picked; ++k)
            if (hueDist(hue, hues[k]) < 30) apart = false;
        if (!apart) continue;
        hues[picked] = hue;
        out[picked] = fromHue(hue);
        ++picked;
    }
    return picked;
}

void family(uint8_t badge, Rgb out[3]) {
    static const Rgb ROCK[3] = {RED, ORANGE, DEEP_ORANGE};
    static const Rgb ELECTRO[3] = {BLUE, PURPLE, CYAN};
    static const Rgb POP[3] = {PINK, MAGENTA, VIOLET};
    static const Rgb RAP[3] = {PURPLE, GOLD, MAGENTA};
    static const Rgb CALM[3] = {TEAL, WARM_WHITE, SOFT_AMBER};
    static const Rgb BRASIL[3] = {AMBER, GREEN, YELLOW};
    static const Rgb NEUTRAL[3] = {TEAL, PURPLE, WARM_WHITE};
    const Rgb* f;
    if (badge <= 2) f = POP;
    else if (badge <= 5 || badge == 27) f = RAP;
    else if (badge <= 8) f = ROCK;
    else if (badge <= 10) f = ELECTRO;
    else if (badge <= 20) f = BRASIL;
    else if (badge <= 26) f = CALM;   // 21..26: jazz, classic, OST, folk, country, gospel
    else f = NEUTRAL;                 // 28 chinese, 29 world, 30 ???, NONE and anything unknown
    for (int i = 0; i < 3; ++i) out[i] = f[i];
}

Palette build(const Rgb* cover, int coverCount, uint8_t badge, int minuteOfDay) {
    Palette p{};
    Rgb fam[3];
    family(badge, fam);
    if (!cover || coverCount < 2) {
        for (int i = 0; i < 3; ++i) p.c[i] = fam[i];
    } else {
        int hues[3] = {};
        int n = coverCount > 3 ? 3 : coverCount;
        for (int i = 0; i < n; ++i) {
            int hue = toHsv(cover[i]).h, bestD = 361, target = hue;
            for (int k = 0; k < 3; ++k) {
                int fh = toHsv(fam[k]).h, d = hueDist(hue, fh);
                if (d < bestD) { bestD = d; target = fh; }
            }
            int step = hueDelta(hue, target);
            if (step > 15) step = 15;
            if (step < -15) step = -15;
            hues[i] = hue + step;
            p.c[i] = fromHue(hues[i]);
        }
        if (n < 3) {   // the family colour farthest from every cover hue fills the last slot
            int bestK = 0, bestD = -1;
            for (int k = 0; k < 3; ++k) {
                int nearest = 180, fh = toHsv(fam[k]).h;
                for (int i = 0; i < n; ++i) {
                    int d = hueDist(fh, hues[i]);
                    if (d < nearest) nearest = d;
                }
                if (nearest > bestD) { bestD = nearest; bestK = k; }
            }
            p.c[2] = fam[bestK];
        }
    }
    bool day = minuteOfDay < 0 || (minuteOfDay >= 360 && minuteOfDay < 1080);
    if (day) {
        p.cap = 100;
        p.depth = 60;
        return p;
    }
    bool evening = (minuteOfDay >= 1080 && minuteOfDay < 1320) || (minuteOfDay >= 300 && minuteOfDay < 360);
    p.cap = evening ? 70 : 45;
    p.depth = evening ? 45 : 30;
    for (int i = 0; i < 3; ++i) {
        Hsv v = toHsv(p.c[i]);
        if (v.s * 100 < 50 * 255) p.c[i] = AMBER;                                  // whites -> amber
        else if (v.h >= 180 && v.h <= 270) p.c[i] = fromHsv(v.h + 15, v.s, v.v);   // cool hues toward 300
    }
    return p;
}

Palette forLights(const Rgb* cover, int coverCount, uint8_t badge, int minuteOfDay) {
    Palette p = build(cover, coverCount, badge, -1);   // -1: day, the colours unshifted
    const Palette timed = build(cover, coverCount, badge, minuteOfDay);
    p.cap = timed.cap;
    p.depth = timed.depth;
    return p;
}

}  // namespace songpalette
