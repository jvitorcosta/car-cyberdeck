// Copied from black/ui/screen_now.cpp and laid out for 480x272 (nowscreen.h).
#include "nowscreen.h"
#include <cstring>
#include "../board/canvas.h"
#include "../../black/core/mem.h"
#include "../../black/core/shared.h"
#include "../../black/images/walksprite.h"
#include "../../black/ui/battle.h"
#include "../../black/ui/icons.h"
#include "../../black/ui/labels.h"
#include "../../black/ui/pokeball.h"
#include "../../black/ui/textdraw.h"
#include "../../black/ui/theme.h"
#include "../../black/ui/typebadge.h"
#include "../../util/black/genre.h"
#include "../../util/black/genrebadge.h"
#include "../../util/black/glyphrun.h"
#include "../../util/black/interp.h"
#include "../../util/black/walkanim.h"
#include "../../util/black/walkrect.h"
#include "../../util/common/text.h"

namespace pokeui {
using ui::Tab;

// ---- layout (480x272 landscape) ----
static constexpr int W = 480;
static constexpr int ART_BOX = 124;                                   // at 8,28
static constexpr int INFO_X = 142, INFO_Y = 28, INFO_W = 330, INFO_H = 124;
static constexpr int STAT_X = 8, STAT_Y = 154, STAT_W = 464, STAT_H = 72;
// The walker (32 px) stands on the bar: its top, BAR_Y - 33, stays under the name row.
static constexpr int BAR_X = 28, BAR_Y = 210, BAR_W = 432, BAR_H = 10;   // incl. "HP" tag
static constexpr int FILL_X = BAR_X + ui::HP_TAG_W + 1, FILL_W = BAR_W - ui::HP_TAG_W - 2;
static constexpr int EXP_Y = 221;
static constexpr int DLG_X = 6, DLG_Y = 228, DLG_W = 468, DLG_H = 42;
static constexpr int CD_CY = 12;
static constexpr int NAME_Y = 158, TIME_RIGHT = 456, TIME_CLEAR_X = 330;
static constexpr int BADGE_W = 76, BADGE_H = 18;

static int s_cdX = 100;
static int s_ballX = -1;
static int s_ballFrame = 0;
static char s_lastLyric[160] = "\x01";
static bool s_lastNotes = true;

void drawCdFrame(TFT_eSPI& t, int frame) { ui::drawCd(t, s_cdX, CD_CY, frame, theme::pal().top); }

void drawTopStrip(TFT_eSPI& t, const AppState& st) {
    t.fillRect(0, 0, W, 24, theme::pal().top);
    const char* title = ui::topTitle(st.isPlaying);
    ui::shadowText(t, title, 8, 4, 2, theme::pal().topText, theme::pal().boxBorder, TL_DATUM);
    s_cdX = 8 + t.textWidth(title, 2) + 12;
    drawCdFrame(t, 0);

    const int right = W - 8;
    int nameW = ui::drawText(t, st.deviceName[0] ? st.deviceName : "device", right, 4, theme::pal().topText,
                             theme::pal().boxBorder, TR_DATUM, 160);
    int devX = right - nameW - 6 - icons::SIZE;
    ui::drawIcon(t, icons::forDevice(st.deviceType), devX, 6, theme::pal().topText, theme::pal().top);
    int repX = devX - 22, shufX = repX - 20;
    ui::drawIcon(t, icons::Icon::Repeat, repX, 6, st.repeat ? theme::pal().topText : theme::pal().iconOff,
                 theme::pal().top);
    if (st.repeat == 2) {   // repeat-one: tiny "1"
        t.setTextColor(theme::HP_TAG_TEXT);
        t.drawString("1", repX + icons::SIZE, 11, 1);
    }
    ui::drawIcon(t, icons::Icon::Shuffle, shufX, 6, st.shuffle ? theme::pal().topText : theme::pal().iconOff,
                 theme::pal().top);
}

static void drawBall(TFT_eSPI& t) {
    if (s_ballX < 0) return;
    for (int y = 0; y < pokeball::SIZE; ++y)
        for (int x = 0; x < pokeball::SIZE; ++x) {
            uint16_t c;
            switch (pokeball::pixel(x, y, s_ballFrame)) {
                case pokeball::Px::Red:   c = theme::POKE_RED; break;
                case pokeball::Px::White: c = TFT_WHITE; break;
                case pokeball::Px::Dark:  c = theme::BALL_DARK; break;
                default:                  c = theme::pal().boxFill; break;
            }
            t.drawPixel(s_ballX + x, NAME_Y + 1 + y, c);   // into the frame: pushImage isn't virtual
        }
}

void drawPokeballFrame(TFT_eSPI& t, int frame) {
    s_ballFrame = frame;
    drawBall(t);
}

void drawProgressRegion(TFT_eSPI& t, const AppState& st) {
    const uint32_t rem = st.durationMs > st.progressMs ? st.durationMs - st.progressMs : 0;
    const uint32_t tot = st.durationMs;
    const float hpFrac = tot ? static_cast<float>(rem) / static_cast<float>(tot) : 1.0f;
    char tbuf[24];
    snprintf(tbuf, sizeof tbuf, "%lu:%02lu/%lu:%02lu", static_cast<unsigned long>(rem / 60000),
             static_cast<unsigned long>((rem / 1000) % 60), static_cast<unsigned long>(tot / 60000),
             static_cast<unsigned long>((tot / 1000) % 60));
    t.fillRect(TIME_CLEAR_X, NAME_Y, TIME_RIGHT + 2 - TIME_CLEAR_X, 17, theme::pal().boxFill);
    ui::shadowText(t, tbuf, TIME_RIGHT, NAME_Y, 2, theme::pal().text, theme::pal().textShadow, TR_DATUM);
    s_ballX = TIME_RIGHT - t.textWidth(tbuf, 2) - 4 - pokeball::SIZE;
    if (s_ballX < TIME_CLEAR_X) s_ballX = -1;
    drawBall(t);
    ui::hpBarBattle(t, BAR_X, BAR_Y, BAR_W, BAR_H, hpFrac);
    ui::expBar(t, FILL_X, EXP_Y, FILL_W, static_cast<float>(st.volume) / 100.0f);
}

// Gen-3 summary-style badge: rounded box in the colour, darker border, white label.
static void drawBadge(TFT_eSPI& t, int x, int y, const char* label, uint16_t c) {
    const uint16_t edge = theme::darken(c);
    t.fillRoundRect(x, y, BADGE_W, BADGE_H, 4, c);
    t.drawRoundRect(x, y, BADGE_W, BADGE_H, 4, edge);
    ui::shadowText(t, label, x + BADGE_W / 2, y + BADGE_H / 2, 2, TFT_WHITE, edge, MC_DATUM);
}

// `text` at twice the size on one or two lines of maxW pixels from x, y; "..." past the second.
static void drawTitle(TFT_eSPI& t, const char* text, int x, int y, int maxW) {
    constexpr size_t MAX_ITEMS = 96;
    glyphrun::Item items[MAX_ITEMS + 3];
    const size_t n = glyphrun::decode(text, items, MAX_ITEMS, ui::asciiWidth2, ui::wideWidth, &t, true,
                                      ui::emojiWidth, ui::emojiSeq);
    if (!n) return;
    const int dot = ui::asciiWidth2('.', &t);
    const glyphrun::Wrap w = glyphrun::wrapTwo(items, n, maxW / 2, 3 * dot);
    ui::drawRunScaled(t, items, w.aEnd, x, y, theme::pal().text, theme::pal().textShadow, TL_DATUM, 2);
    if (w.bStart >= w.bEnd) return;
    size_t bn = w.bEnd - w.bStart;
    if (w.bEllipsis) {   // items past bEnd aren't drawn, so "..." can take their place
        for (int k = 0; k < 3; ++k)
            items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.', static_cast<uint8_t>(dot)};
        bn += 3;
    }
    ui::drawRunScaled(t, items + w.bStart, bn, x, y + 32, theme::pal().text, theme::pal().textShadow, TL_DATUM, 2);
}

void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre) {
    const int x0 = INFO_X + 12, y = INFO_Y + 8, tw = INFO_W - 36;
    t.fillRect(x0, y - 1, tw, BADGE_H + 2, theme::pal().boxFill);
    int x = x0, maxW = tw;
    if (genre != genre::NONE && genre < genrebadge::count()) {
        const genrebadge::Badge& b = genrebadge::at(genre);
        drawBadge(t, x, y, b.label, b.color);
        x += BADGE_W + 8;
        maxW -= BADGE_W + 8;
    }
    ui::drawText(t, st.artist[0] ? st.artist : "Artist", x, y + 1, theme::pal().text, theme::pal().textShadow,
                 TL_DATUM, maxW);
}

