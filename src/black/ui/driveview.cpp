#include "driveview.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "../../util/car/drivedash.h"
#include "../../util/car/gaugehud.h"
#include "../../util/common/rgb565.h"
#include "../core/mem.h"

namespace ui {
namespace {
using drivedash::Box;
using rgb565::hex;

// the mocks' palette (native RGB565: sprite draws)
constexpr uint16_t HOT = hex(0xff1f6e), DEEP = hex(0xb0003a), CY = hex(0x00e5ff), PU = hex(0x9b4dff),
                   AL = hex(0xff2020), WIN = hex(0x0a0006), RIM = hex(0x3a3a44), GHOST = hex(0x05212a),
                   ARC_OFF = hex(0x3a0f26), CELL_OFF = hex(0x2a0a18), WHITE = hex(0xffffff), BLACK = 0;

// the speed: three digits in the window
constexpr int DIG_X = 62, DIG_Y = 109, DIG_W = 48, DIG_H = 56, DIG_T = 9, DIG_GAP = 8;
// the ladders: COOL, LOAD, VOLT columns
constexpr int LAD_Y = 174, LAD_W = 96, LAD_BAR_Y = 190, LAD_BAR_H = 14, CELLS = (LAD_W - 4) / 5;
constexpr int WARM_X = 74, WARM_Y = 214, WARM_W = 240, WARM_H = 12;
struct Region { int16_t x, y, w, h; };
constexpr Region TOP{62, 22, 196, 86}, SPEED{60, 107, 168, 60}, TAG{200, 0, 120, 20}, WARM{72, 212, 244, 16};
Region ladder(int i) { return Region{(int16_t)(6 + i * 104), (int16_t)LAD_Y, (int16_t)(LAD_W + 4), 32}; }

TFT_eSprite* s_frame = nullptr;   // the screen as drawn (PSRAM)
uint16_t* s_bg = nullptr;         // the background baked once, in the sprite's own byte order
int s_state = 0;                  // 0 not made yet, 1 ready, -1 no memory
drivedash::Alerts s_alerts;
struct Shown {
    int arc, warm, cells[3];
    char rpm[8], speed[4], val[3][8], tag[8];
    bool alert[3];
} s_shown;

void restore(Region r) {   // the background under r back into the frame
    uint16_t* f = (uint16_t*)s_frame->getPointer();
    for (int y = r.y; y < r.y + r.h; ++y) memcpy(f + y * 320 + r.x, s_bg + y * 320 + r.x, (size_t)r.w * 2);
}
void push(Region r) { s_frame->pushSprite(r.x, r.y, r.x, r.y, r.w, r.h); }

// The background: the maroon fall, the faint pink diagonals, the black header, the speed window, the
// labels that never change.
void bake() {
    TFT_eSprite& s = *s_frame;
    for (int y = 0; y < 240; ++y) {
        const float f = y / 239.0f;
        const int r = (int)lroundf(42 + (18 - 42) * f), g = 0, b = (int)lroundf(24 + (10 - 24) * f);
        for (int x = 0; x < 320; ++x) {
            const bool line = (x + y) % 16 == 0;   // 7 % of the pink on the diagonals
            s.drawPixel(x, y, line ? rgb565::rgb((uint8_t)(r + (255 - r) * 7 / 100), (uint8_t)(g + 31 * 7 / 100),
                                                 (uint8_t)(b + (110 - b) * 7 / 100))
                                   : rgb565::rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
        }
    }
    s.fillRect(0, 0, 320, 20, BLACK);
    s.setFreeFont(&FreeSansBold9pt7b);
    s.setTextColor(HOT);
    s.setTextDatum(ML_DATUM);
    s.drawString("DRIVE", 6, 10);
    s.setFreeFont(nullptr);
    s.fillRoundRect(52, 104, 216, 66, 6, WIN);
    s.drawRoundRect(52, 104, 216, 66, 6, RIM);
    s.setTextColor(PU);
    s.setTextDatum(BL_DATUM);
    s.drawString("KM/H", 228, 164, 2);
    static const char* const LABEL[3] = {"COOL", "LOAD", "VOLT"};
    s.setTextDatum(TL_DATUM);
    for (int i = 0; i < 3; ++i) s.drawString(LABEL[i], 8 + i * 104, LAD_Y + 1, 2);
    s.setTextColor(DEEP);
    s.drawString("WARM-UP", 6, WARM_Y - 1, 2);
    memcpy(s_bg, s.getPointer(), 320 * 240 * 2);
}

void drawTop(int lit, const char* rpm) {
    TFT_eSprite& s = *s_frame;
    restore(TOP);
    for (int i = 0; i < drivedash::ARC_N; ++i) {
        const drivedash::Quad q = drivedash::arcQuad(i);
        const bool on = i < lit;
        const uint16_t c = on ? (drivedash::arcHot(i) ? AL : HOT) : ARC_OFF;
        s.fillTriangle(q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2], c);
        s.fillTriangle(q.x[0], q.y[0], q.x[2], q.y[2], q.x[3], q.y[3], c);
        if (on) s.drawLine((q.x[0] + q.x[3]) / 2, (q.y[0] + q.y[3]) / 2, (q.x[1] + q.x[2]) / 2, (q.y[1] + q.y[2]) / 2, WHITE);
    }
    s.setFreeFont(&FreeSansBold18pt7b);
    s.setTextColor(CY);
    s.setTextDatum(TC_DATUM);
    s.drawString(rpm, 160, 42);
    s.setFreeFont(nullptr);
    s.setTextColor(PU);
    s.drawString("RPM x1000", 160, 78, 2);
    push(TOP);
}

void drawSpeed(const char* cells) {
    TFT_eSprite& s = *s_frame;
    restore(SPEED);
    for (int d = 0; d < 3; ++d) {
        const int x = DIG_X + d * (DIG_W + DIG_GAP);
        const uint8_t on = drivedash::segments(cells[d]);
        for (int k = 0; k < 7; ++k) {
            const Box b = drivedash::segment(k, x, DIG_Y, DIG_W, DIG_H, DIG_T);
            s.fillRect(b.x, b.y, b.w, b.h, (on >> k) & 1 ? CY : GHOST);
        }
    }
    push(SPEED);
}

void drawLadder(int i, int lit, const char* val, bool alert, uint16_t col, bool hotZone) {
    TFT_eSprite& s = *s_frame;
    const Region r = ladder(i);
    restore(r);
    const int px = 8 + i * 104;
    // the value: inverted red on a problem
    const int vw = 40;
    if (alert) s.fillRect(px + LAD_W - vw, LAD_Y, vw, 16, AL);
    s.setTextColor(WHITE);
    s.setTextDatum(TR_DATUM);
    s.drawString(val, px + LAD_W - 2, LAD_Y + 1, 2);
    s.fillRect(px, LAD_BAR_Y, LAD_W, LAD_BAR_H, WIN);
    s.drawRect(px, LAD_BAR_Y, LAD_W, LAD_BAR_H, RIM);
    for (int c = 0; c < CELLS; ++c) {
        const bool hot = hotZone && c * 5 > CELLS * 4;   // the last 20 % of the coolant's ladder
        s.fillRect(px + 2 + c * 5, LAD_BAR_Y + 2, 3, LAD_BAR_H - 4, c < lit ? (alert || hot ? AL : col) : CELL_OFF);
    }
    push(r);
}

void drawWarm(int lit) {
    TFT_eSprite& s = *s_frame;
    restore(WARM);
    const int cells = (WARM_W - 4) / 5;
    s.fillRect(WARM_X, WARM_Y, WARM_W, WARM_H, WIN);
    s.drawRect(WARM_X, WARM_Y, WARM_W, WARM_H, RIM);
    for (int c = 0; c < cells; ++c) s.fillRect(WARM_X + 2 + c * 5, WARM_Y + 2, 3, WARM_H - 4, c < lit ? CY : CELL_OFF);
    push(WARM);
}

void drawTag(const char* tag) {
    TFT_eSprite& s = *s_frame;
    restore(TAG);
    s.setTextColor(PU);
    s.setTextDatum(MR_DATUM);
    s.drawString(tag, 314, 10, 2);
    push(TAG);
}

void value(float v, int decimals, const char* suffix, char* out, size_t n) {
    gaugehud::text(v, decimals, out, n);
    if (std::isfinite(v)) strlcat(out, suffix, n);
}
}  // namespace

bool driveReady(TFT_eSPI& tft) {
    if (s_state == 0) {
        s_state = -1;
        s_bg = (uint16_t*)mem::bigAlloc(320 * 240 * 2);
        if (s_bg) {
            s_frame = new TFT_eSprite(&tft);
            s_frame->setColorDepth(16);
            if (s_frame->createSprite(320, 240)) {   // PSRAM: TFT_eSprite takes it when there is some
                bake();
                s_state = 1;
            } else {   // not tried again: give both back
                delete s_frame;
                s_frame = nullptr;
                free(s_bg);
                s_bg = nullptr;
            }
        }
        Serial.printf("[screen] drive dash %s\n", s_state == 1 ? "ready" : "out of memory");
    }
    return s_state == 1;
}

void drawDrive(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole) {
    if (!driveReady(tft)) return;
    if (whole) {
        memcpy(s_frame->getPointer(), s_bg, 320 * 240 * 2);
        s_frame->pushSprite(0, 0);
        memset(&s_shown, 0, sizeof s_shown);
        s_shown.arc = s_shown.warm = -1;
        for (int i = 0; i < 3; ++i) s_shown.cells[i] = -1;
    }
    const bool live = l.link == obdplan::Link::Live;
    const float rpm = live ? l.rpm : NAN, kmh = live ? l.kmh : NAN, cool = live ? l.coolC : NAN,
                load = live ? l.load : NAN, volts = l.volts;   // the adapter's own reading, link or not
    if (live) s_alerts.update(cool, volts, rpm, nowMs);
    else s_alerts = drivedash::Alerts();   // no readings, no red: the values say "--"
    // the link's state, top right
    const char* tag = gaugehud::linkTag(l.link);
    if (strcmp(tag, s_shown.tag) != 0) {
        drawTag(tag);
        strlcpy(s_shown.tag, tag, sizeof s_shown.tag);
    }
    // the arc and the RPM
    char t[8];
    drivedash::rpmText(rpm, t, sizeof t);
    const int lit = drivedash::arcLit(rpm);
    if (lit != s_shown.arc || strcmp(t, s_shown.rpm) != 0) {
        drawTop(lit, t);
        s_shown.arc = lit;
        strlcpy(s_shown.rpm, t, sizeof s_shown.rpm);
    }
    // the speed
    char sp[4];
    gaugehud::speedCells(kmh, sp);
    if (strcmp(sp, s_shown.speed) != 0) {
        drawSpeed(sp);
        strlcpy(s_shown.speed, sp, sizeof s_shown.speed);
    }
    // the ladders: coolant to 130 C, load to 100 %, volts 10 to 16
    const float v[3] = {cool, load, std::isfinite(volts) ? volts - 10.0f : NAN}, max[3] = {130.0f, 100.0f, 6.0f};
    const bool alert[3] = {s_alerts.coolant(), false, s_alerts.volts()};
    const uint16_t col[3] = {CY, HOT, PU};
    char val[3][8];
    value(cool, 0, "C", val[0], sizeof val[0]);
    value(load, 0, "%", val[1], sizeof val[1]);
    value(volts, 1, "V", val[2], sizeof val[2]);
    for (int i = 0; i < 3; ++i) {
        const int cells = drivedash::ladderLit(v[i], max[i], CELLS);
        if (cells != s_shown.cells[i] || alert[i] != s_shown.alert[i] || strcmp(val[i], s_shown.val[i]) != 0) {
            drawLadder(i, cells, val[i], alert[i], col[i], i == 0);
            s_shown.cells[i] = cells;
            s_shown.alert[i] = alert[i];
            strlcpy(s_shown.val[i], val[i], sizeof s_shown.val[i]);
        }
    }
    // the warm-up
    const int warm = (int)lroundf(drivedash::warmup(cool) * ((WARM_W - 4) / 5));
    if (warm != s_shown.warm) {
        drawWarm(warm);
        s_shown.warm = warm;
    }
}
}  // namespace ui
