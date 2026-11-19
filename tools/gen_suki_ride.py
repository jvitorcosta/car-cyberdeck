# tools/gen_suki_ride.py
"""Build data/sukiride.bin: the art of the Suki drive screen (v19, full screen) on the black deck's GAUGES screens
(black/ui/sukirideview, util/car/sukiride). Run:
  python tools/gen_suki_ride.py --assets <dir> [--font Orbitron.ttf] [--out data/sukiride.bin]
                                [--preview out.png]

<dir> holds the layers the research session cut from the film monitor's making-of video:
suki-layer-backdrop.png (640 x 365: the logo, the pink curve, the bezel, the three silver ribbons,
their insides cleared) and suki-layer-cel-<second>.png (253 x 365 each: the girl, the right-hand strip
of the same art, camera still). The screen fills the deck: the art is scaled evenly to the full 240 px
(x 0.6575, the right edge kept) and only the girl's strip is stored, ART_W px from ART_X to the right
edge (the deck draws the left panel itself), plus the SUKI logo's letters as an alpha mask (the deck
tints them in the theme). The deck recolours the art per theme on the device, so it is stored ONCE:
8-bit indices into one 256-colour palette (its original colours). The film's art is not ours to publish: the layers and data/sukiride.bin
are git-ignored, and the repo only carries this tool.

Fonts: alpha-only glyphs (the deck picks the colour). The mock's face is Orbitron at weight 800
(--font "Orbitron[wght].ttf", SIL OFL, from github.com/google/fonts ofl/orbitron); without --font the
tool uses Century Gothic Bold from Windows.
Format "SKR2": src/util/car/sukiride.h (keep the constants below in step with it). Needs Pillow.
"""
import argparse
import glob
import os
import re
import struct
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC_W, SRC_H, SRC_CEL_X = 640, 365, 387
H = 240                    # the deck's height
FULL_W = 421               # the whole art scaled evenly to H (640 * 240 / 365, rounded)
ART_X, ART_W = 160, 160    # the girl's strip on the screen: ART_X to the right edge
LOGO_BOX = (40, 10, 350, 84)   # the SUKI letters in the backdrop (source pixels)
LOGO_W, LOGO_H = 108, 26   # the logo mask at device size
COLOURS = 256
# (size px, tracking, chars): the order of sukiride::FontId
CHARS_SMALL = "ABCDEGHIKLMNOPRSTV/%.-0123456789"
FONTS = [
    (7, 0, CHARS_SMALL),          # F_SMALL: labels, units, the state tag, the RPM number, EST
    (10, 0, "0123456789.-"),      # F_VALUE: the tiles' values
    (16, 0, "1234567-"),          # F_GEAR: the gear numeral
    (12, 0, "0123456789.-"),      # F_BIG: the bars' values
]
WEIGHT = 800   # Orbitron 800, the mock's numerals and tags (ignored by a static font)
DEFAULT_FONTS = ["C:/Windows/Fonts/GOTHICB.TTF"]


def cel_files(assets):
    """The cels in time order: [(second, path)]."""
    out = []
    for p in glob.glob(os.path.join(assets, "suki-layer-cel-*.png")):
        m = re.search(r"suki-layer-cel-([\d.]+)\.png$", p)
        if m:
            out.append((float(m.group(1)), p))
    return sorted(out)


def strip(full):
    """The girl's strip of one 640 x 365 frame at device size: scaled evenly to H, right edge kept."""
    return full.resize((FULL_W, H), Image.LANCZOS).crop((FULL_W - ART_W, 0, FULL_W, H))


def logo(backdrop):
    """The SUKI letters as alpha (LOGO_W * LOGO_H bytes): the white of the letters, not their shadow."""
    box = backdrop.crop(LOGO_BOX)
    r, g, b = box.split()
    low = ImageChops.darker(r, ImageChops.darker(g, b)).resize((LOGO_W, LOGO_H), Image.LANCZOS)
    return bytes(max(0, min(255, (v - 150) * 4)) for v in low.tobytes())


def layers(assets):
    """(backdrop 640 x 365 RGB, [girl strip ART_W x H RGB]). Each cel is composed onto the full
    backdrop first and scaled as a whole, so the strip's edges blend as they would."""
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
        cels.append(strip(full))
    return bd, cels


def quantize(cels):
    """One 256-colour palette over every cel: (palette [(r,g,b)] * 256, [cel indices])."""
    sheet = Image.new("RGB", (ART_W * len(cels), H))
    for i, c in enumerate(cels):
        sheet.paste(c, (i * ART_W, 0))
    q = sheet.quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:COLOURS * 3]
    pal += [0] * (COLOURS * 3 - len(pal))
    palette = [tuple(pal[i * 3:i * 3 + 3]) for i in range(COLOURS)]
    idx = q.tobytes()
    sw = sheet.width

    def crop(x0, w):
        return b"".join(idx[y * sw + x0:y * sw + x0 + w] for y in range(H))

    return palette, [crop(i * ART_W, ART_W) for i in range(len(cels))]


def glyphs(font_path, size, chars):
    """[(char, width, alpha bytes width*height)], cell height: each glyph on the font's own baseline
    in a cell `size` tall that starts at the cap top, anti-aliased by drawing 4x and box-filtering
    down; and the cap height."""
    big = ImageFont.truetype(font_path, size * 4)
    try:   # a variable font (Orbitron[wght]): the mock's 800 weight
        big.set_variation_by_axes([WEIGHT])
    except (OSError, ValueError):
        pass
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


def pack(palette, cels, logo_alpha, fonts):
    """fonts: [(cell height, tracking, cap height, [(char, width, alpha bytes)])] -> the SKR2 file's
    bytes."""
    head = b"SKR2" + struct.pack("<HHHHHHHH", ART_X, ART_W, H, len(cels), len(fonts), LOGO_W, LOGO_H, 0)
    body = bytearray(head)
    for r, g, b in palette:
        body += bytes((r, g, b))
    for c in cels:
        assert len(c) == ART_W * H
        body += c
    assert len(logo_alpha) == LOGO_W * LOGO_H
    body += logo_alpha
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
    ap.add_argument("--preview", default=None, help="write the calm cel back as a PNG to check it")
    a = ap.parse_args()
    font = a.font or next((f for f in DEFAULT_FONTS if os.path.isfile(f)), None)
    if not font:
        sys.exit("gen_suki_ride: no font: pass --font")
    backdrop, cels = layers(a.assets)
    palette, cidx = quantize(cels)
    fonts = []
    for size, track, chars in FONTS:
        gl, cell, cap = glyphs(font, size, chars)
        fonts.append((cell, track, cap, gl))
    blob = pack(palette, cidx, logo(backdrop), fonts)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(blob)
    print("gen_suki_ride: %s, %d bytes (%d cels, font %s)" % (a.out, len(blob), len(cels), os.path.basename(font)))
    if a.preview:   # the calm cel (7) as stored
        flat = [v for c in palette for v in c]
        im = Image.frombytes("P", (ART_W, H), cidx[min(7, len(cidx) - 1)])
        im.putpalette(flat)
        im.convert("RGB").save(a.preview)


if __name__ == "__main__":
    main()
