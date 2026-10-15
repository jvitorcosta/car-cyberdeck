#include "dashboard.h"
#include "avatarview.h"
#include "../util/menunav.h"
#include <cstring>
#include "../ui/textdraw.h"
#include "../ui/theme.h"
#include "../util/glyphrun.h"
#include "../util/lyricclock.h"

namespace dashboard {
static constexpr int W = 320, H = 240;
static char s_big[160] = "\x01", s_small[160] = "\x01", s_status[64] = "\x01";

void invalidate() { s_big[0] = s_small[0] = s_status[0] = '\x01'; }

// Big line in the column [x0, W): font 2 at 2x over up to two rows when the whole line fits,
// else at 1x over two rows (glyphrun::wrapTwo, "..." if even that is too long). Centred.
static void bigLine(TFT_eSPI& t, const char* text, int x0, int cy) {
    constexpr size_t MAX = 160;
    static glyphrun::Item items[MAX + 3];   // + room for a "..." on the second row
    const size_t n = glyphrun::decode(text, items, MAX, ui::asciiWidth2, ui::wideWidth, &t);
    const int colW = W - x0 - 24, cx = x0 + (W - x0) / 2;
    const uint16_t fg = theme::pal().topText, sh = TFT_BLACK;
    const int dot = ui::asciiWidth2('.', &t);
    int scale = 2;
    glyphrun::Wrap w = glyphrun::wrapTwo(items, n, colW / 2, 3 * dot);   // items keep 1x widths
    if (w.bEllipsis) {
        scale = 1;
        w = glyphrun::wrapTwo(items, n, colW, 3 * dot);
    }
    const int dy = scale == 2 ? 18 : 9;
    auto run = [&](const glyphrun::Item* it, size_t k, int y) {
        if (scale == 2) ui::drawRunScaled(t, it, k, cx, y, fg, sh, MC_DATUM, 2);
        else ui::drawRun(t, it, k, cx, y, fg, sh, MC_DATUM);
    };
    if (w.bStart >= w.bEnd) {
        run(items, w.aEnd, cy);
        return;
    }
    run(items, w.aEnd, cy - dy);
    size_t bn = w.bEnd - w.bStart;
    if (w.bEllipsis) {   // items past bEnd are not drawn, so "..." can overwrite them
        for (int k = 0; k < 3; ++k)
            items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.', (uint8_t)dot};
        bn += 3;
    }
    run(items + w.bStart, bn, cy + dy);
}

static void status(TFT_eSPI& t, const char* text) {
    if (strcmp(text, s_status) == 0) return;
    strncpy(s_status, text, sizeof s_status - 1);
    t.fillRect(0, H - 22, W, 22, theme::pal().top);
    if (text[0]) ui::drawText(t, text, W / 2, H - 11, theme::pal().iconOff, TFT_BLACK, MC_DATUM, W - 16);
}

static void lyrics(TFT_eSPI& t, const char* big, const char* small) {
    if (strcmp(big, s_big) == 0 && strcmp(small, s_small) == 0) return;
    strncpy(s_big, big, sizeof s_big - 1);
    strncpy(s_small, small, sizeof s_small - 1);
    const int x0 = avatarview::on() ? 104 : 0;   // she keeps the left side
    t.fillRect(x0, 0, W - x0, avatarview::on() ? menunav::BAR.y - 2 : H - 22, theme::pal().top);
    bigLine(t, big, x0, 92);
    if (small[0])
        ui::drawText(t, small, x0 + (W - x0) / 2, 168, theme::pal().iconOff, TFT_BLACK, MC_DATUM,
                     W - x0 - 16);
}

void draw(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs) {
    const bool full = !avatarview::on();     // no avatar: today's screen, with its status strip
    if (v.wrongKey) {
        lyrics(t, "Wrong key", "REMOTE_TOKEN must match on both decks");
        if (full) status(t, "");
    } else if (!v.polledAtMs) {
        lyrics(t, "Looking for the deck...", "Is the new deck on, on the same hotspot?");
        if (full) status(t, "");
    } else {
        const lyricclock::View lv = lyricclock::pick(v.deck, nowMs - v.polledAtMs);
        lyrics(t, lv.big, lv.small);
        if (full) status(t, v.deckOk ? "" : "Looking for the deck...");
    }
}
}
