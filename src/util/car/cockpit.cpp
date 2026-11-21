#include "cockpit.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../common/rgb565.h"
#include "../common/termpal.h"
#include "gaugehud.h"
#include "sukiride.h"

namespace cockpit {

Rect region(Region r) {
    switch (r) {
        case R_SPEED: return {SPD_X, SPD_Y, SPD_CELLS * SPD_CW + (SPD_CELLS - 1) * SPD_GAP, SPD_CH};
        case R_RPM: return {118, 124, 66, 30};   // the number only: the labels left of it never change
        case R_TACH: return {TACH_X, 154, TACH_W + 2, 38};   // the capsules and the scale under them
        case R_TRIP: return {TRIP_X0, 196, 176, 38};
        default: {
            const int i = (int)r - (int)R_ROW0;
            if (i < 0 || i >= ROWS) return {0, 0, 0, 0};
            return {ROW_X, (int16_t)rowY(i), ROW_W, ROW_H};
        }
    }
}

int pillsLit(float frac) {
    if (!std::isfinite(frac)) return 0;
    const float f = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
    return (int)std::lround(f * PILLS);
}

int tachLit(float rpm) {
    if (!(rpm > 0.0f)) return 0;   // NAN too
    const int n = (int)std::ceil(rpm * (float)TACH_N / 7000.0f);
    return n > TACH_N ? TACH_N : n;
}

Zone tachZone(int i) {
    const float top = (float)(i + 1) * 7000.0f / (float)TACH_N;   // the rpm at the capsule's upper edge
    return top > 6500.0f ? Zone::Alert : top > 5000.0f ? Zone::Warn : Zone::Norm;
}

namespace {
float clamp01(float f) { return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f; }

// The first pill of a bar's danger part: the pills at and after `from` (a fraction of the track) are red.
uint8_t hotFrom(float from) { return (uint8_t)std::ceil(from * (float)PILLS - 1e-4f); }

// v with `decimals`, then a suffix; "--" when v is unknown (gaugehud::text), no suffix then.
void value(float v, int decimals, const char* suffix, char* out, size_t n) {
    gaugehud::text(v, decimals, out, n);
    if (!std::isfinite(v) || !n || (out[0] == '-' && out[1] == '-')) return;   // no unit after a "--" fallback
    const size_t len = strlen(out), add = strlen(suffix);
    if (len + add < n) memcpy(out + len, suffix, add + 1);
}

void fill(Card* c, const char* label, float v, int decimals, const char* suffix, float frac, float from, bool bad) {
    snprintf(c->label, sizeof c->label, "%s", label);
    value(v, decimals, suffix, c->val, sizeof c->val);
    c->lit = std::isfinite(v) ? (uint8_t)pillsLit(frac) : 0;
    c->hot = from < 0.0f ? (uint8_t)PILLS : hotFrom(from);
    c->bad = bad;
}
}  // namespace

void scene(const obdwire::Live& l, bool coolBad, bool battBad, int storedCodes, bool mil, Scene* out) {
    if (!out) return;
    Scene s;
    const bool live = l.link == obdplan::Link::Live;
    s.live = live;
    const float nan = NAN;
    const float rpm = live ? l.rpm : nan, kmh = live ? l.kmh : nan;
    if (std::isfinite(kmh)) gaugehud::speedCells(kmh, s.speed);   // else "---"
    if (std::isfinite(rpm)) snprintf(s.rpm, sizeof s.rpm, "%ld", std::lround(rpm < 0.0f ? 0.0f : rpm > 9999.0f ? 9999.0f : rpm));
    s.tachLit = (uint8_t)tachLit(rpm);
    s.rpmZone = rpm >= 6500.0f ? Zone::Alert : rpm >= 5000.0f ? Zone::Warn : Zone::Norm;
    // COOL, BATT, KM/L, PEDAL, LOAD. The battery is the adapter's own reading: shown with or without the link.
    fill(&s.card[COOL], "COOL", live ? l.coolC : nan, 0, "\xB0", sukiride::coolFrac(live ? l.coolC : nan), 0.64f, live && coolBad);
    fill(&s.card[BATT], "BATT", l.volts, 1, "V", sukiride::battFrac(l.volts), -1.0f, live && battBad);
    // KM/L is the trip's average, not the instant one (the car's own dash shows that); like the trip, it stays without the link
    fill(&s.card[KMPL], "KM/L", l.tripKmL, 1, "", clamp01(l.tripKmL / 25.0f), -1.0f, false);
    fill(&s.card[PEDAL], "PEDAL", live ? l.throttle : nan, 0, "%", sukiride::pedalFrac(live ? l.throttle : nan), 0.85f, false);
    fill(&s.card[LOAD], "LOAD", live ? l.load : nan, 0, "%", clamp01((live ? l.load : nan) / 100.0f), 0.85f, false);
    gaugehud::text(std::isfinite(l.tripKm) ? std::fmin(l.tripKm, 999.9f) : l.tripKm, 1, s.trip, sizeof s.trip);   // stays shown without the link; 999.9 at most
    if (std::isfinite(l.rangeKm) && l.rangeKm >= 0.0f) snprintf(s.range, sizeof s.range, "%ld", std::lround(std::fmin(9999.0f, l.rangeKm)));
    if (live && (mil || storedCodes > 0)) {
        s.status = Status::Chk;
        s.codes = (uint8_t)(storedCodes < 0 ? 0 : storedCodes > 99 ? 99 : storedCodes);
    } else if (l.link == obdplan::Link::NoEngine) {
        s.status = Status::KeyOn;
    } else if (!live) {
        s.status = Status::NoLink;
    }
    *out = s;
}

namespace {
struct Hash {
    uint32_t h = 2166136261u;
    void bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
    }
    void str(const char* s) { bytes(s, strlen(s) + 1); }
    void num(uint32_t v) { bytes(&v, sizeof v); }
};
}  // namespace

