#include "karaokeview.h"
#include "battle.h"
#include "screen_now.h"
#include "textdraw.h"
#include "theme.h"
#include "lyricmsg.h"
#include "../core/mem.h"
#include "../../util/black/glyphrun.h"

namespace ui {

// ---- layout (320x240): the mockup's option B at half size ----
static constexpr int TOP_H = 44;                          // song, artist, bar over the top colour
static constexpr int BAR_X = 8, BAR_Y = 38, BAR_W = 304, BAR_H = 4;
static constexpr int BOX_X = 4, BOX_Y = 44, BOX_W = 312, BOX_H = 194;
static constexpr int IN_X = BOX_X + 6, IN_W = BOX_W - 12;  // the box's inside, left to right
static constexpr int TEXT_X = 16, TEXT_W = 288;           // every line starts at TEXT_X
static constexpr int PREV_Y = 54;                         // the line just sung, font 2
static constexpr int ROW1_Y = 120, ROW_H = 32;            // the sung line at 2x; feet on ROW1_Y
static constexpr int NEXT_Y = 192;                        // the next line, font 2
static constexpr int SCALE = 2;
// Walker band: ROW1_Y - BAND_H - 1 - HOP_PX = 73, clear of the line just sung (54..72).

static glyphrun::Item* s_items = nullptr;   // the sung line, decoded (PSRAM)
static size_t s_aEnd = 0, s_bStart = 0, s_bN = 0;
static int s_w1 = 0, s_w2 = 0;              // its rows' widths at 2x; both 0 for a message
static int s_lit = 0;                       // px swept so far
static walkrect::Rect s_walk{0, 0, 0, 0};

// a and b mixed 1:1 per channel (RGB565): the dim tones between the text and the box.
static uint16_t mix(uint16_t a, uint16_t b) {
    return (uint16_t)((((a & 0xF81F) + (b & 0xF81F)) >> 1 & 0xF81F) |
                      (((a & 0x07E0) + (b & 0x07E0)) >> 1 & 0x07E0));
}
static uint16_t dimTone() { return mix(mix(theme::pal().text, theme::pal().dlgFill), theme::pal().dlgFill); }

static void drawRow(TFT_eSPI& t, int row, uint16_t fg, uint16_t shadow) {
    if (row == 0) drawRunScaled(t, s_items, s_aEnd, TEXT_X, ROW1_Y, fg, shadow, TL_DATUM, SCALE);
    else drawRunScaled(t, s_items + s_bStart, s_bN, TEXT_X, ROW1_Y + ROW_H, fg, shadow, TL_DATUM, SCALE);
}

// The lit tone over a row, clipped to x0..x1 (from TEXT_X) inside whatever viewport the loop set
// (the camera band's).
static void litBand(TFT_eSPI& t, int row, int x0, int x1) {
    const int vx = t.getViewportX(), vy = t.getViewportY(), vw = t.getViewportWidth(),
              vh = t.getViewportHeight();
    const bool vd = t.getViewportDatum();
    const int y = ROW1_Y + row * ROW_H;
    const int cx0 = max(TEXT_X + x0, vx), cy0 = max(y, vy);
    const int cx1 = min(TEXT_X + x1 + SCALE, vx + vw), cy1 = min(y + ROW_H + SCALE, vy + vh);   // + shadow
    if (cx1 > cx0 && cy1 > cy0) {
        t.setViewport(cx0, cy0, cx1 - cx0, cy1 - cy0, false);
        drawRow(t, row, theme::pal().text, theme::pal().dlgShadow);
    }
    t.setViewport(vx, vy, vw, vh, vd);
}

static void drawLines(TFT_eSPI& t, const shared::KaraokeLines& k, const char* message) {
    const uint16_t fill = theme::pal().dlgFill;
    t.fillRect(IN_X, PREV_Y - 2, IN_W, 20, fill);
    t.fillRect(IN_X, ROW1_Y, IN_W, 2 * ROW_H + SCALE, fill);
    t.fillRect(IN_X, NEXT_Y - 2, IN_W, 20, fill);
    s_aEnd = s_bStart = s_bN = 0;
    s_w1 = s_w2 = s_lit = 0;

    // Before the first line the first line waits in the sung line's place, nothing under it.
    const bool intro = !message && !k.cur[0];
    const char* sung = message ? message : (intro ? k.next : k.cur);
    if (!message && k.prev[0])
        drawText(t, k.prev, TEXT_X, PREV_Y, dimTone(), fill, TL_DATUM, TEXT_W);
    if (!message && !intro && k.next[0])
        drawText(t, k.next, TEXT_X, NEXT_Y, mix(theme::pal().text, fill), fill, TL_DATUM, TEXT_W);

    constexpr size_t MAX_ITEMS = lyricmsg::TEXT_CAP;   // one item per byte at most; +3 for "..."
    if (!s_items) s_items = (glyphrun::Item*)mem::bigAlloc((MAX_ITEMS + 3) * sizeof(glyphrun::Item));
    if (!s_items || !sung[0]) return;
    const size_t n = glyphrun::decode(sung, s_items, MAX_ITEMS, asciiWidth2, wideWidth, &t, false,
                                      emojiWidth, emojiSeq);
    if (!n) return;
    const glyphrun::Wrap w = glyphrun::wrapTwo(s_items, n, TEXT_W / SCALE, 3 * asciiWidth2('.', &t));
    s_aEnd = w.aEnd;
    if (w.bStart < w.bEnd) {
        s_bStart = w.bStart;
        s_bN = w.bEnd - w.bStart;
        if (w.bEllipsis) {   // items past bEnd are not drawn, so "..." can overwrite them
            for (int i = 0; i < 3; ++i)
                s_items[w.bEnd + i] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.',
                                       (uint8_t)asciiWidth2('.', &t)};
            s_bN += 3;
        }
    }
    // A message reads in the text tone at once; a lyric line starts dim and gets swept.
    const uint16_t fg = message ? theme::pal().text : dimTone();
    const uint16_t sh = message ? theme::pal().dlgShadow : fill;
    drawRow(t, 0, fg, sh);
    if (s_bN) drawRow(t, 1, fg, sh);
    if (!message) {
        s_w1 = glyphrun::width(s_items, s_aEnd) * SCALE;
        s_w2 = s_bN ? glyphrun::width(s_items + s_bStart, s_bN) * SCALE : 0;
    }
}

