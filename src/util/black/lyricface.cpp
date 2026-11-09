#include "lyricface.h"
#include "../../black/fonts/vt323_36.h"
#include "../../black/fonts/vt323_24.h"
#include "../../black/fonts/vt323_19.h"
#include "../../black/fonts/vt323_17.h"

namespace lyricface {

const Font& font(Face f) {
    switch (f) {
    case Face::SUNG: return VT323_36;
    case Face::SUNG_SMALL: return VT323_24;
    case Face::LINE: return VT323_19;
    case Face::SMALL: break;
    }
    return VT323_17;
}

int advance(Face f, uint8_t latin1) {
    const Font& ft = font(f);
    if (latin1 < ft.first || latin1 > ft.last) return 0;
    return ft.glyph[latin1 - ft.first].xAdvance;
}

int lineHeight(Face f) {
    const Font& ft = font(f);
    int below = 0;   // the deepest ink under the baseline
    for (int c = ft.first; c <= ft.last; ++c) {
        const Glyph& g = ft.glyph[c - ft.first];
        if (g.height && g.yOffset + g.height > below) below = g.yOffset + g.height;
    }
    const int need = ascent(f) + below;
    return need > ft.yAdvance ? need : ft.yAdvance;
}

int rows(Face f) {
    if (f == Face::SUNG) return 2;
    return f == Face::SUNG_SMALL ? 3 : 1;
}

int ascent(Face f) {
    const Font& ft = font(f);
    int top = 0;   // the highest ink over the baseline: an accented capital's mark
    for (int c = ft.first; c <= ft.last; ++c)
        if (ft.glyph[c - ft.first].height && -ft.glyph[c - ft.first].yOffset > top) top = -ft.glyph[c - ft.first].yOffset;
    return top;
}

int capAscent(Face f) {
    const Font& ft = font(f);
    return -ft.glyph['H' - ft.first].yOffset;
}

}  // namespace lyricface
