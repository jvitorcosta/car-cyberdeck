#include "gaugeview.h"
#include <cmath>
#include <cstring>
#include "../../util/car/gaugehud.h"
#include "../../util/car/streetface.h"
#include "../../util/common/rgb565.h"
#include "pink.h"
#include "clocktext.h"

// CITY DATALINK, street look: the RPM ramp in neon, pink 7-segment speed, three chrome pills for the
// slow values, and the street panel on the right: the navigator (her mood follows the engine), the
// stripes behind her running with the speed, and the last 12 s of RPM as a trace. The panel is drawn
// in a sprite (PSRAM) and pushed whole; everything else is redrawn only where it changed.
namespace ui {
namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t BG = rgb(10, 4, 22);
constexpr uint16_t PANEL = rgb(26, 8, 44);        // the header strip
constexpr uint16_t LINE = rgb(110, 60, 255);      // frames and dividers (violet)
constexpr uint16_t PINK = rgb565::hex(PINK_RGB), GHOST_PINK = rgb(52, 8, 36);
constexpr uint16_t CYAN = rgb(47, 243, 255), GHOST_CYAN = rgb(8, 40, 48);
constexpr uint16_t RED = rgb(255, 40, 40), GHOST_RED = rgb(52, 8, 8);
constexpr uint16_t TEXT = rgb(245, 245, 255);
constexpr uint16_t DIM = rgb(84, 74, 106);        // an unknown value, an idle tag
constexpr uint16_t PEAK = rgb(255, 255, 255);
constexpr uint16_t WARN_BG = rgb(230, 30, 40);
// the chrome pills
constexpr uint16_t CHROME = rgb(176, 176, 192), CHROME_HI = rgb(236, 236, 246), CHROME_LO = rgb(112, 112, 132);
constexpr uint16_t PILL_VALUE = rgb(40, 70, 235), PILL_LABEL = rgb(210, 16, 40);
// the street panel
constexpr uint16_t ST_RED = rgb(196, 14, 48), ST_STRIPE = rgb(236, 52, 112);
constexpr uint16_t WAVE_BG = rgb(60, 0, 22), WAVE_BAR = rgb(255, 200, 230);
constexpr uint16_t HAIR = rgb(255, 64, 150), HAIR_HI = rgb(255, 150, 200), HAIR_DK = rgb(150, 10, 70);
constexpr uint16_t SKIN = rgb(255, 226, 208), BLUSH = rgb(255, 140, 160), INK = rgb(20, 4, 20);
constexpr uint16_t IRIS = rgb(120, 40, 220), WHITE = rgb(255, 255, 255), GEAR = rgb(40, 34, 56);

// the header
constexpr int SHIFT_X = 124, SHIFT_W = 48, CLOCK_R = 216;
// the RPM ramp
constexpr int RAMP_X = 6, PITCH = 13, BLOCK_W = 10, SKEW = 4, RAMP_BASE = 80, H_MIN = 8, H_MAX = 56;
constexpr int SCALE_Y = 84;
// the speed: 3 drawn digits
constexpr int SPD_X = 4, SPD_Y = 98, DIG_W = 28, DIG_H = 50, DIG_T = 6, DIG_GAP = 6;
// next to it
constexpr int RPM_R = 176, RPM_Y = 106;
// throttle and load
constexpr int BAR_Y = 153, THR_X = 22, LOAD_X = 108, CELL_PITCH = 7, CELL_W = 5, CELL_H = 8, CELLS = 10;
// the pills, stepping right as they go down
constexpr int PILL_Y = 162, PILL_H = 24, PILL_PITCH = 26, PILL_X = 4, PILL_STEP = 7, PILL_R = 172;
// the street panel
constexpr int ST_X = 180, ST_Y = 96, ST_W = 140, ST_H = 144;

struct Shown {
    uint16_t block[gaugehud::SEGS];
    char rpm[8], speed[4], pill[3][8], clock[6];
    int thrCells, loadCells;
    bool pillWarn[3], pillKnown[3], shift;
    obdplan::Link link;
};
Shown s_shown;
gaugehud::Peak s_peak;
streetface::Stripes s_stripes;
streetface::Trace s_trace;
TFT_eSprite* s_panel = nullptr;   // the street panel; null: no memory, the panel stays plain

void block(TFT_eSPI& tft, int i, uint16_t c) {
    const int x = RAMP_X + i * PITCH, yb = RAMP_BASE, yt = RAMP_BASE - gaugehud::height(i, H_MIN, H_MAX);
    tft.fillTriangle(x, yb, x + BLOCK_W, yb, x + SKEW + BLOCK_W, yt, c);
    tft.fillTriangle(x, yb, x + SKEW + BLOCK_W, yt, x + SKEW, yt, c);
}

uint16_t blockColor(int i, int lit, int peak) {
    const gaugehud::Zone z = gaugehud::zone(i);
    if (i < lit) return z == gaugehud::Zone::Red ? RED : z == gaugehud::Zone::Amber ? PINK : CYAN;
    if (peak > lit && i == peak - 1) return PEAK;
    return z == gaugehud::Zone::Red ? GHOST_RED : z == gaugehud::Zone::Amber ? GHOST_PINK : GHOST_CYAN;
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
    for (const R& s : segs) tft.fillRect(s.x, s.y, s.w, s.h, (on & s.bit) ? PINK : GHOST_PINK);
}

// a number right-aligned at (r, y) in font f, over a cleared box of width w
void number(TFT_eSPI& tft, const char* t, int r, int y, int w, int h, int f, uint16_t fg, uint16_t bg) {
    tft.fillRect(r - w, y, w, h, bg);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(t, r, y, f);
}

void cellsRow(TFT_eSPI& tft, int x, int lit, uint16_t on, uint16_t off) {
    for (int j = 0; j < CELLS; ++j) tft.fillRect(x + j * CELL_PITCH, BAR_Y, CELL_W, CELL_H, j < lit ? on : off);
}

void header(TFT_eSPI& tft, bool shift, obdplan::Link link, const char* clock, bool frame) {
    if (frame) {
        tft.fillRect(0, 0, tft.width(), 18, PANEL);
        tft.setFreeFont(&FreeSansBoldOblique9pt7b);
        tft.setTextColor(PINK);
        tft.setTextDatum(BL_DATUM);
        tft.drawString("CITY", 5, 16);
        const int w = tft.textWidth("CITY");
        tft.setFreeFont(nullptr);
        tft.setTextColor(CYAN, PANEL);
        tft.setTextDatum(BL_DATUM);
        tft.drawString("DATALINK", 9 + w, 15, 1);
        const int sx = 9 + w + tft.textWidth("DATALINK", 1) + 4;
        for (int k = 0; k < 3; ++k)   // three slashes after the name
            tft.fillTriangle(sx + k * 5, 14, sx + 3 + k * 5, 14, sx + 7 + k * 5, 4, PINK);
    }
    tft.fillRect(SHIFT_X, 2, SHIFT_W, 14, shift ? PINK : PANEL);
    tft.drawRect(SHIFT_X, 2, SHIFT_W, 14, shift ? PINK : LINE);
    tft.setTextColor(shift ? BG : DIM, shift ? PINK : PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SHIFT", SHIFT_X + SHIFT_W / 2, 9, 2);
    tft.fillRect(CLOCK_R - 40, 1, 40, 16, PANEL);
    tft.setTextColor(CYAN, PANEL);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(clock, CLOCK_R, 1, 2);
    const uint16_t c = link == obdplan::Link::Live ? CYAN : link == obdplan::Link::NoEngine ? PINK : DIM;
    tft.fillRect(222, 1, 98, 16, PANEL);
    tft.setTextColor(c, PANEL);
    tft.drawString(gaugehud::linkTag(link), 314, 1, 2);
}

void frame(TFT_eSPI& tft) {
    tft.fillScreen(BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(LINE, BG);
    for (int k = 0; k <= 7; ++k) {   // the scale under the ramp, x1000 rpm
        const int x = RAMP_X + k * 1000 * gaugehud::SEGS * PITCH / (int)gaugehud::RPM_MAX;
        char t[2] = {(char)('0' + k), 0};
        tft.drawString(t, x < 3 ? 3 : x, SCALE_Y, 1);
    }
    tft.drawFastHLine(0, 93, tft.width(), LINE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(CYAN, BG);
    tft.drawString("RPM", 106, 98, 1);
    tft.drawString("KM/H", 106, 134, 2);
    tft.setTextColor(PINK, BG);
    tft.drawString("THR", 4, BAR_Y, 1);
    tft.setTextColor(CYAN, BG);
    tft.drawString("LD", 92, BAR_Y, 1);
}

void pill(TFT_eSPI& tft, int i, const char* t, bool warn, bool known) {
    static const char* const NAMES[3] = {"COOLANT", "INTAKE", "BATTERY"};
    static const char* const UNITS[3] = {"C", "C", "V"};
    const int x = PILL_X + i * PILL_STEP, y = PILL_Y + i * PILL_PITCH, w = PILL_R - x;
    tft.fillRect(0, y, ST_X - 2, PILL_H, BG);
    tft.fillRoundRect(x, y, w, PILL_H, PILL_H / 2, warn ? WARN_BG : CHROME);
    tft.fillRoundRect(x + 4, y + 2, w - 8, 9, 4, warn ? rgb(255, 110, 120) : CHROME_HI);
    tft.drawFastHLine(x + 10, y + PILL_H - 3, w - 20, warn ? rgb(150, 10, 20) : CHROME_LO);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(warn ? WHITE : known ? PILL_VALUE : CHROME_LO);
    tft.drawString(t, x + 12, y + PILL_H / 2 + 1, 4);
    const int vw = tft.textWidth(t, 4);
    tft.drawString(UNITS[i], x + 14 + vw, y + PILL_H / 2 + 2, 2);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(warn ? WHITE : PILL_LABEL);
    tft.drawString(NAMES[i], x + w - 12, y + PILL_H / 2 + 1, 2);
}

// ---- the street panel (sprite coordinates) ----
void stripes(TFT_eSprite& s, int off) {
    s.fillSprite(ST_RED);
    constexpr int P = streetface::Stripes::PERIOD, BAND = 9;
    for (int x = -ST_H - P; x < ST_W + P; x += P) {   // 45-degree bands running left
        const int a = x - off;
        s.fillTriangle(a, ST_H, a + BAND, ST_H, a + BAND + ST_H, 0, ST_STRIPE);
        s.fillTriangle(a, ST_H, a + BAND + ST_H, 0, a + ST_H, 0, ST_STRIPE);
    }
}

void trace(TFT_eSprite& s) {
    s.fillRoundRect(4, 4, ST_W - 8, 28, 6, WAVE_BG);
    s.drawRoundRect(4, 4, ST_W - 8, 28, 6, CHROME_HI);
    constexpr int MID = 18, HMAX = 11;
    for (int i = 0; i < streetface::Trace::N; ++i) {
        const int x = 7 + i * 4, h = s_trace.height(i, HMAX);
        if (h < 0) s.fillRect(x, MID - 1, 2, 2, HAIR_DK);
        else s.fillRect(x, MID - h - 1, 2, 2 * h + 2, WAVE_BAR);
    }
}

void eye(TFT_eSprite& s, int ex, int ey, int side, streetface::Mood m, bool shut, int gl) {
    // side: -1 her right eye (screen left), +1 her left; the inner corner is towards the nose
    using streetface::Mood;
    if (shut) {
        s.drawWideLine(ex - 9, ey, ex, ey + 3, 2.5f, INK, SKIN);
        s.drawWideLine(ex, ey + 3, ex + 9, ey, 2.5f, INK, SKIN);
    } else {
        const int ry = m == Mood::Angry ? 7 : m == Mood::Focus ? 9 : 11;
        s.fillEllipse(ex, ey, 9, ry, WHITE);
        s.fillEllipse(ex + gl, ey + 1, 6, ry < 8 ? ry : 8, IRIS);
        s.fillCircle(ex + gl, ey + 2, 3, INK);
        s.fillCircle(ex + gl - 2, ey - 2, 2, WHITE);
        if (m == Mood::Angry || m == Mood::Focus) {   // the lid slants down to the nose
            const int drop = m == Mood::Angry ? 6 : 3, in = ex - side * 10, out = ex + side * 10;
            s.fillTriangle(out, ey - ry - 2, in, ey - ry - 2, in, ey - ry + drop, SKIN);
            s.drawWideLine(out, ey - ry, in, ey - ry + drop, 2.5f, INK, SKIN);
        } else {
            s.drawWideLine(ex - 10, ey - ry + 1, ex + 10, ey - ry, 2.5f, INK, SKIN);
        }
    }
    // the brow: level when calm, inner end down when focused, sharply down when angry
    const int drop = m == Mood::Angry ? 8 : m == Mood::Focus ? 4 : (m == Mood::Sleep ? -1 : 0);
    const int by = ey - 15, in = ex - side * 9, out = ex + side * 9;
    s.drawWideLine(out, by, in, by + drop, m == Mood::Angry ? 3.5f : 2.5f, HAIR_DK, SKIN);
}

void navigator(TFT_eSprite& s, streetface::Mood m, uint32_t nowMs) {
    using streetface::Mood;
    const int cx = 70, cy = 90;
    // hair at the back, the twin tails and their ties
    s.fillEllipse(cx - 47, cy + 20, 13, 34, HAIR);
    s.fillEllipse(cx + 47, cy + 20, 13, 34, HAIR);
    s.fillEllipse(cx - 50, cy + 14, 4, 18, HAIR_HI);
    s.fillEllipse(cx + 44, cy + 14, 4, 18, HAIR_HI);
    s.fillCircle(cx, cy - 6, 43, HAIR);
    s.fillCircle(cx - 40, cy - 12, 6, CYAN);
    s.fillCircle(cx + 40, cy - 12, 6, CYAN);
    // the face, then the fringe over it
    s.fillEllipse(cx, cy + 8, 33, 31, SKIN);
    s.fillRect(cx - 34, cy - 36, 68, 12, HAIR);
    for (int k = 0; k < 6; ++k) {
        const int x0 = cx - 36 + k * 12, tip = cy - 14 + ((k == 0 || k == 5) ? 10 : (k % 2) * 4);
        s.fillTriangle(x0, cy - 26, x0 + 14, cy - 26, x0 + 7, tip, HAIR);
    }
    s.drawWideLine(cx - 22, cy - 40, cx - 6, cy - 44, 3.0f, HAIR_HI, HAIR);   // a shine on the hair
    // the headset: the band over her head, the ear cup with its light, the mic boom
    s.drawSmoothArc(cx, cy - 4, 46, 42, 100, 260, GEAR, HAIR);
    s.fillRoundRect(cx + 31, cy - 6, 11, 22, 4, GEAR);
    s.fillCircle(cx + 36, cy + 4, 2, (nowMs / 500) % 2 ? CYAN : PINK);
    s.drawWideLine(cx + 34, cy + 15, cx + 16, cy + 30, 2.5f, GEAR, SKIN);
    s.fillCircle(cx + 15, cy + 30, 3, GEAR);
    // eyes, cheeks
    const bool shut = streetface::eyesShut(m, nowMs);
    const int gl = streetface::glance(m, nowMs);
    eye(s, cx - 15, cy + 6, -1, m, shut, gl);
    eye(s, cx + 15, cy + 6, +1, m, shut, gl);
    if (m == Mood::Focus || m == Mood::Angry) {
        s.fillEllipse(cx - 21, cy + 20, 5, 2, BLUSH);
        s.fillEllipse(cx + 21, cy + 20, 5, 2, BLUSH);
    }
    // the mouth
    const int my = cy + 27;
    switch (m) {
        case Mood::Angry:   // gritted teeth
            s.fillRoundRect(cx - 13, my - 5, 26, 11, 3, WHITE);
            s.drawRoundRect(cx - 13, my - 5, 26, 11, 3, INK);
            s.drawFastHLine(cx - 12, my, 24, INK);
            for (int k = -6; k <= 6; k += 6) s.drawFastVLine(cx + k, my - 4, 9, INK);
            break;
        case Mood::Focus:
            s.drawWideLine(cx - 6, my, cx + 6, my - 1, 2.0f, INK, SKIN);
            break;
        case Mood::Look:
            s.fillEllipse(cx, my, 3, 4, INK);
            break;
        case Mood::Sleep:
            s.drawWideLine(cx - 4, my, cx + 4, my, 2.0f, INK, SKIN);
            s.setTextColor(WHITE);
            s.setTextDatum(TL_DATUM);
            s.drawString((nowMs / 700) % 2 ? "z Z" : "Z z", cx + 30, cy - 46, 2);
            break;
        default:   // calm: a small smile
            s.drawWideLine(cx - 7, my - 2, cx, my + 2, 2.0f, INK, SKIN);
            s.drawWideLine(cx, my + 2, cx + 7, my - 2, 2.0f, INK, SKIN);
            break;
    }
}

void panel(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool shift) {
    const int off = s_stripes.update(l.kmh, nowMs);
    s_trace.update(l.rpm, nowMs);
    if (!s_panel) {
        tft.fillRect(ST_X, ST_Y, ST_W, ST_H, ST_RED);
        return;
    }
    TFT_eSprite& s = *s_panel;
    stripes(s, off);
    trace(s);
    navigator(s, streetface::mood(l.link, l.rpm), nowMs);
    s.drawRect(0, 0, ST_W, ST_H, shift ? PINK : LINE);
    if (shift) s.drawRect(1, 1, ST_W - 2, ST_H - 2, PINK);
    s.pushSprite(ST_X, ST_Y);
}
}  // namespace

void drawGauges(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole) {
    if (whole) {
        frame(tft);
        memset(&s_shown, 0, sizeof s_shown);
        s_shown.thrCells = s_shown.loadCells = -1;
        s_trace.reset();
        if (!s_panel) {   // once; kept for the next time the gauges come up
            s_panel = new TFT_eSprite(&tft);
            s_panel->setColorDepth(16);
            if (!s_panel->createSprite(ST_W, ST_H)) {
                delete s_panel;
                s_panel = nullptr;
            }
        }
    }
    const bool shift = gaugehud::shiftOn(l.rpm, nowMs);
    char clock[6];
    clockText(clock);
    if (whole || shift != s_shown.shift || l.link != s_shown.link || strcmp(clock, s_shown.clock) != 0)
        header(tft, shift, l.link, clock, whole);
    s_shown.shift = shift;
    s_shown.link = l.link;
    strlcpy(s_shown.clock, clock, sizeof s_shown.clock);
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
        number(tft, t, RPM_R, RPM_Y, 70, 26, 4, std::isfinite(l.rpm) ? TEXT : DIM, BG);
        strlcpy(s_shown.rpm, t, sizeof s_shown.rpm);
    }
    // the speed: only the digits that changed
    char sp[4];
    gaugehud::speedCells(l.kmh, sp);
    for (int k = 0; k < 3; ++k)
        if (whole || sp[k] != s_shown.speed[k]) digit(tft, SPD_X + k * (DIG_W + DIG_GAP), SPD_Y, sp[k]);
    memcpy(s_shown.speed, sp, sizeof sp);
    // throttle and load
    const int thr = gaugehud::cells(l.throttle, CELLS), load = gaugehud::cells(l.load, CELLS);
    if (whole || thr != s_shown.thrCells) cellsRow(tft, THR_X, thr, PINK, GHOST_PINK);
    if (whole || load != s_shown.loadCells) cellsRow(tft, LOAD_X, load, CYAN, GHOST_CYAN);
    s_shown.thrCells = thr;
    s_shown.loadCells = load;
    // the pills
    const float vals[3] = {l.coolC, l.intakeC, l.volts};
    const bool warn[3] = {gaugehud::coolWarn(l.coolC), false, gaugehud::battWarn(l.volts, l.rpm)};
    for (int i = 0; i < 3; ++i) {
        gaugehud::text(vals[i], i == 2 ? 1 : 0, t, sizeof t);
        const bool known = std::isfinite(vals[i]);
        if (whole || warn[i] != s_shown.pillWarn[i] || known != s_shown.pillKnown[i] || strcmp(t, s_shown.pill[i]) != 0) {
            pill(tft, i, t, warn[i], known);
            s_shown.pillWarn[i] = warn[i];
            s_shown.pillKnown[i] = known;
            strlcpy(s_shown.pill[i], t, sizeof s_shown.pill[i]);
        }
    }
    // the street panel: every frame (it moves)
    panel(tft, l, nowMs, shift);
}
}  // namespace ui