void drawKaraokeBar(TFT_eSPI& t, const AppState& st) {
    const float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    const int fw = (int)(frac > 1.0f ? BAR_W : frac * BAR_W);
    t.fillRect(BAR_X, BAR_Y, fw, BAR_H, theme::EXP_BLUE);
    t.fillRect(BAR_X + fw, BAR_Y, BAR_W - fw, BAR_H, theme::HP_EMPTY);
}

void drawKaraoke(TFT_eSPI& t, const AppState& st, const shared::KaraokeLines& k, const char* message,
                 bool whole) {
    if (whole) {
        t.fillRect(0, 0, t.width(), TOP_H, theme::pal().top);
        drawText(t, st.trackName, 8, 4, theme::pal().topText, theme::pal().boxBorder, TL_DATUM, 304, true);
        drawText(t, st.artist, 8, 20, theme::CD_SILVER, theme::pal().boxBorder, TL_DATUM, 304);
        drawKaraokeBar(t, st);
        t.fillRect(0, BOX_Y, t.width(), t.height() - BOX_Y, theme::pal().top);
        dialogueBox(t, BOX_X, BOX_Y, BOX_W, BOX_H);
        s_walk = {0, 0, 0, 0};
    } else if (message && s_walk.w) {   // a message has no Pokémon on it
        t.fillRect(s_walk.x, s_walk.y, s_walk.w, s_walk.h, theme::pal().dlgFill);
        s_walk = {0, 0, 0, 0};
    }
    drawLines(t, k, message);
}

int karaokeLineW() { return s_w1 + s_w2; }

void drawKaraokeSweep(TFT_eSPI& t, int px) {
    const int total = s_w1 + s_w2;
    if (!total) return;
    if (px > total) px = total;
    if (px < s_lit) {   // the song went back inside the line: dim again, sweep from the start
        drawRow(t, 0, dimTone(), theme::pal().dlgFill);
        if (s_bN) drawRow(t, 1, dimTone(), theme::pal().dlgFill);
        s_lit = 0;
    }
    if (px <= s_lit) return;
    if (s_lit < s_w1) litBand(t, 0, s_lit, min(px, s_w1));
    if (px > s_w1) litBand(t, 1, max(s_lit, s_w1) - s_w1, px - s_w1);
    s_lit = px;
}

int karaokeWalkerOffset(int px) {
    const int total = s_w1 + s_w2;
    if (!total) return 0;
    if (px > total) px = total;
    return (int)((int64_t)px * s_w1 / total);
}

void drawKaraokeWalker(TFT_eSPI& t, uint32_t animMs, int step, int offset, int lift) {
    const int w = walkerWidth();
    if (!w) return;
    int cx = TEXT_X + offset;
    if (cx < IN_X + w / 2) cx = IN_X + w / 2;   // the whole sprite inside the box
    if (cx > IN_X + IN_W - w / 2) cx = IN_X + IN_W - w / 2;
    blitWalker(t, cx, ROW1_Y - lift, animMs, step, theme::pal().dlgFill, s_walk);
}
}  // namespace ui
