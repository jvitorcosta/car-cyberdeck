#include "gauges.h"
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "../ui/battle.h"
#include "../ui/textdraw.h"
#include "../ui/theme.h"
#include "../util/menunav.h"
#include "../util/obdplan.h"

#ifndef OBD_REDLINE
#define OBD_REDLINE 6500        // the Honda City 1.5 L's; the RPM bar's full scale
#endif

namespace gauges {
namespace {
using obd::Pid;
using obdplan::Band;
constexpr uint32_t STALE_MS = 3000;   // older readings are drawn dimmed
constexpr int W = 320;

struct Box { int x, y, w, h; const char* name; };
constexpr Box RPM{6, 4, 308, 58, "RPM"};
constexpr Box SPEED{6, 66, 140, 134, "SPEED"};
constexpr Box SMALL[4] = {{150, 66, 80, 65, "COOLANT"}, {234, 66, 80, 65, "BATTERY"},
                          {150, 135, 80, 65, "LOAD"}, {234, 135, 80, 65, "THROTTLE"}};
constexpr Pid SMALL_PID[4] = {Pid::Coolant, Pid::Battery, Pid::Load, Pid::Throttle};

char s_shown[6][24];       // RPM, speed, then the four small boxes: the text last drawn
bool s_full = true;

uint16_t bandColor(Band b) {
    switch (b) {
    case Band::Cold: return theme::EXP_BLUE;
    case Band::Warn: return theme::HP_YELLOW;
    case Band::Hot: return theme::HP_RED;
    default: return theme::HP_GREEN;
    }
}

void bar(TFT_eSPI& t, int x, int y, int w, int h, float f, uint16_t c) {
    const int fill = (int)(f * (w - 2) + 0.5f);
    t.drawRect(x, y, w, h, theme::pal().boxBorder);
    t.fillRect(x + 1, y + 1, fill, h - 2, c);
    t.fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, theme::HP_EMPTY);
}

bool fresh(const obdlink::View& v, Pid p, uint32_t now) {
    return v.have[(int)p] && now - v.atMs[(int)p] < STALE_MS;
}

// The text a box shows for p ("--" before a reading); the trailing '~' marks it stale.
void text(const obdlink::View& v, Pid p, uint32_t now, char* out, size_t n) {
    if (!v.have[(int)p]) { snprintf(out, n, "--"); return; }
    const float x = v.value[(int)p];
    switch (p) {
    case Pid::Battery: snprintf(out, n, "%.1fV", (double)x); break;
    case Pid::Coolant: snprintf(out, n, "%dC", (int)x); break;
    case Pid::Load:
    case Pid::Throttle: snprintf(out, n, "%d%%", (int)(x + 0.5f)); break;
    default: snprintf(out, n, "%d", (int)(x + 0.5f)); break;
    }
    if (!fresh(v, p, now)) strncat(out, "~", n - strlen(out) - 1);
}

void drawSmall(TFT_eSPI& t, int i, const obdlink::View& v, uint32_t now, const char* s) {
    const Box& b = SMALL[i];
    const Pid p = SMALL_PID[i];
    const theme::Palette& pal = theme::pal();
    t.fillRect(b.x + 4, b.y + 18, b.w - 8, 26, pal.boxFill);
    const uint16_t fg = fresh(v, p, now) ? pal.text : pal.boxShadow;
    ui::drawText(t, s, b.x + b.w / 2, b.y + 31, fg, pal.textShadow, MC_DATUM, b.w - 8);
    const float x = v.have[(int)p] ? v.value[(int)p] : 0.0f;
    float f = 0.0f;
    Band band = Band::Ok;
    if (p == Pid::Coolant) { f = obdplan::frac(x, 40.0f, 120.0f); band = obdplan::coolantBand(x); }
    else if (p == Pid::Battery) { f = obdplan::frac(x, 11.0f, 15.0f); band = obdplan::batteryBand(x); }
    else f = obdplan::frac(x, 0.0f, 100.0f);
    bar(t, b.x + 6, b.y + b.h - 16, b.w - 12, 8, v.have[(int)p] ? f : 0.0f, bandColor(band));
}
}

void invalidate() { s_full = true; }

void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t now) {
    const theme::Palette& pal = theme::pal();
    if (s_full) {
        t.fillRect(0, 0, W, menunav::BAR.y - 2, pal.top);
        ui::battleBox(t, RPM.x, RPM.y, RPM.w, RPM.h, ui::Tab::None);
        ui::battleBox(t, SPEED.x, SPEED.y, SPEED.w, SPEED.h, ui::Tab::None);
        for (const Box& b : SMALL) ui::battleBox(t, b.x, b.y, b.w, b.h, ui::Tab::None);
        ui::drawText(t, RPM.name, RPM.x + 10, RPM.y + 6, pal.text, pal.textShadow, TL_DATUM, 80);
        ui::drawText(t, "km/h", SPEED.x + SPEED.w / 2, SPEED.y + SPEED.h - 18, pal.text,
                     pal.textShadow, MC_DATUM, SPEED.w);
        for (const Box& b : SMALL)
            ui::drawText(t, b.name, b.x + b.w / 2, b.y + 9, pal.text, pal.textShadow, MC_DATUM, b.w - 6);
        memset(s_shown, 0, sizeof s_shown);
        s_full = false;
    }
    char s[24];
    text(v, Pid::Rpm, now, s, sizeof s);
    if (strcmp(s, s_shown[0]) != 0) {
        strlcpy(s_shown[0], s, sizeof s_shown[0]);
        t.fillRect(RPM.x + 200, RPM.y + 4, 100, 20, pal.boxFill);
        ui::drawText(t, s, RPM.x + RPM.w - 10, RPM.y + 6, fresh(v, Pid::Rpm, now) ? pal.text : pal.boxShadow,
                     pal.textShadow, TR_DATUM, 100);
        const float rpm = v.have[(int)Pid::Rpm] ? v.value[(int)Pid::Rpm] : 0.0f;
        bar(t, RPM.x + 10, RPM.y + 30, RPM.w - 20, 16, obdplan::frac(rpm, 0.0f, OBD_REDLINE),
            bandColor(obdplan::rpmBand(rpm, OBD_REDLINE)));
    }
    text(v, Pid::Speed, now, s, sizeof s);
    if (strcmp(s, s_shown[1]) != 0) {
        strlcpy(s_shown[1], s, sizeof s_shown[1]);
        t.fillRect(SPEED.x + 6, SPEED.y + 20, SPEED.w - 12, 70, pal.boxFill);
        t.setTextColor(fresh(v, Pid::Speed, now) ? pal.text : pal.boxShadow, pal.boxFill);
        t.setTextDatum(MC_DATUM);
        t.setTextSize(2);
        t.drawString(s, SPEED.x + SPEED.w / 2, SPEED.y + 55, 4);   // font 4 doubled: ~52 px
        t.setTextSize(1);
    }
    for (int i = 0; i < 4; ++i) {
        text(v, SMALL_PID[i], now, s, sizeof s);
        if (strcmp(s, s_shown[2 + i]) == 0) continue;
        strlcpy(s_shown[2 + i], s, sizeof s_shown[2 + i]);
        drawSmall(t, i, v, now, s);
    }
}
}
