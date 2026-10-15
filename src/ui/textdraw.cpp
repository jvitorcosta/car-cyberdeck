#include "textdraw.h"
#include "accents.h"
#include "battle.h"
#include "cjkdata.h"

namespace ui {

int asciiWidth2(char c, void* ctx) {
    char one[2] = {c, 0};
    return ((TFT_eSPI*)ctx)->textWidth(one, 2);
}

int wideWidth(uint32_t cp, void*) {
    int w = 0;
    return cjk().glyph(cp, &w) ? w : 0;
}

// s: integer scale (1 = the deck's normal size); each pixel becomes an s x s block.
static void drawMark(TFT_eSPI& t, txt::Mark m, char base, int cx, int top, uint16_t col, int s) {
    int r0 = top + accents::topRow(m, base) * s;
    for (int y = 0; y < accents::H; ++y) {
        const char* r = accents::row(m, y);
        if (!r) return;
        for (int x = 0; x < accents::W; ++x)
            if (r[x] == '#') t.fillRect(cx - accents::W / 2 * s + x * s, r0 + y * s, s, s, col);
    }
}

static void drawWide(TFT_eSPI& t, uint32_t cp, int x, int top, uint16_t col, int s) {
    int w = 0;
    const uint8_t* g = cjk().glyph(cp, &w);
    if (!g) return;
    int bpr = w / 8;                                     // bytes per row
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < w; ++c)
            if ((g[r * bpr + c / 8] >> (7 - (c & 7))) & 1)
                t.fillRect(x + c * s, top + (CJK_DY + r) * s, s, s, col);
}

int drawRun(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
            uint16_t shadow, uint8_t datum) {
    return drawRunScaled(t, it, n, x, y, fg, shadow, datum, 1);
}

int drawRunScaled(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
                  uint16_t shadow, uint8_t datum, int scale) {
    const int s = scale < 1 ? 1 : scale;
    int w = glyphrun::width(it, n) * s, h = t.fontHeight(2) * s;
    int left = x, top = y;
    switch (datum) {
        case TC_DATUM: left = x - w / 2; break;
        case TR_DATUM: left = x - w; break;
        case ML_DATUM: top = y - h / 2; break;
        case MC_DATUM: left = x - w / 2; top = y - h / 2; break;
        default: break;   // TL_DATUM
    }
    int px = left;
    char one[2] = {0, 0};
    for (size_t i = 0; i < n; ++i) {
        const glyphrun::Item& g = it[i];
        if (g.kind == glyphrun::Kind::Wide) {
            drawWide(t, g.cp, px + s, top + s, shadow, s);   // shadow first, like shadowText
            drawWide(t, g.cp, px, top, fg, s);
        } else {
            one[0] = g.ch;
            if (s == 1) {
                shadowText(t, one, px, top, 2, fg, shadow, TL_DATUM);
            } else {   // the same transparent text + shadow, with font 2 enlarged
                t.setTextSize(s);
                t.setTextDatum(TL_DATUM);
                t.setTextColor(shadow);
                t.drawString(one, px + s, top + s, 2);
                t.setTextColor(fg);
                t.drawString(one, px, top, 2);
                t.setTextSize(1);
            }
            if (g.mark != txt::Mark::None) {
                int cx = px + (g.w * s - 1) / 2;
                drawMark(t, g.mark, g.ch, cx + s, top + s, shadow, s);
                drawMark(t, g.mark, g.ch, cx, top, fg, s);
            }
        }
        px += g.w * s;
    }
    return w;
}

int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper) {
    constexpr size_t MAX_ITEMS = 96;   // AppState strings are < 96 bytes
    glyphrun::Item items[MAX_ITEMS];
    size_t n = glyphrun::decode(utf8 ? utf8 : "", items, MAX_ITEMS, asciiWidth2, wideWidth, &t, upper);
    n = glyphrun::fit(items, n, MAX_ITEMS, maxW, asciiWidth2, &t);
    return drawRun(t, items, n, x, y, fg, shadow, datum);
}

}
