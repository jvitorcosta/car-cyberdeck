#include "karaokeview.h"
#include "screen_now.h"
#include "textdraw.h"
#include "lyricmsg.h"
#include "../core/mem.h"
#include "../../util/black/glyphrun.h"
#include "../../util/black/interp.h"
#include "../../util/black/karaokepal.h"
#include "../../util/black/lyricface.h"
#include "../../util/common/rgb565.h"
#include "../../util/common/termpal.h"

namespace ui {

using lyricface::Face;

// ---- layout (320x240): the yellow deck's terminal look, black and VT323 ----
// Rows' tops; each row is lyricface::lineHeight tall, its accents inside it.
static constexpr int TEXT_X = 12, TEXT_W = 296;           // every line starts at TEXT_X
static constexpr int SONG_Y = 1;                          // LINE (21 px): the song left, the artist right
static constexpr int SONG_GAP = 12, ARTIST_MAX_W = TEXT_W / 2;
// The walker, full size (64x32 at most), walks on the bar's top at the song's progress, as on the
// normal screen: its band 22..55, under the song's row.
static constexpr int BAR_X = TEXT_X, BAR_Y = 55, BAR_W = TEXT_W, BAR_H = 3;
static constexpr int PREV_Y = 62;                         // the line just sung, LINE: 62..83
static constexpr int ROW1_Y = 88;                         // the sung line's first row: 3 rows, 117 px in SUNG
static constexpr int NEXT_GAP = 6;                        // the next line, under the sung line's last row: to 232

struct Colours { uint16_t bg, song, artist, barOn, barOff, lit, unsung, prev, next; };
static Colours s_c;                          // setKaraokeLook
static glyphrun::Item* s_items = nullptr;   // the sung line, decoded (PSRAM)
static glyphrun::Rows s_rows{};             // its rows; n 0 for nothing to sweep
static Face s_face = Face::SUNG;            // its face: SUNG, or SUNG_SMALL when SUNG won't fit
static int s_w[glyphrun::MAX_ROWS] = {};    // its rows' widths; all 0 for a message
static int s_lit = 0;                       // px swept so far
static walkrect::Rect s_walk{0, 0, 0, 0};
static int s_clipX = 0, s_clipY = 0, s_clipW = 320, s_clipH = 240;   // setKaraokeClip
static int s_light = -1;                    // setKaraokeLook's: termpal's theme for the ALERT colour
static bool s_tagFix = true, s_tagLinked = false;   // drawKaraokeTags' last

void setKaraokeClip(int x, int y, int w, int h) {
    s_clipX = x;
    s_clipY = y;
    s_clipW = w;
    s_clipH = h;
}

void setKaraokeLook(int light) {
    const karaokepal::Pal p = karaokepal::of(light);
    s_light = light;
    s_c = {rgb565::hex(p.bg),     rgb565::hex(p.song),  rgb565::hex(p.artist),
           rgb565::hex(p.barOn),  rgb565::hex(p.barOff), rgb565::hex(p.lit),
           rgb565::hex(p.unsung), rgb565::hex(p.prev),  rgb565::hex(p.next)};
}

static int rowY(int row) { return ROW1_Y + row * lyricface::lineHeight(s_face); }

static void drawRow(TFT_eSPI& t, int row, uint16_t fg) {
    drawRunFace(t, s_items + s_rows.start[row], s_rows.end[row] - s_rows.start[row], TEXT_X, rowY(row), fg,
                s_face);
}

// The lit tone over a row, clipped to x0..x1 (from TEXT_X) inside the screens' clip (with vpDatum
// false TFT_eSPI's viewport getters say the whole screen, so main hands the clip over).
static void litBand(TFT_eSPI& t, int row, int x0, int x1) {
    const int y = rowY(row);
    const int cx0 = max(TEXT_X + x0, s_clipX), cy0 = max(y, s_clipY);
    const int cx1 = min(TEXT_X + x1, s_clipX + s_clipW);
    const int cy1 = min(y + lyricface::lineHeight(s_face), s_clipY + s_clipH);
    if (cx1 <= cx0 || cy1 <= cy0) return;
    t.setViewport(cx0, cy0, cx1 - cx0, cy1 - cy0, false);
    drawRow(t, row, s_c.lit);
    t.setViewport(s_clipX, s_clipY, s_clipW, s_clipH, false);
}

// One row of text in a face at y, cut with "..." to maxW, from TEXT_X or (right) ending at
// rightEnd. Its width.
static int drawLine(TFT_eSPI& t, const char* utf8, Face f, int y, uint16_t fg, bool upper = false,
                    int maxW = TEXT_W, bool right = false, int rightEnd = TEXT_X + TEXT_W) {
    constexpr size_t MAX_ITEMS = 96;   // more than TEXT_W holds in any face
    glyphrun::Item items[MAX_ITEMS];
    size_t n = glyphrun::decode(utf8, items, MAX_ITEMS, faceWidth, wideWidth, faceCtx(f), upper, emojiWidth,
                                emojiSeq);
    n = glyphrun::fit(items, n, MAX_ITEMS, maxW, faceWidth, faceCtx(f));
    const int w = glyphrun::width(items, n);
    drawRunFace(t, items, n, right ? rightEnd - w : TEXT_X, y, fg, f);
    return w;
}

// The link tags (drawKaraokeTags): "GPS" and "DECK" in SMALL, TAG_GAP apart, right-aligned at the
// row's end, on the song row's middle.
static constexpr const char* TAG_GPS = "GPS";
static constexpr const char* TAG_DECK = "DECK";
static constexpr int TAG_GAP = 6;
static int asciiW(const char* s, Face f) {
    int w = 0;
    for (; *s; ++s) w += faceWidth(*s, faceCtx(f));
    return w;
}
static int tagsW() { return asciiW(TAG_GPS, Face::SMALL) + TAG_GAP + asciiW(TAG_DECK, Face::SMALL); }
static void drawTag(TFT_eSPI& t, const char* s, int x, int y, uint16_t fg, bool strike) {
    glyphrun::Item items[8];
    const size_t n = glyphrun::decode(s, items, 8, faceWidth, wideWidth, faceCtx(Face::SMALL), false, emojiWidth,
                                      emojiSeq);
    drawRunFace(t, items, n, x, y, fg, Face::SMALL);
    if (strike) t.drawFastHLine(x, y + lyricface::lineHeight(Face::SMALL) / 2, asciiW(s, Face::SMALL), fg);
}
static void drawTags(TFT_eSPI& t) {
    const int w = tagsW(), x = TEXT_X + TEXT_W - w, h = lyricface::lineHeight(Face::LINE);
    const int y = SONG_Y + (h - lyricface::lineHeight(Face::SMALL)) / 2;
    t.fillRect(x, SONG_Y, w, h, s_c.bg);
    const uint16_t alert = rgb565::hex(termpal::rgb(termpal::ALERT, s_light, true));
    drawTag(t, TAG_GPS, x, y, s_tagFix ? s_c.unsung : alert, !s_tagFix);
    drawTag(t, TAG_DECK, x + asciiW(TAG_GPS, Face::SMALL) + TAG_GAP, y, s_tagLinked ? s_c.unsung : alert, !s_tagLinked);
}

void drawKaraokeTags(TFT_eSPI& t, bool fix, bool linked) {
    s_tagFix = fix;
    s_tagLinked = linked;
    drawTags(t);
}

// The sung line (or a message) in s_items, wrapped in the largest face that fits it in its rows (2 in
// SUNG, 3 in SUNG_SMALL); "..." on the last row only when even SUNG_SMALL doesn't.
static void layOut(const char* sung, size_t cap) {
    size_t n = 0;
    for (Face f : {Face::SUNG, Face::SUNG_SMALL}) {
        s_face = f;
        n = glyphrun::decode(sung, s_items, cap, faceWidth, wideWidth, faceCtx(f), false, emojiWidth, emojiSeq);
        s_rows = glyphrun::wrapRows(s_items, n, TEXT_W, 3 * faceWidth('.', faceCtx(f)), lyricface::rows(f));
        if (!s_rows.ellipsis) return;
    }
    // past the last row's end nothing is drawn, so "..." can overwrite it (s_items has room for 3)
    size_t& end = s_rows.end[s_rows.n - 1];
    for (int i = 0; i < 3; ++i)
        s_items[end++] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.',
                          (uint8_t)faceWidth('.', faceCtx(s_face))};
}

