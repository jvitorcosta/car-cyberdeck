# tools/gen_suki_ride.py
"""Build data/sukiride.bin: the art of the Suki drive screen (R1.1) on the black deck's GAUGES screens
(black/ui/sukirideview, util/car/sukiride). Run:
  python tools/gen_suki_ride.py --assets <dir> [--font Orbitron.ttf] [--out data/sukiride.bin]
                                [--preview out.png]

<dir> holds the layers the research session cut from the film monitor's making-of video:
suki-layer-backdrop.png (640 x 365: the logo, the pink curve, the bezel, the three silver ribbons,
their insides cleared) and suki-layer-cel-<second>.png (253 x 365 each: the girl, the right-hand strip
of the same art, camera still). The deck draws everything at half size (320 x 182, letterboxed) and
recolours the art per theme on the device, so the art is stored ONCE: 8-bit indices into one 256-colour
palette (its original colours). The film's art is not ours to publish: the layers and data/sukiride.bin
are git-ignored, and the repo only carries this tool.

Fonts: alpha-only glyphs (the deck picks the colour). Orbitron is the mock's face; without --font the
tool uses Century Gothic Bold from Windows (the old Suki look's numerals).
Format "SKR1": src/util/car/sukiride.h (keep the constants below in step with it). Needs Pillow.
"""
import argparse
import glob
import os
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC_W, SRC_H, SRC_CEL_X = 640, 365, 387
W, H = 320, 182            # the art at device size
CEL_X, CEL_W = 193, 127    # the girl's strip (SRC_CEL_X / 2, rounded down; to the right edge)
COLOURS = 256
# (size px, tracking, chars): the order of sukiride::FontId
CHARS_SMALL = "ABCDEHIKLMOPRSTV/%.-0123456789"
FONTS = [
    (8, 1, CHARS_SMALL),          # F_SMALL: labels, units, tags, KM/H, EST
    (12, 0, "0123456789.-"),      # F_VALUE: the ribbons' and chips' values
    (18, 0, "1234567-"),          # F_GEAR: the gear numeral
    (9, 0, "DSM"),                # F_MODE: the mode letter
]
DEFAULT_FONTS = ["C:/Windows/Fonts/GOTHICB.TTF"]


def cel_files(assets):
    """The cels in time order: [(second, path)]."""
    out = []
    for p in glob.glob(os.path.join(assets, "suki-layer-cel-*.png")):
        m = re.search(r"suki-layer-cel-([\d.]+)\.png$", p)
        if m:
            out.append((float(m.group(1)), p))
    return sorted(out)


def half(full):
    return full.resize((W, H), Image.LANCZOS)


def layers(assets):
    """(backdrop 320 x 182 RGB, [cel strip 127 x 182 RGB]) at device size. Each cel is composed onto
    the full backdrop first and halved as a whole, so the strip's left edge blends as it would."""
    bd = Image.open(os.path.join(assets, "suki-layer-backdrop.png")).convert("RGB")
    if bd.size != (SRC_W, SRC_H):
        sys.exit("gen_suki_ride: the backdrop is %dx%d, not %dx%d" % (bd.size + (SRC_W, SRC_H)))
    cels = []
    files = cel_files(assets)
    if not files:
        sys.exit("gen_suki_ride: no suki-layer-cel-*.png in " + assets)
    for _, p in files:
        c = Image.open(p).convert("RGB")
        if c.size != (SRC_W - SRC_CEL_X, SRC_H):
            sys.exit("gen_suki_ride: %s is %dx%d" % ((p,) + c.size))
        full = bd.copy()
        full.paste(c, (SRC_CEL_X, 0))
        cels.append(half(full).crop((CEL_X, 0, W, H)))
    # the backdrop carries the calm cel (index 7, the one held at a stop) under the strip
    full = bd.copy()
    full.paste(Image.open(files[min(7, len(files) - 1)][1]).convert("RGB"), (SRC_CEL_X, 0))
    return half(full), cels


