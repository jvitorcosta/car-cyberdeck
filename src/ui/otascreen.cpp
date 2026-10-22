#include "otascreen.h"
#include <cstdio>
#include <cstring>
#include "battle.h"
#include "theme.h"
#include "../util/backdrop.h"
#include "../util/ridersprite.h"
#include "../util/rgb565.h"

namespace ui {

constexpr int BOX_X = 40, BOX_Y = 56, BOX_W = 240, BOX_H = 64;
constexpr int BAR_X = BOX_X + 14, BAR_Y = BOX_Y + 46, BAR_W = BOX_W - 28;
constexpr int PCT_X = BOX_X + BOX_W - 14, PCT_Y = BOX_Y + 10;

void drawUpdate(TFT_eSPI& t, int pct, bool full) {
    const theme::Palette& p = theme::pal();
    if (full) {
        background(t);
        topStrip(t);
        shadowText(t, "CarDeck", 6, 2, 2, p.topText, p.boxBorder, TL_DATUM);
        battleBox(t, BOX_X, BOX_Y, BOX_W, BOX_H, Tab::Right);
        shadowText(t, "NEW FIRMWARE", BOX_X + 14, BOX_Y + 10, 2, p.text, p.textShadow, TL_DATUM);
        dialogueBox(t, 6, 160, 308, 74);
        shadowText(t, "Updating over WiFi...", 18, 172, 4, p.text, p.dlgShadow, TL_DATUM);
        shadowText(t, "Keep the deck powered", 18, 202, 2, p.text, p.dlgShadow, TL_DATUM);
    }
    char s[8];
    snprintf(s, sizeof s, "%d%%", pct);
    t.fillRect(PCT_X - 44, PCT_Y, 44, 16, p.boxFill);
    shadowText(t, s, PCT_X, PCT_Y, 2, p.text, p.textShadow, TR_DATUM);
    expBar(t, BAR_X, BAR_Y, BAR_W, pct / 100.0f);
}

// ---- The Star Light Zone screen (spec 2026-10-22-shadow-update-screen-design) ----
namespace {
using namespace otaview;
using rgb565::hex;
constexpr uint16_t WHITE = 0xFFFF, BLACK = 0x0000, RED = hex(0xFF5A2A), DEEP = hex(0xC01818);
constexpr uint16_t BORDER_C = hex(0xE8E8F0), TRACK = hex(0x101018), GREY = hex(0x808080);
constexpr uint16_t JET = hex(0x77FFFF), JET_EDGE = hex(0x2A9AB0);
constexpr int STRIP_H = 20, BAND_Y = 186, BAND_H = 26, NOTE_Y = 216, RIDER_W = 20;
constexpr uint32_t BOB_MS = 120;

// One backdrop row into the screen (rows 0..STRIP_H-1 darkened to ~20% for the strip). The file
// need not be 2-byte aligned: copied through a row buffer, as avatarview does.
void pushRow(TFT_eSPI& t, int y) {
    static uint16_t row[320];
    const uint8_t* src = backdrop::row(y);
    if (!src) return;
    const int w = backdrop::width();
    memcpy(row, src, (size_t)w * 2);              // big-endian pixels: pushImage's order as is
    if (y < STRIP_H)
        for (int x = 0; x < w; ++x) {
            const uint16_t c = rgb565::be(row[x]);
            const uint16_t r = (c >> 11) / 5, g = ((c >> 5) & 63) / 5, b = (c & 31) / 5;
            row[x] = rgb565::be((uint16_t)(r << 11 | g << 5 | b));
        }
    t.pushImage(0, y, w, 1, row);
}

// The fill's colour at column i of the rail's inside: deep red on the left to orange.
uint16_t fillColour(int i) {
    const int k = INNER_W > 1 ? i * 255 / (INNER_W - 1) : 0;
    auto mix = [k](int a, int b) { return (uint8_t)(a + (b - a) * k / 255); };
    return rgb565::rgb(mix(0xC0, 0xFF), mix(0x18, 0x5A), mix(0x18, 0x2A));
}

void strip(TFT_eSPI& t, const ota::View& v) {
    for (int y = 0; y < STRIP_H; ++y) pushRow(t, y);
    const bool failed = v.phase == Phase::Failed;
    shadowText(t, failed ? "Update failed" : "FIRMWARE UPDATE", 8, 2, 2, WHITE, BLACK, TL_DATUM);
    char s[8];
    snprintf(s, sizeof s, "%d%%", v.pct < 0 ? 0 : v.pct > 100 ? 100 : v.pct);
    shadowText(t, s, 312, 2, 2, failed ? GREY : RED, BLACK, TR_DATUM);
}

// The rail band (backdrop rows under and around the rail, then the rail and its fill).
void rail(TFT_eSPI& t, const ota::View& v, int top) {
    for (int y = top; y < BAND_Y + BAND_H; ++y) pushRow(t, y);
    const bool failed = v.phase == Phase::Failed;
    t.fillRect(RAIL_X, RAIL_Y, RAIL_W, RAIL_H, failed ? GREY : BORDER_C);
    t.fillRect(INNER_X, RAIL_Y + BORDER, INNER_W, RAIL_H - 2 * BORDER, TRACK);
    const int fill = fillWidth(v.pct);
    for (int i = 0; i < fill; ++i)
        t.drawFastVLine(INNER_X + i, RAIL_Y + BORDER, RAIL_H - 2 * BORDER, failed ? GREY : fillColour(i));
}

// The jet glows over the fill's tip (stage 2: Shadow above them). rows: repaint the backdrop above
// the rail first (a bob step alone; after rail() it's already clean).
void jets(TFT_eSPI& t, const ota::View& v, int bob, bool rows) {
    if (rows)
        for (int y = BAND_Y; y < RAIL_Y; ++y) pushRow(t, y);
    const int x = riderX(v.pct, RIDER_W), y = RAIL_Y - 11 + bob;
    for (const int jx : {x + 2, x + RIDER_W - 8}) {
        t.fillRoundRect(jx, y, 6, 8, 3, JET_EDGE);
        t.fillRoundRect(jx + 1, y + 1, 4, 5, 2, JET);
    }
}
}

// Shadow (util/ridersprite) over his stretch of the night, composed row by row in RAM and pushed
// once per row, so he never flickers: rows riderTop..RAIL_Y-1 across the whole width (his last
// spot is cleared with them).
void rider(TFT_eSPI& t, const ota::View& v, int frame) {
    static uint16_t row[320];
    const int w = ridersprite::width(), bw = backdrop::width();
    const int x0 = riderX(v.pct, w), top = riderTop(ridersprite::height());
    for (int y = top; y < RAIL_Y; ++y) {
        const uint8_t* src = backdrop::row(y);
        if (!src) continue;
        memcpy(row, src, (size_t)bw * 2);
        for (int x = 0; x < w && x0 + x < bw; ++x) {
            uint16_t c;
            if (ridersprite::pixel(frame, x, y - top, &c)) row[x0 + x] = rgb565::be(c);
        }
        t.pushImage(0, y, bw, 1, row);
    }
}

bool drawShadowUpdate(TFT_eSPI& t, const ota::View& v, uint32_t nowMs, bool full) {
    if (backdrop::width() == 0) return false;
    static Phase shownPhase = Phase::Idle;
    static int shownPct = -1, shownStep = -1;
    if (v.phase == Phase::Done) {                 // the flash, once; the deck reboots after it
        if (full || shownPhase != Phase::Done) {
            t.fillScreen(DEEP);
            shadowText(t, "FIRMWARE UPDATE", 160, 112, 4, WHITE, BLACK, TC_DATUM);
        }
        shownPhase = v.phase;
        return true;
    }
    // The animation's step: Shadow's run frame with his file, else the jet glows' 1 px bob.
    const bool shadow = ridersprite::runFrames() > 0;
    const int step = shadow ? ridersprite::frameAt(nowMs) : (int)((nowMs / BOB_MS) & 1);
    const Repaint r = repaint(full, v.phase != shownPhase || v.pct != shownPct, step != shownStep, v.phase);
    if (full) {
        for (int y = STRIP_H; y < backdrop::height(); ++y) pushRow(t, y);
        shadowText(t, "Keep the deck powered", RAIL_X, NOTE_Y, 2, WHITE, BLACK, TL_DATUM);
    }
    if (r.strip) strip(t, v);
    if (r.rail) rail(t, v, railRepaintTop(shadow));
    if (shadow) {
        if (r.jets) rider(t, v, step);                                  // skating
        else if (r.rail && v.phase == Phase::Failed) rider(t, v, ridersprite::stand());   // he stops
    } else if (r.jets) {
        jets(t, v, step, !r.rail);
    }
    shownPhase = v.phase;
    shownPct = v.pct;
    shownStep = step;
    return true;
}

}
