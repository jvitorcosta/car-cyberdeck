#include "otascreen.h"
#include <cstdio>
#include "battle.h"
#include "theme.h"

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

}
