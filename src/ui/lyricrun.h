#pragma once
// The yellow deck's lyric text as a run of items: Latin-1 characters drawn in VT323 (ui/termfont) as
// before, and Japanese as Wide items from the Unifont subset (ui/jpfont). A CJK or Hangul character
// the subset lacks (Chinese-only hanzi, level 2 kanji, Korean) is a Box: an outline the size of a
// wide glyph. Wide glyphs are 16 px Unifont, drawn 2x in BIG (the lyric line) and 1x in every other
// face, centred on the face's caps. Word-wrapping breaks at spaces and between characters next to
// a Wide or Box item (Japanese has no spaces). PURE, host-tested.
#include <cstddef>
#include <cstdint>

#include "termfont.h"

namespace lyricrun {

enum class Kind : uint8_t { Latin1, Wide, Box };
// Latin1: cp is the Latin-1 byte (w unused: termfont measures it). Wide: cp the codepoint (the font
// is all in the BMP), w its glyph's width at 1x (16, or 8 for the halfwidth forms). Box: w = BOX_W
// (cp unused, 0: the codepoint may be past the BMP).
struct Item {
    uint16_t cp;
    Kind kind;
    uint8_t w;
};
constexpr int GLYPH_H = 16, BOX_W = 16;

// A codepoint's glyph width in the Japanese font (8 or 16), 0 when the font doesn't have it.
using GlyphWidth = int (*)(uint32_t cp, const void* ctx);

// A CJK or Hangul codepoint: one the font lacks still takes a cell, as a Box.
bool isCjk(uint32_t cp);

// UTF-8 -> items: each character txt::latin1Fold keeps (ASCII, Latin-1, the folded quotes, dashes,
// spaces, ellipsis, Œ...) gives its Latin-1 bytes, exactly as latin1Fold would; one it drops is a
// Wide item when gw has it, a Box when isCjk, else dropped (emoji). upper: Latin-1 upper-cased as
// txt::latin1Upper. Stops when cap items are used (a folded character goes in whole or not at all).
size_t decode(const char* utf8, Item* out, size_t cap, GlyphWidth gw, const void* ctx, bool upper = false);

// How many times a 16 px glyph is drawn in face f: 2 in BIG (32 px), else 1.
int scale(termfont::Face f);
// The pen's travel over one item in face f.
int advance(termfont::Face f, const Item& it);
// The width of it[0..n) (followed by ".." when dots) as termfont::width / TFT_eSPI's textWidth
// measure text: the advances, the last character counting its ink. For pure Latin-1 items it equals
// termfont::width of their bytes.
int width(termfont::Face f, const Item* it, size_t n, bool dots = false);
// The top row of a Wide / Box glyph for text on `baseline` in face f: the glyph (16 * scale rows)
// centred on the caps.
int glyphTop(termfont::Face f, int baseline);

// A wrapped line: items [start, end), then ".." when dots (the text was cut there).
struct Span {
    uint16_t start, end;
    bool dots;
};
// Word-wrapping, as karaoke's screen wants it: it[0..n) over up to maxLines lines of maxPx in face
// f, each as many words as fit (a Wide or Box item is a word of its own; a word wider than a line is
// broken), the last line cut with ".." when the rest doesn't fit (a space before the ".." goes).
// Returns the lines used (0 for an empty or blank text); *cut (optional) tells whether text was
// dropped. With maxLines 1 it fits a single line.
int wrap(const Item* it, size_t n, termfont::Face f, int maxPx, int maxLines, Span out[], bool* cut = nullptr);
// Whether a line holds a Wide or Box item.
bool hasWide(const Item* it, const Span& s);

}  // namespace lyricrun
