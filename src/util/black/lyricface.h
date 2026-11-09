#pragma once
#include <cstdint>
// The karaoke view's lettering: Atkinson Hyperlegible Bold (made for legibility; tools/fonts) at four
// sizes as Adafruit-GFX tables, Latin-1 0x20-0xFF (the accented letters drawn whole). The tables live
// in src/black/fonts (tools/gfxfont.py, --ns lyricface); the view hands them to TFT_eSPI's
// setFreeFont, so Glyph and Font are GFXglyph and GFXfont field for field. PURE, host-tested.
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

// SUNG the line being sung (22 px), SUNG_SMALL the same when it would not fit three rows (18 px),
// LINE the song title and the lines before and after (14 px), SMALL the artist (12 px).
enum class Face : uint8_t { SUNG, SUNG_SMALL, LINE, SMALL };

const Font& font(Face f);
// The pen's advance for a Latin-1 code; 0 outside the font.
int advance(Face f, uint8_t latin1);
int lineHeight(Face f);
// Rows from a line's top to its baseline: the highest ink in the font (Á's mark), so a row's clear
// and the sweep's clip hold its accents.
int ascent(Face f);

}  // namespace lyricface
