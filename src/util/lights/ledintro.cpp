#include "ledintro.h"
#include "../yellow/elkcmd.h"

namespace ledintro {
namespace {
songpalette::Rgb at(songpalette::Rgb s, int pct) {
    const uint8_t p = static_cast<uint8_t>(pct < 0 ? 0 : pct > 100 ? 100 : pct);
    return {elkcmd::dim(s.r, p), elkcmd::dim(s.g, p), elkcmd::dim(s.b, p)};
}

// from + (to - from) x smoothstep(k / n), rounded: slow out of the dark, slow into the peak.
int ease(int from, int to, int k, int n) {
    const long t3 = 3L * k * k * n - 2L * k * k * k;  // smoothstep(k / n) x n^3
    const long n3 = static_cast<long>(n) * n * n;
    const long d = static_cast<long>(to - from) * t3;
    return from + static_cast<int>((d + (d >= 0 ? n3 / 2 : -n3 / 2)) / n3);
}

void add(Track& t, uint32_t ms, songpalette::Rgb c) {
    if (t.n < MAX_KEYS) t.k[t.n++] = {ms, c};
}
}  // namespace

int peakPct(int pct) {
    const int p = pct + PEAK_LIFT;
    return p > 100 ? 100 : p;
}

Track plan(bool inside, songpalette::Rgb swatch, int pct) {
    Track t;
    const int peak = peakPct(pct);
    if (inside) {
        // dark, then 5 steps up to the peak (1.5 s), 3 down onto the look (0.9 s), the look again
        constexpr int RISE = 5, SETTLE = 3;
        uint32_t ms = 0;
        for (int k = 0; k <= RISE; k++, ms += IN_GAP_MS) add(t, ms, at(swatch, ease(0, peak, k, RISE)));
        for (int k = 1; k <= SETTLE; k++, ms += IN_GAP_MS) add(t, ms, at(swatch, ease(peak, pct, k, SETTLE)));
        add(t, ms, at(swatch, pct));
    } else {
        // dark while the inside light rises, then half the peak, the peak, the look, the look again
        add(t, 0, at(swatch, 0));
        add(t, OUT_DELAY_MS, at(swatch, peak / 2));
        add(t, OUT_DELAY_MS + OUT_GAP_MS, at(swatch, peak));
        add(t, OUT_DELAY_MS + 2 * OUT_GAP_MS, at(swatch, pct));
        add(t, OUT_DELAY_MS + 3 * OUT_GAP_MS, at(swatch, pct));
    }
    return t;
}

uint32_t length(const Track& t) { return t.n ? t.k[t.n - 1].at : 0; }
}  // namespace ledintro
