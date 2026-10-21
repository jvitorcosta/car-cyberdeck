#include "carstrip.h"
#include <cstring>
#include "gauges.h"
#include "../ui/textdraw.h"
#include "../util/glyphrun.h"
#include "../util/lyricclock.h"
#include "../util/rgb565.h"
#include "../util/text.h"

namespace carstrip {
namespace {
using rgb565::hex;
constexpr uint16_t HI = hex(0x41FF6A), MID = hex(0x1D8A3A), BLACK = 0x0000;   // the cluster's
constexpr int X0 = 52, W = 320 - X0 - 4, ROW1 = gauges::STRIP_Y + 13, ROW2 = gauges::STRIP_Y + 33;
char s_shown[2 * 160 + 4] = "\x01";   // what it last drew: style + both rows

// Her line over the two rows (wrapped at a space, "..." if it still doesn't fit).
void twoRows(TFT_eSPI& t, const char* text, uint16_t fg, uint16_t bg) {
    constexpr size_t MAX = 160;
    static glyphrun::Item items[MAX + 3];
    const size_t n = glyphrun::decode(text, items, MAX, ui::asciiWidth2, ui::wideWidth, &t);
    const int dot = ui::asciiWidth2('.', &t);
    const glyphrun::Wrap w = glyphrun::wrapTwo(items, n, W, 3 * dot);
    ui::drawRun(t, items, w.aEnd, X0, ROW1, fg, bg, ML_DATUM);
    size_t bn = w.bEnd > w.bStart ? w.bEnd - w.bStart : 0;
    if (w.bEllipsis) {
        for (int k = 0; k < 3; ++k)
            items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.', (uint8_t)dot};
        bn += 3;
    }
    if (bn) ui::drawRun(t, items + w.bStart, bn, X0, ROW2, fg, bg, ML_DATUM);
}
}

void invalidate() { s_shown[0] = '\x01'; }

void draw(TFT_eSPI& t, const decklink::View& v, const char* herLine, bool warning, uint32_t nowMs) {
    const char* big = "";
    const char* small = "";
    char style = 'l';                                 // l lyric, h her line, w her warning
    if (herLine) {
        big = herLine;
        style = warning ? 'w' : 'h';
    } else if (v.wrongKey) {
        small = "Wrong key: REMOTE_TOKEN must match";
    } else if (!v.polledAtMs) {
        small = "Looking for the deck...";
    } else {
        const lyricclock::View lv = lyricclock::pick(v.deck, nowMs - v.polledAtMs);
        big = lv.big;
        small = lv.small;
    }
    char key[sizeof s_shown];
    snprintf(key, sizeof key, "%c%s\x02%s", style, big, small);
    if (strcmp(key, s_shown) == 0) return;
    txt::copy(s_shown, key, sizeof s_shown);

    const uint16_t bg = style == 'w' ? HI : BLACK;
    t.fillRect(X0 - 2, gauges::STRIP_Y + 1, 320 - X0 + 2, 240 - gauges::STRIP_Y - 1, bg);
    t.drawFastHLine(0, gauges::STRIP_Y, 320, MID);      // the strip's top edge
    if (style != 'l') {
        twoRows(t, big, style == 'w' ? BLACK : HI, bg);
        return;
    }
    if (big[0]) ui::drawText(t, big, X0, ROW1, HI, BLACK, ML_DATUM, W);
    if (small[0]) ui::drawText(t, small, X0, ROW2, MID, BLACK, ML_DATUM, W);
}
}
