#include "tripticker.h"
#include <cmath>
#include <cstdio>
#include "gaugehud.h"

namespace tripticker {
namespace {
void dashes(char out[TEXT]) { snprintf(out, TEXT, "--"); }

// One decimal under 100 ("23.4"), whole from there ("123"), never over 99999.
void tenths(float v, char out[TEXT]) {
    if (!std::isfinite(v) || !(v >= 0.0f)) return dashes(out);
    const float r = std::round(v * 10.0f) / 10.0f + 0.0f;   // + 0: -0 prints as 0
    if (r < 100.0f) snprintf(out, TEXT, "%.1f", (double)r);
    else snprintf(out, TEXT, "%ld", (long)std::fmin(99999.0f, std::round(v)));
}

void kml(float v, char out[TEXT]) {
    if (!std::isfinite(v) || !(v >= 0.0f)) return dashes(out);
    snprintf(out, TEXT, "%.1f", (double)(std::fmin(v, 99.9f) + 0.0f));
}
}  // namespace

const char* label(Cell c) {
    switch (c) {
        case Cell::KmlNow: return "KM/L NOW";
        case Cell::TripKml: return "TRIP KM/L";
        case Cell::TripKm: return "TRIP KM";
        case Cell::Range: return "RANGE KM";
        case Cell::TripTime: return "TRIP TIME";
        case Cell::TripRs: return "TRIP R$";
        default: return "";
    }
}

void text(Cell c, const obdwire::Live& l, char out[TEXT]) {
    switch (c) {
        case Cell::KmlNow:
            if (l.link != obdplan::Link::Live) return dashes(out);
            if (l.kml > COAST_KML) {
                snprintf(out, TEXT, "COAST");
                return;
            }
            return kml(l.kml, out);
        case Cell::TripKml: return kml(l.tripKmL, out);
        case Cell::TripKm: return tenths(l.tripKm, out);
        case Cell::Range:
            if (!std::isfinite(l.rangeKm) || !(l.rangeKm >= 0.0f)) return dashes(out);
            snprintf(out, TEXT, "%ld", (long)std::fmin(9999.0f, std::round(l.rangeKm)));
            return;
        case Cell::TripTime: {
            uint32_t h = l.tripS / 3600, m = l.tripS / 60 % 60;
            if (h > 99) h = 99, m = 59;
            snprintf(out, TEXT, "%lu:%02lu", (unsigned long)h, (unsigned long)m);
            return;
        }
        case Cell::TripRs: return tenths(l.tripRs, out);
        default: return dashes(out);
    }
}

bool alert(Cell c, const obdwire::Live& l) {
    return c == Cell::Range && l.rangeKm >= 0.0f && l.rangeKm < LOW_RANGE_KM;   // NAN: false
}

void header(const obdwire::Live& l, char* out, size_t n) {
    uint32_t min = l.tripS / 60;
    if (min > 999) min = 999;
    snprintf(out, n, "> TRIP \xB7 %02lu MIN", (unsigned long)min);
}

const char* status(obdplan::Link link) {
    return link == obdplan::Link::Live ? "OK" : gaugehud::linkTag(link);
}
}  // namespace tripticker
