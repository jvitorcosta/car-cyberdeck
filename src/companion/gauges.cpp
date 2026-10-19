#include "gauges.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "../util/obdplan.h"
#include "../util/rgb565.h"

#ifndef OBD_REDLINE
#define OBD_REDLINE 6500        // the Honda City 1.5 L's; the tach's full scale
#endif

namespace gauges {
namespace {
using obd::Pid;
using rgb565::hex;
// Fallout-terminal green on black (spec 2026-10-19-gauge-cluster): values, labels/frames/stale,
// empty tracks.
constexpr uint16_t HI = hex(0x41FF6A), MID = hex(0x1D8A3A), DARK = hex(0x0E4A1F), BLACK = 0x0000;
// The tach: a 16 px band over the top. TFT_eSPI arc angles start at the bottom and run clockwise,
// so 90..270 sweeps left to right over the top.
constexpr int CX = 160, CY = 126, R_OUT = 102, R_IN = 86, A0 = 90, A1 = 270;
constexpr float WARN_FROM = 0.85f;                // the top zone, as obdplan::rpmWarn
struct Box { int x, y, w, h; };
constexpr Box SPEED{108, 60, 104, 52}, SUB{90, 112, 140, 14}, TEMP{254, 14, 62, 26};
constexpr Box FUEL{4, 138, 152, 56}, BATT{164, 138, 152, 56};
constexpr int BAR_X = 62, BAR_W = 220, BAR_H = 10, PEDAL_Y = 203, THROTTLE_Y = 221;

enum Field { F_SPEED, F_SUB, F_TEMP, F_FUELPCT, F_KML, F_FUELLINE, F_VOLTS, F_CHARGING, F_BATTLINE,
             F_PEDAL, F_THROTTLE, F_ARC, F_COUNT };
char s_shown[F_COUNT][48];                // what each field last drew (style char + text)
bool s_full = true;

bool fresh(const obdlink::View& v, Pid p, uint32_t now) {
    return v.have[(int)p] && now - v.atMs[(int)p] < obdplan::staleAfterMs(p);
}
float val(const obdlink::View& v, Pid p) { return v.have[(int)p] ? v.value[(int)p] : NAN; }

void num(char* out, size_t n, float x, int decimals) {
    if (std::isnan(x)) snprintf(out, n, "--");
    else snprintf(out, n, "%.*f", decimals, (double)x);
}

// True (and remembered) when field f's text or look changed. style: 'n' value, 's' stale, 'w'
// warning (inverted), 'o' off (nothing drawn).
bool changed(Field f, const char* text, char style) {
    char key[48];
    snprintf(key, sizeof key, "%c%s", style, text);
    if (strcmp(key, s_shown[f]) == 0) return false;
    strlcpy(s_shown[f], key, sizeof s_shown[f]);
    return true;
}

// Clears box b and draws s in it (datum TL/TC/TR at the box's top), in style's colours.
void put(TFT_eSPI& t, const Box& b, const char* s, char style, uint8_t datum, int font, int size = 1) {
    const uint16_t bg = style == 'w' ? HI : BLACK;
    t.fillRect(b.x, b.y, b.w, b.h, bg);
    if (style == 'o') return;
    t.setTextColor(style == 'w' ? BLACK : style == 's' ? MID : HI, bg);
    t.setTextDatum(datum);
    t.setTextSize(size);
    const int x = datum == TR_DATUM ? b.x + b.w - 1 : datum == TC_DATUM ? b.x + b.w / 2 : b.x + 1;
    t.drawString(s, x, b.y + 1, font);
    t.setTextSize(1);
}

void label(TFT_eSPI& t, const char* s, int x, int y) {
    t.setTextColor(MID, BLACK);
    t.setTextDatum(TL_DATUM);
    t.drawString(s, x, y, 1);
}

void arc(TFT_eSPI& t, float rpm, bool stale) {
    const float f = std::isnan(rpm) ? 0.0f : obdplan::frac(rpm, 0.0f, OBD_REDLINE);
    const int a = A0 + (int)(f * (A1 - A0) + 0.5f), w = A0 + (int)(WARN_FROM * (A1 - A0));
    char key[16];
    snprintf(key, sizeof key, "%d", a);
    if (!changed(F_ARC, key, stale ? 's' : 'n')) return;
    if (a > A0) t.drawArc(CX, CY, R_OUT, R_IN, A0, a, stale ? MID : HI, BLACK, false);
    if (a < w) t.drawArc(CX, CY, R_OUT, R_IN, a, w, DARK, BLACK, false);
    const int z = a > w ? a : w;                  // the top zone, still empty: dim, not dark
    if (z < A1) t.drawArc(CX, CY, R_OUT, R_IN, z, A1, MID, BLACK, false);
}

void bar(TFT_eSPI& t, Field f, int y, float pct, bool stale) {
    char s[8];
    num(s, sizeof s, pct, 0);
    if (!changed(f, s, stale ? 's' : 'n')) return;
    const int fill = std::isnan(pct) ? 0 : (int)(obdplan::frac(pct, 0.0f, 100.0f) * BAR_W + 0.5f);
    t.fillRect(BAR_X, y, fill, BAR_H, stale ? MID : HI);
    t.fillRect(BAR_X + fill, y, BAR_W - fill, BAR_H, DARK);
    put(t, Box{BAR_X + BAR_W + 4, y, 316 - BAR_X - BAR_W - 4, BAR_H}, s, stale ? 's' : 'n', TR_DATUM, 1);
}

void statics(TFT_eSPI& t) {
    label(t, "TEMP", 272, 4);
    t.drawRect(FUEL.x, FUEL.y, FUEL.w, FUEL.h, MID);
    t.drawRect(BATT.x, BATT.y, BATT.w, BATT.h, MID);
    label(t, "FUEL", FUEL.x + 5, FUEL.y + 4);
    label(t, "KM/L", FUEL.x + 92, FUEL.y + 30);
    label(t, "BATTERY", BATT.x + 5, BATT.y + 4);
    label(t, "VOLTS", BATT.x + 80, BATT.y + 30);
    label(t, "PEDAL", 6, PEDAL_Y + 1);
    label(t, "THROTTLE", 6, THROTTLE_Y + 1);
}
}

void invalidate() { s_full = true; }

void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t now) {
    if (s_full) {
        statics(t);
        memset(s_shown, 0, sizeof s_shown);
        s_full = false;
    }
    char s[48], a[12], b[12];
    const float rpm = val(v, Pid::Rpm);
    const bool running = fresh(v, Pid::Rpm, now) && rpm > 0.0f;

    arc(t, rpm, !fresh(v, Pid::Rpm, now));
    num(s, sizeof s, val(v, Pid::Speed), 0);
    const char speedStyle = fresh(v, Pid::Speed, now) ? 'n' : 's';
    if (changed(F_SPEED, s, speedStyle)) put(t, SPEED, s, speedStyle, TC_DATUM, 4, 2);
    num(a, sizeof a, rpm, 0);
    snprintf(s, sizeof s, "KM/H  %s RPM", a);
    const char subStyle = !std::isnan(rpm) && obdplan::rpmWarn(rpm, OBD_REDLINE) ? 'w'
                          : fresh(v, Pid::Rpm, now)                               ? 'n' : 's';
    if (changed(F_SUB, s, subStyle)) put(t, SUB, s, subStyle, TC_DATUM, 2);

    const float c = val(v, Pid::Coolant);
    num(a, sizeof a, c, 0);
    snprintf(s, sizeof s, std::isnan(c) ? "%s" : "%sC", a);
    const char tempStyle = !std::isnan(c) && obdplan::coolantWarn(c) ? 'w' : fresh(v, Pid::Coolant, now) ? 'n' : 's';
    if (changed(F_TEMP, s, tempStyle)) put(t, TEMP, s, tempStyle, TR_DATUM, 4);

    // FUEL: tank %, km/l now (big), average and km left.
    const bool airFresh = fresh(v, Pid::Maf, now) || fresh(v, Pid::Map, now);
    num(a, sizeof a, val(v, Pid::Fuel), 0);
    snprintf(s, sizeof s, std::isnan(val(v, Pid::Fuel)) ? "%s" : "%s%%", a);
    const char pctStyle = fresh(v, Pid::Fuel, now) ? 'n' : 's';
    if (changed(F_FUELPCT, s, pctStyle)) put(t, Box{FUEL.x + 100, FUEL.y + 3, 48, 9}, s, pctStyle, TR_DATUM, 1);
    num(s, sizeof s, v.kmlNow, 1);
    if (changed(F_KML, s, airFresh ? 'n' : 's')) put(t, Box{FUEL.x + 5, FUEL.y + 15, 84, 26}, s, airFresh ? 'n' : 's', TL_DATUM, 4);
    num(a, sizeof a, v.kmlAvg, 1);
    num(b, sizeof b, v.kmLeft, 0);
    snprintf(s, sizeof s, "AVG %s  %s KM LEFT", a, b);
    if (changed(F_FUELLINE, s, 'n')) put(t, Box{FUEL.x + 5, FUEL.y + 44, FUEL.w - 10, 9}, s, 'n', TL_DATUM, 1);

    // BATTERY: volts (big), charging, the dip at engine start.
    const float volts = val(v, Pid::Battery);
    num(s, sizeof s, volts, 1);
    const char voltStyle = !std::isnan(volts) && obdplan::batteryWarn(volts, running) ? 'w'
                           : fresh(v, Pid::Battery, now)                               ? 'n' : 's';
    if (changed(F_VOLTS, s, voltStyle)) put(t, Box{BATT.x + 5, BATT.y + 15, 72, 26}, s, voltStyle, TL_DATUM, 4);
    const bool charging = running && volts >= 13.2f;
    if (changed(F_CHARGING, "CHARGING", charging ? 'w' : 'o'))
        put(t, Box{BATT.x + BATT.w - 56, BATT.y + 3, 52, 9}, "CHARGING", charging ? 'w' : 'o', TC_DATUM, 1);
    num(a, sizeof a, v.atStart, 1);
    snprintf(s, sizeof s, std::isnan(v.atStart) ? "AT START %s" : "AT START %s V", a);
    if (changed(F_BATTLINE, s, 'n')) put(t, Box{BATT.x + 5, BATT.y + 44, BATT.w - 10, 9}, s, 'n', TL_DATUM, 1);

    // Pedal vs throttle, both scaled to their learnt travel.
    bar(t, F_PEDAL, PEDAL_Y, v.pedalPct, !(fresh(v, Pid::PedalD, now) || fresh(v, Pid::PedalE, now)));
    bar(t, F_THROTTLE, THROTTLE_Y, v.throttlePct, !fresh(v, Pid::Throttle, now));
}
}
