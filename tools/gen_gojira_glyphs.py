"""Generate the ゴジラ (GOJIRA) name glyphs for the yellow deck and the Garmin watch.

Why: GOJIRA's name is shown in katakana, and neither the yellow deck's VT323 fonts (Latin-1) nor the
FR255's fonts have kana. The three glyphs come from the GNU Unifont blob the black deck already
ships (data/cjk16.bin, made by tools/gen_cjk_font.py; GPLv2+ with the font embedding exception, or
SIL OFL 1.1: see CREDITS.md), read with the blob's own format (src/black/ui/cjkfont's reader; the host
test test_gojiraname checks the header against it).
Run: python tools/gen_gojira_glyphs.py
Writes:
  src/yellow/fonts/gojira16.h                          the three 16x16 bitmaps (2 B/row, MSB left)
  garmin/carlights/resources/drawables/gojira_white.png the name, white on transparent, cropped to its ink
  garmin/carlights/resources/drawables/gojira_black.png the same in black (for a light fill under it)
"""
import os
import struct

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BLOB = os.path.join(ROOT, "data", "cjk16.bin")
HEADER = os.path.join(ROOT, "src", "yellow", "fonts", "gojira16.h")
DRAWABLES = os.path.join(ROOT, "garmin", "carlights", "resources", "drawables")
CODEPOINTS = [0x30B4, 0x30B8, 0x30E9]  # ゴ ジ ラ


def glyph(blob, cp):
    """The 32-byte 16x16 bitmap of cp (cjkfont::Font::glyph, for 16 px wide ranges)."""
    if blob[:4] != b"UFNT":
        raise SystemExit("not a UFNT blob: " + BLOB)
    n = struct.unpack_from("<H", blob, 4)[0]
    for i in range(n):
        first, last, width, offset = struct.unpack_from("<IIBI", blob, 6 + 13 * i)
        if first <= cp <= last:
            if width != 16:
                raise SystemExit("U+%04X is not 16 px wide" % cp)
            at = offset + (cp - first) * 32
            return blob[at:at + 32]
    raise SystemExit("U+%04X is not in the blob" % cp)


def pixels(glyphs):
    """(x, y) of every inked pixel of the name, the glyphs 16 px apart."""
    out = []
    for k, g in enumerate(glyphs):
        for row in range(16):
            bits = g[2 * row] << 8 | g[2 * row + 1]
            out += [(16 * k + col, row) for col in range(16) if bits >> (15 - col) & 1]
    return out


def write_header(glyphs):
    lines = [
        "#pragma once",
        "// ゴジラ (U+30B4 U+30B8 U+30E9), GOJIRA's name on the yellow deck: 16x16 1-bit bitmaps, 2 bytes per",
        "// row, MSB the leftmost pixel. From GNU Unifont 16.0.04 (GPLv2+ with the GNU font embedding exception,",
        "// or SIL OFL 1.1; CREDITS.md) via data/cjk16.bin, by tools/gen_gojira_glyphs.py. Do not edit.",
        "#include <cstdint>",
        "",
        "namespace gojira16 {",
        "inline constexpr uint8_t GLYPHS[3][32] = {",
    ]
    for cp, g in zip(CODEPOINTS, glyphs):
        rows = [", ".join("0x%02X" % b for b in g[h:h + 16]) for h in (0, 16)]
        lines += ["    {%s,  // U+%04X" % (rows[0], cp), "     %s}," % rows[1]]
    lines += ["};", "}  // namespace gojira16", ""]
    with open(HEADER, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def write_png(path, ink, pts):
    xs = [x for x, _ in pts]
    ys = [y for _, y in pts]
    x0, y0 = min(xs), min(ys)
    img = Image.new("RGBA", (max(xs) - x0 + 1, max(ys) - y0 + 1), (0, 0, 0, 0))
    for x, y in pts:
        img.putpixel((x - x0, y - y0), ink)
    img.save(path)
    return img.size


def main():
    with open(BLOB, "rb") as f:
        blob = f.read()
    glyphs = [glyph(blob, cp) for cp in CODEPOINTS]
    write_header(glyphs)
    pts = pixels(glyphs)
    size = write_png(os.path.join(DRAWABLES, "gojira_white.png"), (255, 255, 255, 255), pts)
    write_png(os.path.join(DRAWABLES, "gojira_black.png"), (0, 0, 0, 255), pts)
    print("wrote %s and the watch's %dx%d bitmaps" % (HEADER, size[0], size[1]))


if __name__ == "__main__":
    main()
