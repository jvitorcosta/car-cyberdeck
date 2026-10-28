#include "gojiraname.h"

#include "../fonts/gojira16.h"

namespace gojiraname {

const uint8_t* glyph(int i) { return gojira16::GLYPHS[i < 0 ? 0 : i >= N_GLYPHS ? N_GLYPHS - 1 : i]; }

Rows rows(termfont::Face f) {
    if (termfont::capHeight(f) >= INK_LAST - INK_FIRST + 1) return {INK_FIRST, INK_LAST};
    return {INK_FIRST + 1, INK_LAST - 1};
}

int top(termfont::Face f, int baseline) {
    const Rows r = rows(f);
    return baseline - (r.last - r.first + 1);
}

namespace {
// One character's advance, or its width when it ends the text (termfont::width's rule).
int one(termfont::Face f, char c, bool last) {
    if (c == MARK) return WIDTH;
    const char s[2] = {c, 0};
    return last ? termfont::width(f, s) : termfont::advance(f, s);
}
}  // namespace

int advance(termfont::Face f, const char* s) {
    int x = 0;
    for (const char* p = s; *p; p++) x += one(f, *p, false);
    return x;
}

int width(termfont::Face f, const char* s) {
    int x = 0;
    for (const char* p = s; *p; p++) x += one(f, *p, p[1] == 0);
    return x;
}

}  // namespace gojiraname