uint32_t key(const Scene& s, Region r) {
    Hash h;
    switch (r) {
        case R_SPEED:
            h.str(s.speed);
            h.num((uint32_t)s.status);
            h.num(s.status == Status::Chk ? s.codes : 0);
            break;
        case R_RPM: h.str(s.rpm); h.num((uint32_t)s.rpmZone); break;
        case R_TACH: h.num(s.tachLit); break;
        case R_TRIP: h.str(s.trip); h.str(s.range); break;
        default: {
            const int i = (int)r - (int)R_ROW0;
            if (i < 0 || i >= ROWS) break;
            const Card& c = s.card[i];
            h.str(c.val);
            h.num(c.lit);
            h.num(c.hot);
            h.num(c.bad);
            break;
        }
    }
    return h.h;
}

// ---- colours ----
namespace {
uint32_t scale(uint32_t c, float k, float toWhite) {   // x k toward black, then toWhite toward white
    auto ch = [&](int sh) {
        const float v = ((c >> sh) & 255) * k;
        return (uint32_t)std::lround(std::fmin(255.0f, v + (255.0f - v) * toWhite)) << sh;
    };
    return ch(16) | ch(8) | ch(0);
}
}  // namespace

void makePalette(int theme, Palette* out) {
    if (!out) return;
    // exactly the yellow deck's pal(): termpal's night palette for the theme, always, on termpal's BG
    const uint32_t bg = termpal::rgb(termpal::BG, theme, true), tx = termpal::rgb(termpal::TEXT, theme, true),
                   md = termpal::rgb(termpal::MID, theme, true), dm = termpal::rgb(termpal::DIM, theme, true),
                   al = termpal::rgb(termpal::ALERT, theme, true);
    using rgb565::hex;
    out->bg = hex(bg);
    out->tx = hex(tx);
    out->md = hex(md);
    out->dm = hex(dm);
    out->al = hex(al);
    out->ghost = hex(scale(dm, 0.25f, 0));
    out->warn = hex(scale(tx, 1.0f, 0.55f));
    out->core = hex(scale(tx, 1.0f, 0.85f));
    out->border = hex(scale(dm, 0.65f, 0));
}
}  // namespace cockpit
