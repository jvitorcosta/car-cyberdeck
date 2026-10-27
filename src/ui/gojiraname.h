#pragma once
// GOJIRA's name on the yellow deck: ゴジラ in katakana, which VT323 (Latin-1) doesn't have. A name
// the screen shows holds ledpresets::GOJIRA_MARK where the three glyphs go (">\x01 APPLIED"); the
// text path measures the mark as WIDTH px and draws GLYPHS (16x16 Unifont bitmaps, fonts/gojira16.h,
// from tools/gen_gojira_glyphs.py) in its place, at 1x, sitting on the baseline like the caps. Their
// ink is rows 1..14 of the 16; a face whose caps are shorter than that (SMALL, BODY) gets rows 2..13,
// the 12 rows the Lights status line has (it loses ゴ's topmost dakuten pixel and the end of ラ's
// tail). PURE, host-tested.
#include <cstdint>

#include "../util/yellow/ledpresets.h"
#include "termfont.h"

namespace gojiraname {
constexpr char MARK = ledpresets::GOJIRA_MARK;
constexpr int N_GLYPHS = 3, GLYPH_W = 16, WIDTH = N_GLYPHS * GLYPH_W;
constexpr int INK_FIRST = 1, INK_LAST = 14;  // the glyphs' inked rows

// Glyph i (0..2: ゴ ジ ラ): 16 rows of 2 bytes, MSB the leftmost pixel.
const uint8_t* glyph(int i);

// The bitmap rows drawn in face f, first..last.
struct Rows {
    int first, last;
};
Rows rows(termfont::Face f);
// The screen row of rows(f).first for text on `baseline` (the row under the caps): the last drawn row
// sits on the caps' bottom row, baseline - 1.
int top(termfont::Face f, int baseline);

// termfont's advance / width over s, each MARK counting WIDTH.
int advance(termfont::Face f, const char* s);
int width(termfont::Face f, const char* s);
}  // namespace gojiraname
