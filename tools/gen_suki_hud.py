# tools/gen_suki_hud.py
"""Build data/suki.bin: the Suki look of the black deck's gauge screen (black/ui/sukiview), from two
frames of the film monitor's art. Run:
  python tools/gen_suki_hud.py [--frames suki-frames] [--fonts C:/Windows/Fonts] [--out data/suki.bin]
                               [--preview suki.png]

suki-frames/ holds two 1404 x 812 PNGs of the same screen (captured from the making-of video of a fan
recreation): master.png, the finished screen (logo, waveform window, the girl, the text), and
notext.png, a frame from before the text went on, registered to master.png (same scale and place).
The background is master.png with its info row and STAGE bars taken from notext.png and the
waveform's capsules erased; the firmware draws the live values over it. The art is the film's: the
frames and data/suki.bin are git-ignored, like the montage. Fonts: Century Gothic Bold (the film's
wide round numerals) and Impact (the STAGE lettering), from Windows. Needs Pillow.
Format "SUK1" and the layout: src/util/car/sukihud.h (keep the constants below in step with it).
"""
import argparse
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
W, H = 320, 240
FILM_H = 185                      # the film screen, scaled to 320 wide, on top
LILAC, EDGE = (128, 124, 246), (24, 6, 40)
RED = (236, 16, 24)
PINK, CORE = (255, 40, 112), (255, 235, 245)
STRIP_BG, WINDOW_BG = (28, 4, 12), (14, 0, 6)
DEGREE = "*"                      # the firmware's stand-in for the degree sign

# sukihud.h's value regions (x, y, w, h): labels go to their left
BAT, THR, H2O, IAT = (37, 107, 27, 10), (89, 107, 26, 10), (37, 116, 27, 10), (89, 116, 26, 10)
STAGES = [(133, 162, "0-100"), (152, 175, "BEST"), (172, 187, "KM/L")]   # (y mid, right edge, label)

# (id, size px, horizontal stretch, style, chars, tracking): the order of sukihud::FontId
FONTS = [
    ("small", 9, 1.05, "film", "0123456789.-%" + DEGREE, -1),   # film glyphs: their 1 px edges overlap
    ("bar", 12, 1.22, "film", "0123456789:.- ", -1),
    ("clock", 15, 1.10, "film", "0123456789:-", -1),
    ("rpm", 12, 1.15, "film", "0123456789-", -1),
    ("load", 10, 1.00, "film", "0123456789-%", -1),
    ("speed", 30, 1.20, "capsule", "0123456789-", 2),
]


def gray(p):
    return abs(p[0] - p[1]) < 14 and abs(p[1] - p[2]) < 14 and p[0] > 95


def capsule_ink(p):
    return p[0] > 70 or p[1] > 50


def erase_by_rows(img, box, is_ink, grow=5):
    """Remove ink in box: grow the ink mask (anti-aliased edges), then every masked run on a row
    becomes a blend of the clean pixels at its two ends."""
    px = img.load()
    x0, y0, x1, y1 = box
    mask = Image.new("L", img.size, 0)
    mp = mask.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if is_ink(px[x, y]):
                mp[x, y] = 255
    if grow:
        mask = mask.filter(ImageFilter.MaxFilter(grow))
        mp = mask.load()
    for y in range(y0, y1):
        x = x0
        while x < x1:
            if not mp[x, y]:
                x += 1
                continue
            s = x
            while x < x1 and mp[x, y]:
                x += 1
            a, b = px[max(s - 1, 0), y], px[min(x, img.width - 1), y]
            n = x - s + 1
            for i in range(s, x):
                t = (i - s + 1) / n
                px[i, y] = tuple(int(a[k] * (1 - t) + b[k] * t) for k in range(3))


def clean_film(master, notext):
    """The film screen with no text (1404 x 812)."""
    m = master.convert("RGB")
    mask = Image.new("L", m.size, 0)
    md = ImageDraw.Draw(mask)
    md.rectangle((0, 466, 852, 811), fill=255)   # the info row and the STAGE bars
    md.rectangle((0, 466, 884, 560), fill=255)   # and all of the clock
    m = Image.composite(notext.convert("RGB"), m, mask.filter(ImageFilter.GaussianBlur(2)))
    erase_by_rows(m, (34, 234, 802, 434), capsule_ink, grow=7)
    return m


def chrome(master):
    """The STAGE bars' real chrome gradient, top to bottom."""
    m = master.convert("RGB")
    return [m.getpixel((492, y)) for y in range(540, 625) if gray(m.getpixel((492, y)))]


