#include "songpalette.h"
#include "hue.h"
#include "rgb565.h"

namespace songpalette {
namespace {

struct Hsv { int h, s, v; };   // h 0..359, s and v 0..255

Hsv toHsv(Rgb c) {
    int mx = c.r;
    if (c.g > mx) mx = c.g;
    if (c.b > mx) mx = c.b;
    Hsv o{0, 0, mx};
    hue::hueSat(c.r, c.g, c.b, 360, 255, &o.h, &o.s);
    return o;
}

Rgb fromHue(int h) {   // full saturation and value
    h = ((h % 360) + 360) % 360;
    const int f = (h % 60) * 255 / 60, up = f, down = 255 - f;
    switch (h / 60) {
        case 0: return {255, (uint8_t)up, 0};
        case 1: return {(uint8_t)down, 255, 0};
        case 2: return {0, 255, (uint8_t)up};
        case 3: return {0, (uint8_t)down, 255};
        case 4: return {(uint8_t)up, 0, 255};
        default: return {255, 0, (uint8_t)down};
    }
}

constexpr int BUCKETS = 24;           // 15 deg each
constexpr int MIN_SHARE_PCT = 3;      // a hue must cover this much of the picture to count

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
            if (hue::dist(hue, hues[k], 360) < 30) apart = false;
        if (!apart) continue;
        hues[picked] = hue;
        out[picked] = fromHue(hue);
        ++picked;
    }
    return picked;
}

void timeOfDay(int minuteOfDay, uint8_t* cap, uint8_t* depth) {
    const bool day = minuteOfDay < 0 || (minuteOfDay >= 360 && minuteOfDay < 1080);
    const bool evening = (minuteOfDay >= 1080 && minuteOfDay < 1320) || (minuteOfDay >= 300 && minuteOfDay < 360);
    *cap = day ? 100 : evening ? 70 : 45;
    *depth = day ? 60 : evening ? 45 : 30;
}

}  // namespace songpalette
