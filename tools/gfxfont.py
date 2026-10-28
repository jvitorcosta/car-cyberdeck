"""TTF -> Adafruit-GFX font header (the GFXfont tables TFT_eSPI's setFreeFont draws), 1-bit.

Renders Latin-1 (0x20-0xFF: ASCII, then the accented letters and symbols such as the degree
sign) at one pixel size with no anti-aliasing, so a pixel font such as VT323 keeps its hard edges
on the panel, and writes the same tables Adafruit's
fontconvert does: each glyph's bitmap cropped to its ink, rows packed MSB-first and run on
bit by bit (a glyph starts on a byte boundary), glyphs back to back; xAdvance from the font's
advance width, x/yOffset from the pen position on the baseline, yAdvance the font's line
height (ascent + descent). GFXfont covers one run first..last, so the codes in it that are no
character (DEL and the C1 controls, 0x7F-0x9F) get an empty glyph that doesn't move the pen.

The types are written as `termfont::Glyph` / `termfont::Font` (src/yellow/ui/termfont.h: the same layout
as GFXglyph / GFXfont), so the header builds on the host too.

Usage (from the repo root; Pillow only):
    python tools/gfxfont.py tools/fonts/VT323-Regular.ttf 24 src/yellow/fonts/vt323_24.h VT323_24
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 0x20, 0xFF
CONTROLS = range(0x7F, 0xA0)  # DEL and the C1 controls: no character
LICENCE = "SIL OFL 1.1, see tools/fonts/VT323-OFL.txt"


def glyph(font, ch, ascent, descent):
    """(bits, width, height, xAdvance, xOffset, yOffset) of one character, offsets from the pen
    on the baseline (y up is negative, as in GFXglyph)."""
    adv = int(round(font.getlength(ch)))
    pad = 8 + font.size
    w = adv + 2 * pad
    h = ascent + descent + 2 * pad
    img = Image.new("1", (w, h), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = "1"  # no anti-aliasing
    base_y = pad + ascent
    draw.text((pad, base_y), ch, font=font, fill=1, anchor="ls")
    box = img.getbbox()
    if box is None:  # the space: no ink
        return [], 0, 0, adv, 0, 0
    x0, y0, x1, y1 = box
    bits = [1 if img.getpixel((x, y)) else 0 for y in range(y0, y1) for x in range(x0, x1)]
    return bits, x1 - x0, y1 - y0, adv, x0 - pad, y0 - base_y


def pack(bits):
    """MSB-first, rows run on (no per-row padding); the last byte padded with zeros."""
    out = []
    for i in range(0, len(bits), 8):
        chunk = bits[i:i + 8] + [0] * (8 - len(bits[i:i + 8]))
        out.append(sum(b << (7 - k) for k, b in enumerate(chunk)))
    return out


def convert(ttf, size, name):
    font = ImageFont.truetype(ttf, size)
    ascent, descent = font.getmetrics()
    bitmap, glyphs = [], []
    for code in range(FIRST, LAST + 1):
        if code in CONTROLS:
            bits, w, h, adv, xo, yo = [], 0, 0, 0, 0, 0
        else:
            bits, w, h, adv, xo, yo = glyph(font, chr(code), ascent, descent)
        for v, lo, hi in ((w, 0, 255), (h, 0, 255), (adv, 0, 255), (xo, -128, 127), (yo, -128, 127)):
            if not lo <= v <= hi:
                raise ValueError("glyph %r: %d does not fit the table" % (chr(code), v))
        glyphs.append((len(bitmap), w, h, adv, xo, yo, chr(code)))
        bitmap += pack(bits)
    return bitmap, glyphs, ascent + descent


def header(ttf, size, path, name):
    bitmap, glyphs, y_adv = convert(ttf, size, name)
    cmd = "python tools/gfxfont.py %s %d %s %s" % (ttf.replace("\\", "/"), size, path.replace("\\", "/"), name)
    lines = [
        "// %s at %d px: Adafruit-GFX tables, 1-bit, Latin-1 0x20-0xFF." % (os.path.basename(ttf), size),
        "// Font: VT323, Copyright 2011 The VT323 Project Authors (Google Fonts), %s." % LICENCE,
        "// Generated, do not edit:",
        "//   " + cmd,
        "#pragma once",
        '#include "../ui/termfont.h"',
        "",
        "static const uint8_t %sBitmaps[] = {" % name,
    ]
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in bitmap[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const termfont::Glyph %sGlyphs[] = {" % name)
    for off, w, h, adv, xo, yo, ch in glyphs:
        code = ord(ch)
        shown = ("space" if ch == " " else "0x%02X" % code if code in CONTROLS else "nbsp" if code == 0xA0
                 else "soft hyphen" if code == 0xAD else ch)
        lines.append("    {%d, %d, %d, %d, %d, %d},  // '%s'" % (off, w, h, adv, xo, yo, shown))
    lines.append("};")
    lines.append("")
    lines.append("static const termfont::Font %s = {%sBitmaps, %sGlyphs, 0x%02X, 0x%02X, %d};"
                 % (name, name, name, FIRST, LAST, y_adv))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    header(sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4])