def glyph(font, ch, px, stretch, style):
    """One character as RGBA, cropped to its ink across but to the font's whole cell down."""
    s = 2 if style == "capsule" else 1
    fill, edge = (CORE, PINK) if style == "capsule" else (LILAC, EDGE)
    cell = Image.new("RGBA", (px * 3, px * 2 + 2 * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(cell)
    if ch == " ":
        w = max(2, round(px * 0.3))
        return Image.new("RGBA", (w, cell.height), (0, 0, 0, 0))
    d.text((s + 2, s + 1), "\u00b0" if ch == DEGREE else ch, font=font, fill=fill, stroke_width=s, stroke_fill=edge)
    box = cell.getbbox()
    if not box:
        return Image.new("RGBA", (1, cell.height), (0, 0, 0, 0))
    g = cell.crop((box[0], 0, box[2], cell.height))
    return g.resize((max(1, round(g.width * stretch)), g.height), Image.LANCZOS)


def font_glyphs(fonts_dir, px, stretch, style, chars):
    f = ImageFont.truetype(os.path.join(fonts_dir, "GOTHICB.TTF"), px)
    gs = [(ch, glyph(f, ch, px, stretch, style)) for ch in chars]
    # one cell height for the font: the digits' ink rows, so yMid centres the numbers
    top, bot = 10 ** 6, -1
    for ch, g in gs:
        if ch.isdigit():
            b = g.getbbox()
            if b:
                top, bot = min(top, b[1]), max(bot, b[3])
    return [(ch, g.crop((0, top, g.width, bot))) for ch, g in gs], bot - top


def stage_label(img, xr, ymid, text, px, fonts_dir, anchor="r"):
    f = ImageFont.truetype(os.path.join(fonts_dir, "impact.ttf"), px)
    tmp = Image.new("RGBA", (len(text) * px + 20, px * 2), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((4, px), text, font=f, fill=RED, anchor="lm")
    tmp = tmp.crop(tmp.getbbox())
    x = xr - tmp.width if anchor == "r" else xr
    img.paste(tmp, (int(x), int(ymid - tmp.height / 2)), tmp)


def film_label(img, xr, ymid, text, fonts_dir):
    """A small lilac label right-aligned at xr, in the info row's font."""
    f = ImageFont.truetype(os.path.join(fonts_dir, "GOTHICB.TTF"), 9)
    tmp = Image.new("RGBA", (60, 24), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((2, 12), text, font=f, fill=LILAC, anchor="lm", stroke_width=1, stroke_fill=EDGE)
    tmp = tmp.crop(tmp.getbbox())
    tmp = tmp.resize((round(tmp.width * 1.05), tmp.height), Image.LANCZOS)
    img.paste(tmp, (int(xr - tmp.width), int(ymid - tmp.height / 2)), tmp)


def background(master, notext, fonts_dir):
    img = Image.new("RGB", (W, H), STRIP_BG)
    img.paste(clean_film(master, notext).resize((W, FILM_H), Image.LANCZOS), (0, 0))
    # the info row's labels (the film's FD / HD lines) and the STAGE bars' labels
    for (x, y, w, h), lab in ((BAT, "BAT"), (THR, "THR"), (H2O, "H2O"), (IAT, "IAT")):
        film_label(img, x - 2, y + h // 2, lab, fonts_dir)
    for ymid, xr, lab in STAGES:
        stage_label(img, xr, ymid, lab, 10, fonts_dir)
    # the strip: a chrome-framed window, like the waveform's, and its labels
    col = chrome(master)
    d = ImageDraw.Draw(img)
    for i in range(3):
        c = col[int((i + 1) * (len(col) - 1) / 4)]
        d.rounded_rectangle((1 + i, 186 + i, 318 - i, 239 - i), radius=5 - i, fill=c)
    d.rounded_rectangle((4, 189, 315, 236), radius=2, fill=WINDOW_BG)
    stage_label(img, 12, 232, "KM/H", 9, fonts_dir, anchor="l")
    stage_label(img, 262, 232, "RPM", 9, fonts_dir)
    return img


def rgb565_be(rgb):
    r, g, b = rgb[:3]
    c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return bytes((c >> 8, c & 0xFF))


def pack(bg, fonts):
    """bg: a W x H RGB image; fonts: [(cell height, tracking (may be negative), [(char, RGBA image)])]
    -> SUK1 bytes."""
    out = bytearray(b"SUK1" + struct.pack("<HHHH", W, H, len(fonts), 0))
    px = bg.convert("RGB").load()
    for y in range(H):
        for x in range(W):
            out += rgb565_be(px[x, y])
    tables, blobs = [], []
    at = len(out) + sum(4 + 8 * len(gs) for _, _, gs in fonts)
    for h, track, gs in fonts:
        t = bytearray(struct.pack("<BbBB", h, track, len(gs), 0))
        for ch, g in gs:
            data = bytearray()
            gp = g.load()
            for y in range(h):
                for x in range(g.width):
                    p = gp[x, y] if y < g.height else (0, 0, 0, 0)
                    data += rgb565_be(p) + bytes((p[3],))
            t += struct.pack("<BBHI", ord(ch), g.width, 0, at)
            blobs.append(data)
            at += len(data)
        tables.append(t)
    for t in tables:
        out += t
    for b in blobs:
        out += b
    return bytes(out)


def build(master, notext, fonts_dir):
    fonts = []
    for _name, size, stretch, style, chars, track in FONTS:
        gs, h = font_glyphs(fonts_dir, size, stretch, style, chars)
        fonts.append((h, track, gs))
    bg = background(master, notext, fonts_dir)
    return bg, pack(bg, fonts)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--frames", default=os.path.join(ROOT, "suki-frames"))
    ap.add_argument("--fonts", default="C:/Windows/Fonts")
    ap.add_argument("--out", default=os.path.join(ROOT, "data", "suki.bin"))
    ap.add_argument("--preview", help="also write the background as a PNG")
    a = ap.parse_args()
    master = Image.open(os.path.join(a.frames, "master.png"))
    notext = Image.open(os.path.join(a.frames, "notext.png"))
    if master.size != (1404, 812) or notext.size != (1404, 812):
        sys.exit("gen_suki_hud: the frames must be 1404 x 812 and registered (see the docstring)")
    bg, blob = build(master, notext, a.fonts)
    with open(a.out, "wb") as f:
        f.write(blob)
    if a.preview:
        bg.save(a.preview)
    print("gen_suki_hud: %d B -> %s" % (len(blob), os.path.relpath(a.out)))


if __name__ == "__main__":
    main()
