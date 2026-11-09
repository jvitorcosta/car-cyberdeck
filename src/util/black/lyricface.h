#pragma once
#include <cstdint>
// The karaoke view's lettering: the yellow deck's VT323 (tools/fonts, SIL OFL 1.1) at its four sizes,
// so the two decks match, as Adafruit-GFX tables, Latin-1 0x20-0xFF (the accented letters drawn
// whole). The tables live in src/black/fonts (tools/gfxfont.py, --ns lyricface); the view hands them
// to TFT_eSPI's setFreeFont, so Glyph and Font are GFXglyph and GFXfont field for field. PURE,
// host-tested.
namespace lyricface {

struct Glyph {                  // = GFXglyph
    uint32_t bitmapOffset;
    uint8_t width, height;
    uint8_t xAdvance;
    int8_t xOffset, yOffset;    // from the pen on the baseline
};
struct Font {                   // = GFXfont
    const uint8_t* bitmap;
    const Glyph* glyph;
    uint16_t first, last;
    uint8_t yAdvance;           // the line height
};

// SUNG the line being sung (36 px, up to rows() rows), SUNG_SMALL the same when it would not fit
// (24 px), LINE the song title and the lines before and after (19 px), SMALL the artist (17 px).
enum class Face : uint8_t { SUNG, SUNG_SMALL, LINE, SMALL };

const Font& font(Face f);
// The pen's advance for a Latin-1 code; 0 outside the font.
int advance(Face f, uint8_t latin1);
// A row's height: the font's line height, or more where its accents and descenders need it.
int lineHeight(Face f);
// The rows the sung line may take in a face: 3 in SUNG and SUNG_SMALL; 1 otherwise.
int rows(Face f);
// Rows from a line's top to its baseline: the highest ink in the font (Á's mark), so a row's clear
// and the sweep's clip hold its accents.
int ascent(Face f);

}  // namespace lyricface