static void drawNotes(TFT_eSPI& t) {
    const int y = DLG_Y + (DLG_H - icons::SIZE) / 2;
    ui::drawIcon(t, icons::Icon::Note, DLG_X + 10, y, theme::pal().note, theme::pal().dlgFill);
    ui::drawIcon(t, icons::Icon::Note, DLG_X + DLG_W - 10 - icons::SIZE, y, theme::pal().note,
                 theme::pal().dlgFill);
}

static void paintLyric(TFT_eSPI& t, bool notes);

bool drawLyricArea(TFT_eSPI& t, const char* text, bool notes) {
    const char* line = text ? text : "";
    if (notes == s_lastNotes && strncmp(s_lastLyric, line, sizeof s_lastLyric) == 0) return false;
    s_lastNotes = notes;
    txt::copy(s_lastLyric, line, sizeof s_lastLyric);
    paintLyric(t, notes);
    return true;
}

static void paintLyric(TFT_eSPI& t, bool notes) {
    t.fillRect(DLG_X + 5, DLG_Y + 5, DLG_W - 10, DLG_H - 10, theme::pal().dlgFill);
    constexpr size_t MAX_ITEMS = 160;   // one item per byte at most
    static glyphrun::Item items[MAX_ITEMS + 3];
    const size_t n = glyphrun::decode(s_lastLyric, items, MAX_ITEMS, ui::asciiWidth2, ui::wideWidth, &t, false,
                                      ui::emojiWidth, ui::emojiSeq);
    if (!n) return;
    if (notes) drawNotes(t);
    const int textW = DLG_W - 2 * 34, cx = DLG_X + DLG_W / 2;
    if (glyphrun::width(items, n) * 2 <= textW) {
        ui::drawRunScaled(t, items, n, cx, DLG_Y + DLG_H / 2, theme::pal().text, theme::pal().dlgShadow,
                          MC_DATUM, 2);
        return;
    }
    const int dot = ui::asciiWidth2('.', &t);
    const glyphrun::Wrap w = glyphrun::wrapTwo(items, n, textW, 3 * dot);
    if (w.bStart >= w.bEnd) {
        ui::drawRun(t, items, w.aEnd, cx, DLG_Y + DLG_H / 2, theme::pal().text, theme::pal().dlgShadow, MC_DATUM);
        return;
    }
    ui::drawRun(t, items, w.aEnd, cx, DLG_Y + 13, theme::pal().text, theme::pal().dlgShadow, MC_DATUM);
    size_t bn = w.bEnd - w.bStart;
    if (w.bEllipsis) {
        for (int k = 0; k < 3; ++k)
            items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.', static_cast<uint8_t>(dot)};
        bn += 3;
    }
    ui::drawRun(t, items + w.bStart, bn, cx, DLG_Y + 29, theme::pal().text, theme::pal().dlgShadow, MC_DATUM);
}