static void drawLines(TFT_eSPI& t, const shared::KaraokeLines& k, const char* message) {
    const int lineH = lyricface::lineHeight(Face::LINE);
    t.fillRect(0, PREV_Y, t.width(), lineH, s_c.bg);
    t.fillRect(0, ROW1_Y, t.width(), t.height() - ROW1_Y, s_c.bg);
    s_rows = {};
    s_w[0] = s_w[1] = s_w[2] = 0;
    s_lit = 0;

    // Before the first line the first line waits in the sung line's place, nothing under it.
    const bool intro = !message && !k.cur[0];
    const char* sung = message ? message : (intro ? k.next : k.cur);
    if (!message && k.prev[0]) drawLine(t, k.prev, Face::LINE, PREV_Y, s_c.prev);

    constexpr size_t MAX_ITEMS = lyricmsg::TEXT_CAP;   // one item per byte at most; +3 for "..."
    if (!s_items) s_items = (glyphrun::Item*)mem::bigAlloc((MAX_ITEMS + 3) * sizeof(glyphrun::Item));
    int bottom = ROW1_Y;
    if (s_items && sung[0]) {
        layOut(sung, MAX_ITEMS);
        // A message reads in the lit tone at once; a lyric line starts unsung and gets swept.
        for (int r = 0; r < s_rows.n; ++r) drawRow(t, r, message ? s_c.lit : s_c.unsung);
        bottom = rowY(s_rows.n);
        if (!message)
            for (int r = 0; r < s_rows.n; ++r)
                s_w[r] = glyphrun::width(s_items + s_rows.start[r], s_rows.end[r] - s_rows.start[r]);
        if (message) s_rows.n = 0;   // nothing to sweep
    }
    if (!message && !intro && k.next[0]) {
        const int y = min(bottom + NEXT_GAP, t.height() - lineH);
        drawLine(t, k.next, Face::LINE, y, s_c.next);
    }
}