def quantize(backdrop, cels):
    """One 256-colour palette over the backdrop and every cel: (palette [(r,g,b)] * 256, backdrop
    indices, [cel indices])."""
    sheet = Image.new("RGB", (W + CEL_W * len(cels), H))
    sheet.paste(backdrop, (0, 0))
    for i, c in enumerate(cels):
        sheet.paste(c, (W + i * CEL_W, 0))
    q = sheet.quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:COLOURS * 3]
    pal += [0] * (COLOURS * 3 - len(pal))
    palette = [tuple(pal[i * 3:i * 3 + 3]) for i in range(COLOURS)]
    idx = q.tobytes()
    sw = sheet.width

    def crop(x0, w):
        return b"".join(idx[y * sw + x0:y * sw + x0 + w] for y in range(H))

    return palette, crop(0, W), [crop(W + i * CEL_W, CEL_W) for i in range(len(cels))]


def glyphs(font_path, size, chars):
    """[(char, width, alpha bytes width*height)], cell height: each glyph on the font's own baseline
    in a cell `size` tall that starts at the cap top, anti-aliased by drawing 4x and box-filtering
    down; and the cap height."""
    big = ImageFont.truetype(font_path, size * 4)
    asc, desc = big.getmetrics()
    cell = size
    out = []
    hb = big.getbbox("H")
    cap = max(1, int(round((hb[3] - hb[1]) / 4.0)))
    for ch in chars:
        if ch == "-":   # a crisp bar at mid-cap: a hyphen this small blurs into a dot ("--" is "no value")
            w, t = max(3, int(round(size * 0.42))), max(2, int(round(size * 0.14)))
            y0 = (cap - t) // 2
            out.append((ch, w + 1, bytes(255 if y0 <= y < y0 + t and x < w else 0
                                         for y in range(cell) for x in range(w + 1))))
            continue
        draw = ch
        adv = big.getlength(draw)
        im = Image.new("L", (int(adv) + 8, asc + desc), 0)
        ImageDraw.Draw(im).text((0, 0), draw, font=big, fill=255)
        # caps sit at the cell's top: crop from the cap top of "H" so the cell is used well
        cap_top = big.getbbox("H")[1]
        im = im.crop((0, cap_top, im.width, cap_top + cell * 4))
        w = max(1, int(round(adv / 4.0)))
        im = im.resize((w * 4, cell * 4), Image.BOX).resize((w, cell), Image.BOX)
        out.append((ch, w, im.tobytes()))
    return out, cell, cap


def pack(palette, backdrop, cels, fonts):
    """fonts: [(cell height, tracking, cap height, [(char, width, alpha bytes)])] -> the SKR1 file's
    bytes."""
    head = b"SKR1" + struct.pack("<HHHHHH", W, H, CEL_X, CEL_W, len(cels), len(fonts))
    body = bytearray(head)
    for r, g, b in palette:
        body += bytes((r, g, b))
    assert len(backdrop) == W * H
    body += backdrop
    for c in cels:
        assert len(c) == CEL_W * H
        body += c
    fix = []
    for h, track, cap, gl in fonts:
        body += struct.pack("<BbBB", h, track, len(gl), cap)
        for ch, w, _ in gl:
            body += struct.pack("<BBH", ord(ch), w, 0)
            fix.append(len(body))
            body += b"\0\0\0\0"
    k = 0
    for h, _, _, gl in fonts:
        for _, w, px in gl:
            assert len(px) == w * h
            struct.pack_into("<I", body, fix[k], len(body))
            body += px
            k += 1
    return bytes(body)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--assets", required=True)
    ap.add_argument("--font", default=None, help="TTF for every font (default: Century Gothic Bold)")
    ap.add_argument("--out", default=os.path.join(ROOT, "data", "sukiride.bin"))
    ap.add_argument("--preview", default=None, help="write the indexed art back as a PNG to check it")
    a = ap.parse_args()
    font = a.font or next((f for f in DEFAULT_FONTS if os.path.isfile(f)), None)
    if not font:
        sys.exit("gen_suki_ride: no font: pass --font")
    backdrop, cels = layers(a.assets)
    palette, bidx, cidx = quantize(backdrop, cels)
    fonts = []
    for size, track, chars in FONTS:
        gl, cell, cap = glyphs(font, size, chars)
        fonts.append((cell, track, cap, gl))
    blob = pack(palette, bidx, cidx, fonts)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(blob)
    print("gen_suki_ride: %s, %d bytes (%d cels, font %s)" % (a.out, len(blob), len(cels), os.path.basename(font)))
    if a.preview:
        flat = [v for c in palette for v in c]
        im = Image.frombytes("P", (W, H), bidx)
        im.putpalette(flat)
        im.convert("RGB").save(a.preview)


if __name__ == "__main__":
    main()