// The walker's last rect on screen (walkrect); a fresh screen has none.
static walkrect::Rect s_walk{0, 0, 0, 0};

void drawWalker(const AppState& st, uint32_t animMs, int step) {
    static constexpr int SLACK = 8;
    static uint16_t* buf = static_cast<uint16_t*>(
        mem::bigAlloc((walk::MAX_W + 2 * SLACK) * (walk::BAND_H + 1 + 2 * SLACK) * sizeof(uint16_t)));
    if (!buf) return;
    // The network task may promote() a new walker at any time; read frames under the lock.
    shared::Guard lockWalker;
    const walk::Info& wi = walk::info();
    if (!wi.ready) return;
    int fr = 0;
    if (wi.pmd) {
        uint16_t d[walk::MAX_FRAMES];
        for (int i = 0; i < wi.frames; ++i) d[i] = walk::durationMs(i);
        fr = walkanim::frameAt(animMs, d, wi.frames);
    }
    const uint16_t* spr = walk::pixels(fr);
    const uint8_t* msk = walk::mask(fr);
    const int w = wi.w, h = wi.h;
    const int bob = (!wi.pmd && (step % 2)) ? 1 : 0;   // fallback fake-walk only
    const bool mirror = !wi.pmd && ((step / 4) % 2);
    const float frac = st.durationMs ? static_cast<float>(st.progressMs) / static_cast<float>(st.durationMs) : 0.0f;
    const int cx = interp::walkX(frac, FILL_X, FILL_W, w);   // drained / remaining boundary
    const int x0 = cx - w / 2, y0 = BAR_Y - h - bob;          // feet on the bar's top
    const walkrect::Plan p = walkrect::plan(s_walk, {x0, BAR_Y - h - 1, w, h + 1}, SLACK);
    const uint16_t fill = theme::pal().boxFill;
    if (p.clearPrev) canvas::screen().fillRect(s_walk.x, s_walk.y, s_walk.w, s_walk.h, fill);
    const walkrect::Rect& r = p.push;
    const uint16_t fillBE = theme::be(fill);
    for (int i = 0; i < r.w * r.h; ++i) buf[i] = fillBE;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int sx = mirror ? (w - 1 - x) : x;
            if (!msk[y * w + sx]) continue;
            const int bx = x0 + x - r.x, by = y0 + y - r.y;
            if (bx >= 0 && bx < r.w && by >= 0 && by < r.h) buf[by * r.w + bx] = spr[y * w + sx];
        }
    canvas::blit(r.x, r.y, r.w, r.h, buf);
    s_walk = p.next;
}