void drawKaraokeBar(TFT_eSPI& t, const AppState& st) {
    const float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    const int fw = (int)(frac > 1.0f ? BAR_W : frac * BAR_W);
    // the played part solid; the rest empty, a thin line through its middle (the yellow deck's bar)
    t.fillRect(BAR_X, BAR_Y, fw, BAR_H, s_c.barOn);
    t.fillRect(BAR_X + fw, BAR_Y, BAR_W - fw, BAR_H, s_c.bg);
    if (fw < BAR_W) t.drawFastHLine(BAR_X + fw, BAR_Y + BAR_H / 2, BAR_W - fw, s_c.barOff);
}

void drawKaraoke(TFT_eSPI& t, const AppState& st, const shared::KaraokeLines& k, const char* message,
                 bool whole) {
    if (whole) {
        t.fillScreen(s_c.bg);
        // the link tags at the row's end, then the artist left of them, the song left of that
        const int tags = tagsW() + SONG_GAP;
        const int artistW = st.artist[0] ? drawLine(t, st.artist, Face::LINE, SONG_Y, s_c.artist, true,
                                                    ARTIST_MAX_W - tags, true, TEXT_X + TEXT_W - tags) + SONG_GAP
                                         : 0;
        drawLine(t, st.trackName, Face::LINE, SONG_Y, s_c.song, true, TEXT_W - tags - artistW);
        drawTags(t);
        drawKaraokeBar(t, st);
        s_walk = {0, 0, 0, 0};
    }
    drawLines(t, k, message);
}

int karaokeLineW() { return s_w[0] + s_w[1] + s_w[2]; }

void drawKaraokeSweep(TFT_eSPI& t, int px) {
    const int total = karaokeLineW();
    if (!total) return;
    if (px > total) px = total;
    if (px < s_lit) {   // the song went back inside the line: unsung again, sweep from the start
        for (int r = 0; r < s_rows.n; ++r) drawRow(t, r, s_c.unsung);
        s_lit = 0;
    }
    if (px <= s_lit) return;
    int rowStart = 0;   // the row's first px in the whole line's sweep
    for (int r = 0; r < s_rows.n; ++r) {
        const int a = max(s_lit, rowStart), b = min(px, rowStart + s_w[r]);
        if (b > a) litBand(t, r, a - rowStart, b - rowStart);
        rowStart += s_w[r];
    }
    s_lit = px;
}

void drawKaraokeWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step) {
    const int w = walkerWidth();
    if (!w) return;
    const float frac = st.durationMs ? (float)st.progressMs / (float)st.durationMs : 0.0f;
    blitWalker(t, interp::walkX(frac, BAR_X, BAR_W, w), BAR_Y, animMs, step, s_c.bg, s_walk);
}
}  // namespace ui
