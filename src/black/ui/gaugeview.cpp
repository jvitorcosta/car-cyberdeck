#include "gaugeview.h"
#include <cmath>
#include <cstring>
#include "../../util/car/gaugehud.h"

namespace ui {
namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t BG = rgb(0, 0, 0);
constexpr uint16_t PANEL = rgb(8, 16, 32);       // the header strip
constexpr uint16_t LINE = rgb(20, 70, 110);      // frames and dividers
constexpr uint16_t LABEL = rgb(90, 170, 220);    // small captions
constexpr uint16_t TEXT = rgb(235, 245, 255);
constexpr uint16_t DIM = rgb(70, 80, 95);        // an unknown value, an idle tag
constexpr uint16_t LIT_GREEN = rgb(80, 230, 60), GHOST_GREEN = rgb(14, 40, 12);
constexpr uint16_t LIT_AMBER = rgb(255, 170, 0), GHOST_AMBER = rgb(45, 30, 0);
constexpr uint16_t LIT_RED = rgb(255, 40, 30), GHOST_RED = rgb(50, 10, 8);
constexpr uint16_t PEAK = rgb(240, 240, 255);
constexpr uint16_t SEG_LIT = rgb(255, 190, 40), SEG_GHOST = rgb(34, 24, 6);
constexpr uint16_t WARN_BG = rgb(230, 30, 20);

// the RPM ramp
constexpr int RAMP_X = 6, PITCH = 13, BLOCK_W = 10, SKEW = 4, RAMP_BASE = 80, H_MIN = 8, H_MAX = 56;
constexpr int SCALE_Y = 84;
// the speed: 3 drawn digits
constexpr int SPD_X = 8, SPD_Y = 100, DIG_W = 36, DIG_H = 66, DIG_T = 7, DIG_GAP = 8;
// the right column
constexpr int COL_X = 152, VAL_R = 314, RPM_Y = 98, THR_Y = 132, LOAD_Y = 156;
constexpr int CELL_X = 186, CELL_PITCH = 10, CELL_W = 8, CELL_H = 12, CELLS = 10, PCT_X = 288;
// the tiles
constexpr int TILE_Y = 192, TILE_W = 100, TILE_H = 46;
constexpr int TILE_X[3] = {4, 110, 216};

struct Shown {
    uint16_t block[gaugehud::SEGS];
    char rpm[8], speed[4], thrPct[8], loadPct[8], tile[3][8];
    int thrCells, loadCells;
    bool tileWarn[3], shift;
    obdplan::Link link;
};
Shown s_shown;
gaugehud::Peak s_peak;

void block(TFT_eSPI& tft, int i, uint16_t c) {
    const int x = RAMP_X + i * PITCH, yb = RAMP_BASE, yt = RAMP_BASE - gaugehud::height(i, H_MIN, H_MAX);
    tft.fillTriangle(x, yb, x + BLOCK_W, yb, x + SKEW + BLOCK_W, yt, c);
    tft.fillTriangle(x, yb, x + SKEW + BLOCK_W, yt, x + SKEW, yt, c);
}

uint16_t blockColor(int i, int lit, int peak) {
    const gaugehud::Zone z = gaugehud::zone(i);
    if (i < lit) return z == gaugehud::Zone::Red ? LIT_RED : z == gaugehud::Zone::Amber ? LIT_AMBER : LIT_GREEN;
    if (peak > lit && i == peak - 1) return PEAK;
    return z == gaugehud::Zone::Red ? GHOST_RED : z == gaugehud::Zone::Amber ? GHOST_AMBER : GHOST_GREEN;
}

void digit(TFT_eSPI& tft, int x, int y, char ch) {
    const uint8_t on = gaugehud::segments(ch);
    const int t = DIG_T, w = DIG_W, v = (DIG_H - 3 * t) / 2;
    struct R { uint8_t bit; int x, y, w, h; };
    const R segs[7] = {
        {gaugehud::SEG_A, x + t, y, w - 2 * t, t},
        {gaugehud::SEG_B, x + w - t, y + t, t, v},
        {gaugehud::SEG_C, x + w - t, y + 2 * t + v, t, v},
        {gaugehud::SEG_D, x + t, y + 2 * t + 2 * v, w - 2 * t, t},
        {gaugehud::SEG_E, x, y + 2 * t + v, t, v},
        {gaugehud::SEG_F, x, y + t, t, v},
        {gaugehud::SEG_G, x + t, y + t + v, w - 2 * t, t},
    };
    for (const R& s : segs) tft.fillRect(s.x, s.y, s.w, s.h, (on & s.bit) ? SEG_LIT : SEG_GHOST);
}

// a number right-aligned at (r, y) in font f, over a cleared box of width w
void number(TFT_eSPI& tft, const char* t, int r, int y, int w, int h, int f, uint16_t fg, uint16_t bg) {
    tft.fillRect(r - w, y, w, h, bg);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(t, r, y, f);
}

void bar(TFT_eSPI& tft, int y, int lit) {
    for (int j = 0; j < CELLS; ++j)
        tft.fillRect(CELL_X + j * CELL_PITCH, y + 2, CELL_W, CELL_H, j < lit ? LABEL : SEG_GHOST);
}

void header(TFT_eSPI& tft, bool shift, obdplan::Link link, bool frame) {
    if (frame) {
        tft.fillRect(0, 0, tft.width(), 18, PANEL);
        tft.setTextColor(LABEL, PANEL);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("CITY DATALINK", 6, 1, 2);
        for (int k = 0; k < 3; ++k)   // three slashes after the name
            tft.fillTriangle(108 + k * 6, 14, 112 + k * 6, 14, 116 + k * 6, 3, LINE);
    }
    tft.fillRect(136, 2, 58, 14, shift ? WARN_BG : PANEL);
    tft.drawRect(136, 2, 58, 14, shift ? WARN_BG : LINE);
    tft.setTextColor(shift ? BG : DIM, shift ? WARN_BG : PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SHIFT", 165, 9, 2);
    const uint16_t c = link == obdplan::Link::Live ? LIT_GREEN : link == obdplan::Link::NoEngine ? LIT_AMBER : DIM;
    tft.fillRect(240, 1, 80, 16, PANEL);
    tft.setTextColor(c, PANEL);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(gaugehud::linkTag(link), 314, 1, 2);
}

void frame(TFT_eSPI& tft) {
    tft.fillScreen(BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(DIM, BG);
    for (int k = 0; k <= 7; ++k) {   // the scale under the ramp, x1000 rpm
        const int x = RAMP_X + k * 1000 * gaugehud::SEGS * PITCH / (int)gaugehud::RPM_MAX;
        char t[2] = {(char)('0' + k), 0};
        tft.drawString(t, x < 3 ? 3 : x, SCALE_Y, 1);
    }
    tft.drawFastHLine(0, 94, tft.width(), LINE);
    tft.drawFastVLine(144, 98, 88, LINE);
    tft.setTextColor(LABEL, BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("KM/H", SPD_X + 3 * DIG_W + 2 * DIG_GAP, 170, 2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("RPM", COL_X, RPM_Y + 6, 2);
    tft.drawString("THR", COL_X, THR_Y, 2);
    tft.drawString("LOAD", COL_X, LOAD_Y, 2);
    for (int i = 0; i < 3; ++i) tft.drawRect(TILE_X[i], TILE_Y, TILE_W, TILE_H, LINE);
}

void tile(TFT_eSPI& tft, int i, const char* t, bool warn, bool known) {
    static const char* const UNITS[3] = {"C", "C", "V"};
    const int x = TILE_X[i] + 1, y = TILE_Y + 1, w = TILE_W - 2, h = TILE_H - 2;
    const uint16_t bg = warn ? WARN_BG : BG;
    tft.fillRect(x, y, w, h, bg);
    static const char* const NAMES[3] = {"COOLANT", "INTAKE", "BATTERY"};
    tft.setTextColor(warn ? BG : LABEL, bg);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(NAMES[i], TILE_X[i] + 6, TILE_Y + 3, 2);
    const uint16_t fg = warn ? BG : known ? TEXT : DIM;
    tft.setTextColor(fg, bg);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(UNITS[i], TILE_X[i] + TILE_W - 6, TILE_Y + TILE_H - 6, 2);
    tft.drawString(t, TILE_X[i] + TILE_W - 18, TILE_Y + TILE_H - 3, 4);
}
}  // namespace

void drawGauges(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole) {
    if (whole) {
        frame(tft);
        memset(&s_shown, 0, sizeof s_shown);
        s_shown.thrCells = s_shown.loadCells = -1;
    }
    const bool shift = gaugehud::shiftOn(l.rpm, nowMs);
    if (whole || shift != s_shown.shift || l.link != s_shown.link) header(tft, shift, l.link, whole);
    s_shown.shift = shift;
    s_shown.link = l.link;
    // the ramp, block by block
    const int lit = gaugehud::lit(l.rpm);
    const int peak = s_peak.update(lit, nowMs);
    for (int i = 0; i < gaugehud::SEGS; ++i) {
        const uint16_t c = blockColor(i, lit, peak);
        if (whole || c != s_shown.block[i]) {
            block(tft, i, c);
            s_shown.block[i] = c;
        }
    }
    char t[8];
    gaugehud::text(l.rpm, 0, t, sizeof t);
    if (whole || strcmp(t, s_shown.rpm) != 0) {
        number(tft, t, VAL_R, RPM_Y, 110, 26, 4, std::isfinite(l.rpm) ? TEXT : DIM, BG);
        strlcpy(s_shown.rpm, t, sizeof s_shown.rpm);
    }
    // the speed: only the digits that changed
    char sp[4];
    gaugehud::speedCells(l.kmh, sp);
    for (int k = 0; k < 3; ++k)
        if (whole || sp[k] != s_shown.speed[k]) digit(tft, SPD_X + k * (DIG_W + DIG_GAP), SPD_Y, sp[k]);
    memcpy(s_shown.speed, sp, sizeof sp);
    // the bars
    struct { float v; int y; int* cells; char* pct; } bars[2] = {
        {l.throttle, THR_Y, &s_shown.thrCells, s_shown.thrPct},
        {l.load, LOAD_Y, &s_shown.loadCells, s_shown.loadPct},
    };
    for (auto& b : bars) {
        const int c = gaugehud::cells(b.v, CELLS);
        if (whole || c != *b.cells) {
            bar(tft, b.y, c);
            *b.cells = c;
        }
        gaugehud::text(b.v, 0, t, sizeof t);
        if (std::isfinite(b.v)) strlcat(t, "%", sizeof t);
        if (whole || strcmp(t, b.pct) != 0) {
            number(tft, t, VAL_R + 4, b.y, VAL_R + 4 - PCT_X, 16, 2, std::isfinite(b.v) ? TEXT : DIM, BG);
            strlcpy(b.pct, t, 8);
        }
    }
    // the tiles
    const float vals[3] = {l.coolC, l.intakeC, l.volts};
    const bool warn[3] = {gaugehud::coolWarn(l.coolC), false, gaugehud::battWarn(l.volts, l.rpm)};
    for (int i = 0; i < 3; ++i) {
        gaugehud::text(vals[i], i == 2 ? 1 : 0, t, sizeof t);
        if (whole || warn[i] != s_shown.tileWarn[i] || strcmp(t, s_shown.tile[i]) != 0) {
            tile(tft, i, t, warn[i], std::isfinite(vals[i]));
            s_shown.tileWarn[i] = warn[i];
            strlcpy(s_shown.tile[i], t, sizeof s_shown.tile[i]);
        }
    }
}
}  // namespace ui
