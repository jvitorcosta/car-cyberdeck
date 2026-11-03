#include "ledintro.h"
#include "../yellow/elkcmd.h"

namespace ledintro {
namespace {
songpalette::Rgb at(songpalette::Rgb s, int pct) {
    const uint8_t p = static_cast<uint8_t>(pct < 0 ? 0 : pct > 100 ? 100 : pct);
    return {elkcmd::dim(s.r, p), elkcmd::dim(s.g, p), elkcmd::dim(s.b, p)};
}

void add(Track& t, uint32_t ms, songpalette::Rgb c, bool fill = false) {
    if (t.n < MAX_KEYS) t.k[t.n++] = {ms, c, fill};
}
}  // namespace

int peakPct(int pct) {
    const int p = pct + PEAK_LIFT;
    return p > 100 ? 100 : p;
}

Track plan(bool inside, songpalette::Rgb swatch, int pct) {
    Track t;
    const songpalette::Rgb look = at(swatch, pct);
    if (inside) {
        // dark, the curtain fill, then the look twice once it has filled
        add(t, 0, at(swatch, 0));
        add(t, IN_GAP_MS, look, true);
        add(t, IN_GAP_MS + FILL_MS, look);
        add(t, IN_GAP_MS + FILL_MS + IN_GAP_MS, look);
    } else {
        // dark while the inside light fills, then half the peak, the peak, the look, the look again
        const int peak = peakPct(pct);
        add(t, 0, at(swatch, 0));
        add(t, OUT_DELAY_MS, at(swatch, peak / 2));
        add(t, OUT_DELAY_MS + OUT_GAP_MS, at(swatch, peak));
        add(t, OUT_DELAY_MS + 2 * OUT_GAP_MS, look);
        add(t, OUT_DELAY_MS + 3 * OUT_GAP_MS, look);
    }
    return t;
}

uint32_t length(const Track& t) { return t.n ? t.k[t.n - 1].at : 0; }
}  // namespace ledintro
