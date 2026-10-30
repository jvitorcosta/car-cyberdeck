#include "benchdrive.h"
#include <cmath>

namespace benchdrive {
namespace {
struct Point {
    uint16_t s, kmh;
};
// The speed through a cycle, straight lines between the points.
constexpr Point ROUTE[] = {{0, 0},    {40, 0},   {70, 50}, {150, 50}, {170, 0},  {200, 0},  {230, 70}, {330, 70},
                           {350, 30}, {400, 30}, {420, 0}, {450, 0},  {480, 40}, {520, 40}, {540, 0},  {600, 0}};
constexpr int POINTS = sizeof(ROUTE) / sizeof(ROUTE[0]);

float slopeAt(float s) {
    for (int i = 1; i < POINTS; i++)
        if (s < ROUTE[i].s) return (float(ROUTE[i].kmh) - float(ROUTE[i - 1].kmh)) / float(ROUTE[i].s - ROUTE[i - 1].s);
    return 0.0f;
}

float speedAt(float s) {
    for (int i = 1; i < POINTS; i++)
        if (s < ROUTE[i].s) return float(ROUTE[i - 1].kmh) + slopeAt(s) * (s - float(ROUTE[i - 1].s));
    return 0.0f;
}
}  // namespace

Sample at(uint32_t ms) {
    Sample r{ms < CONNECT_MS ? obdplan::Link::Connecting : ms < LIVE_MS ? obdplan::Link::NoEngine : obdplan::Link::Live,
             NAN, NAN, NAN, NAN, NAN, NAN, NAN, false, 0};
    if (r.link != obdplan::Link::Live) return r;
    const uint32_t cycleMs = ms % CYCLE_MS;
    const float s = float(cycleMs) / 1000.0f;
    const uint32_t whole = cycleMs / 1000;
    r.kmh = speedAt(s);
    const float slope = slopeAt(s);
    r.rpm = r.kmh < 1.0f ? 800.0f : 900.0f + r.kmh * 36.0f;
    r.mafGs = slope < 0.0f ? 1.5f : 3.0f + r.kmh * 0.1f + (slope > 0.0f ? 6.0f : 0.0f);
    r.volts = whole >= LOWV_FROM_S && whole < LOWV_TO_S ? LOW_V : 14.1f;
    const float warm = 40.0f + float(ms) / 6000.0f;   // 90 C after 5 min
    r.coolantC = whole >= HOT_FROM_S && whole < HOT_TO_S ? HOT_C : warm < 90.0f ? warm : 90.0f;
    r.trimS = 1.5f;
    r.trimL = 3.0f;
    r.mil = whole >= CODE_FROM_S;
    r.stored = r.mil ? 1 : 0;
    return r;
}
}  // namespace benchdrive
