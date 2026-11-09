#include "lyricface.h"
#include "../../black/fonts/atkb_22.h"
#include "../../black/fonts/atkb_18.h"
#include "../../black/fonts/atkb_14.h"
#include "../../black/fonts/atkb_12.h"

namespace lyricface {

const Font& font(Face f) {
    switch (f) {
    case Face::SUNG: return ATKB_22;
    case Face::SUNG_SMALL: return ATKB_18;
    case Face::LINE: return ATKB_14;
    case Face::SMALL: break;
    }
    return ATKB_12;
}

int advance(Face f, uint8_t latin1) {
    const Font& ft = font(f);
    if (latin1 < ft.first || latin1 > ft.last) return 0;
    return ft.glyph[latin1 - ft.first].xAdvance;
}

int lineHeight(Face f) { return font(f).yAdvance; }

int ascent(Face f) {
    const Font& ft = font(f);
    int top = 0;   // the highest ink over the baseline: an accented capital's mark
    for (int c = ft.first; c <= ft.last; ++c)
        if (ft.glyph[c - ft.first].height && -ft.glyph[c - ft.first].yOffset > top) top = -ft.glyph[c - ft.first].yOffset;
    return top;
}

}  // namespace lyricface
