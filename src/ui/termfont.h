#pragma once
// The yellow deck's terminal lettering: VT323 (SIL OFL 1.1, tools/fonts/) at four sizes, named by
// their role on screen rather than their pixel size, and the measuring the layout needs. The fonts
// are Adafruit-GFX tables (src/fonts/, made by tools/gfxfont.py) that TFT_eSPI's setFreeFont draws;
// Glyph / Font below have GFXglyph / GFXfont's exact layout (ledpanel.cpp static_asserts it), so
// they build on the host too, where TFT_eSPI isn't. PURE, host-tested.
#include <cstdint>

namespace termfont {

struct Glyph {                  // = GFXglyph
    uint32_t bitmapOffset;      // into Font::bitmap
    uint8_t width, height;      // the bitmap, cropped to the ink
    uint8_t xAdvance;           // pen step to the next character
    int8_t xOffset, yOffset;    // the bitmap's top left from the pen (on the baseline; y up is negative)
};

struct Font {                   // = GFXfont
    const uint8_t* bitmap;
    const Glyph* glyph;
    uint16_t first, last;       // the characters covered (0x20..0x7E)
    uint8_t yAdvance;           // line height
};

// SMALL 17 px: the boot lines, the dashboard's header, date, weather words, HI / LO, LIGHTS label and
// strip (trip, messages), the KARAOKE item's LIVE tag, small labels (the Lights colour names, the
// Lights status line);
// BODY 19 px: buttons and the Menu items, tabs, < BACK, INSIDE / OUTSIDE, the Lights preset names,
// the KARAOKE song row, the next lyric; TITLE 24 px: the lights band's colour names and levels, the
// Lights level %, [-] / [+]; BIG 36 px: the clock, the temperature, the lyric line.
// VT323 is drawn on a pixel grid and breaks up below 17 px. At 24 and 36 px every stroke is an even
// 2 / 3 px; 17 and 19 px are off its grid and keep some uneven stems (a 1 px stem next to a 2 px
// one), the smallest sizes that still read cleanly: judged on the device.
enum class Face : uint8_t { SMALL, BODY, TITLE, BIG };
constexpr int N_FACES = 4;

const Font& font(Face f);

// Rows of 'H' above the baseline: what centring and "caps top at y" go by (the text is upper case).
int capHeight(Face f);
// The deepest rows any glyph reaches above / below the baseline ('(' '/' '[' 'Q' 'g' go past the
// caps): what a clear must cover.
int ascent(Face f);
int descent(Face f);
// One step of the (monospace) pen: the width of a cell, e.g. the block cursor.
int cell(Face f);

// The pen's travel over s (the sum of xAdvance): where the next text drawn after it starts, as
// TFT_eSPI's drawString returns it.
int advance(Face f, const char* s);
// The width TFT_eSPI's textWidth gives (and its datums align by): the advances up to the last
// character, then that one's ink (xOffset + width). Characters outside the font count nothing.
int width(const Font& f, const char* s);
int width(Face f, const char* s);
// width() as karaoke::wrap wants it: `face` is a Face as int.
int widthOf(const char* s, int face);

}  // namespace termfont