void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre) {
    s_walk = {0, 0, 0, 0};
    ui::background(t);
    drawTopStrip(t, st);

    ui::battleBox(t, 8, INFO_Y, ART_BOX, ART_BOX, Tab::None);
    t.fillRect(ART_X, ART_Y, ART_W, ART_H, theme::pal().sky);

    ui::battleBox(t, INFO_X, INFO_Y, INFO_W, INFO_H, Tab::Right);
    const int tw = INFO_W - 36;
    drawArtistRow(t, st, genre);
    drawTitle(t, st.trackName[0] ? st.trackName : "Track title", INFO_X + 12, INFO_Y + 32, tw);
    char from[96];
    snprintf(from, sizeof from, "From: %s", st.context[0] ? st.context : "Playlist");
    ui::drawText(t, from, INFO_X + 12, INFO_Y + 102, theme::pal().text, theme::pal().textShadow, TL_DATUM, tw);

    ui::battleBox(t, STAT_X, STAT_Y, STAT_W, STAT_H, Tab::Left);
    int x = 28;
    char lbl[12];
    if (typebadge::label(st.pokeType, lbl, sizeof lbl)) {
        drawBadge(t, x, NAME_Y - 1, lbl, theme::typeColor(st.pokeType));
        x += BADGE_W + 8;
    }
    const int nameW = ui::drawText(t, st.pokeName[0] ? st.pokeName : "Pokemon", x, NAME_Y, theme::pal().text,
                                   theme::pal().textShadow, TL_DATUM, 140, true);
    x += nameW + 8;
    if (st.pokedexNum > 0) {
        char no[16];
        snprintf(no, sizeof no, "No.%04d", st.pokedexNum);
        ui::shadowText(t, no, x, NAME_Y + 5, 1, theme::pal().text, theme::pal().textShadow, TL_DATUM);
    }
    drawProgressRegion(t, st);

    ui::dialogueBox(t, DLG_X, DLG_Y, DLG_W, DLG_H);
    s_lastLyric[0] = '\x01';   // the box is fresh: the next line is drawn
    s_lastLyric[1] = '\0';
}
}  // namespace pokeui
