"""Generate data/emoji16.bin: the most used emoji as 16x16 colour pictures (Twemoji), in flash.

Why: Spotify names and lyrics are full of emoji, and the deck dropped them all. The ~900 most
used (Unicode's frequency tiers 0..11, tools/emoji_freq.txt) cover nearly every name; the 248
joined (ZWJ) emoji without skin tones (tools/emoji_zwj.txt: black cat, rainbow flag, families...)
draw as one picture. With a 16-colour palette per emoji that is ~195 KB of flash and 0 RAM
(src/black/ui/emojifont.h).

  python tools/gen_emoji.py [--tiers N] [--cache DIR]

Needs Pillow (pip install pillow) for the pictures; fetches the Twemoji 72x72 PNGs once into the
cache (default .pio/twemoji). Twemoji graphics: (c) Twitter / jdecked, CC-BY 4.0.
Format (little-endian): "EMJ2", u16 count, u16 nseq, count x u32 key (ascending), nseq x 4 x u32
joined sequence (zero-padded, ascending; sequence i has key SEQ_BASE + i), then count glyphs of
160 B: 16 x u16 RGB565 palette (index 0 = transparent), 16x16 at 4 bpp, high nibble = left pixel.
"""
import argparse
import io
import os
import struct
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
FREQ = os.path.join(ROOT, "tools", "emoji_freq.txt")
ZWJ_LIST = os.path.join(ROOT, "tools", "emoji_zwj.txt")
OUT = os.path.join(ROOT, "data", "emoji16.bin")
TWEMOJI = "https://cdn.jsdelivr.net/gh/jdecked/twemoji@15.1.0/assets/72x72/%s.png"
SIZE, COLOURS = 16, 15            # 15 colours + index 0 transparent
GLYPH_BYTES = 32 + SIZE * SIZE // 2
RI_FIRST, RI_LAST, ZWJ = 0x1F1E6, 0x1F1FF, 0x200D
EXTRA = [[0x1F1E7, 0x1F1F7]]      # always in: the Brazilian flag
SEQ_BASE, SEQ_MAX = 0x120000, 4   # emojifont::SEQ_BASE, SEQ_MAX


def is_modifier(c):
    """Sequence parts that never draw (same list as glyphrun's isModifier)."""
    return c in (0xFE0E, 0xFE0F, 0x20E3) or 0x1F3FB <= c <= 0x1F3FF or 0xE0020 <= c <= 0xE007F


def flag_key(a, b):
    """glyphrun::flagKey."""
    return 0x110000 + (a - RI_FIRST) * 26 + (b - RI_FIRST)


def key_of(seq):
    """The key the deck looks up for an emoji sequence (list of codepoints), or None when it never
    reaches the emoji font (ASCII keycaps and Latin-1 like (c) draw as text)."""
    cps = []
    for c in seq:
        if c == ZWJ:
            break                  # a joined sequence shows its first emoji
        if not is_modifier(c):
            cps.append(c)
    if not cps or cps[0] < 0x800:
        return None
    if RI_FIRST <= cps[0] <= RI_LAST:
        if len(cps) < 2 or not RI_FIRST <= cps[1] <= RI_LAST:
            return None
        return flag_key(cps[0], cps[1])
    return cps[0]


def joined_parts(seq):
    """What the deck looks a joined emoji up by: its codepoints without ZWJ, variation selectors
    and skin tones (glyphrun's joined())."""
    return tuple(c for c in seq if c != ZWJ and c not in (0xFE0E, 0xFE0F)
                 and not 0x1F3FB <= c <= 0x1F3FF)


def read_zwj(path):
    """[(sequence as written, Twemoji file name)] of tools/emoji_zwj.txt."""
    out = []
    for line in open(path, encoding="ascii"):
        if line.startswith("#") or not line.strip():
            continue
        hexes = line.split("\t")[0]
        out.append(([int(c, 16) for c in hexes.split("-")], hexes))
    return out


def twemoji_name(key):
    if key >= 0x110000:            # flag
        a, b = divmod(key - 0x110000, 26)
        return "%x-%x" % (RI_FIRST + a, RI_FIRST + b)
    return "%x" % key


def read_tiers(path, tiers):
    """Keys of tiers 0..tiers, most used first, without duplicates."""
    keys = []
    for line in open(path, encoding="ascii"):
        if line.startswith("#") or not line.strip():
            continue
        tier, seqs = line.rstrip("\n").split("\t")
        if int(tier) > tiers:
            continue
        for s in seqs.split():
            k = key_of([int(c, 16) for c in s.split("-")])
            if k is not None and k not in keys:
                keys.append(k)
    return keys


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def pack(glyphs, seqs=()):
    """glyphs: [(key, palette [RGB565] of <= 15, pixels [256] of 0..len(palette))] -> blob bytes.
    Pixel 0 is transparent; pixel i >= 1 is palette[i - 1]. seqs: joined_parts tuples, ascending;
    seqs[i] is drawn from the glyph with key SEQ_BASE + i."""
    glyphs = sorted(glyphs, key=lambda g: g[0])
    keys = [g[0] for g in glyphs]
    if len(set(keys)) != len(keys):
        raise ValueError("duplicate keys")
    seqs = list(seqs)
    if seqs != sorted(set(seqs)) or any(not 2 <= len(q) <= SEQ_MAX for q in seqs):
        raise ValueError("sequences must be unique, ascending, 2..%d long" % SEQ_MAX)
    out = bytearray(b"EMJ2" + struct.pack("<HH", len(glyphs), len(seqs)))
    out += b"".join(struct.pack("<I", k) for k in keys)
    out += b"".join(struct.pack("<4I", *(list(q) + [0] * (SEQ_MAX - len(q)))) for q in seqs)
    for key, palette, pixels in glyphs:
        if len(palette) > COLOURS or len(pixels) != SIZE * SIZE or max(pixels) > len(palette):
            raise ValueError("emoji %x: %d colours" % (key, len(palette)))
        out += struct.pack("<16H", 0, *(list(palette) + [0] * (COLOURS - len(palette))))
        out += bytes((pixels[i] << 4) | pixels[i + 1] for i in range(0, SIZE * SIZE, 2))
    return bytes(out)


def to_glyph(png):
    """Twemoji PNG bytes -> (palette, pixels): 16x16, alpha >= 50 % opaque, <= 15 colours."""
    from PIL import Image
    im = Image.open(io.BytesIO(png)).convert("RGBA").resize((SIZE, SIZE), Image.LANCZOS)
    px = [im.getpixel((x, y)) for y in range(SIZE) for x in range(SIZE)]
    opaque = [p[:3] for p in px if p[3] >= 128]
    if not opaque:
        return [], [0] * (SIZE * SIZE)
    strip = Image.new("RGB", (len(opaque), 1))
    strip.putdata(opaque)
    q = strip.quantize(min(COLOURS, len(set(opaque))), method=Image.Quantize.MEDIANCUT)
    pal = q.getpalette()[:3 * COLOURS]
    idx = iter(q.tobytes())
    pixels = [next(idx) + 1 if p[3] >= 128 else 0 for p in px]
    used = max(pixels)
    return [rgb565(*pal[3 * i:3 * i + 3]) for i in range(used)], pixels


def fetch(name, cache):
    """Twemoji PNG bytes for a file name ("1f525", "1f408-200d-2b1b"), or None. Twemoji keeps
    FE0F in joined names; a name without it is tried too."""
    for n in dict.fromkeys([name, name.replace("-fe0f", "")]):
        path = os.path.join(cache, n + ".png")
        if not os.path.isfile(path):
            try:
                data = urllib.request.urlopen(TWEMOJI % n, timeout=30).read()
            except Exception:  # noqa: BLE001 - try the next name, else leave it out
                continue
            with open(path, "wb") as f:
                f.write(data)
        return open(path, "rb").read()
    print("gen_emoji: no Twemoji for %s, left out" % name)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tiers", type=int, default=11)
    ap.add_argument("--cache", default=os.path.join(ROOT, ".pio", "twemoji"))
    args = ap.parse_args()
    os.makedirs(args.cache, exist_ok=True)
    keys = read_tiers(FREQ, args.tiers)
    keys += [k for k in (key_of(s) for s in EXTRA) if k not in keys]
    glyphs = []
    for k in keys:
        png = fetch(twemoji_name(k), args.cache)
        if png:
            glyphs.append((k,) + tuple(to_glyph(png)))
    joined = {}
    for seq, name in read_zwj(ZWJ_LIST):
        png = fetch(name, args.cache)
        if png:
            joined[joined_parts(seq)] = to_glyph(png)
    seqs = sorted(joined)
    glyphs += [(SEQ_BASE + i,) + tuple(joined[q]) for i, q in enumerate(seqs)]
    blob = pack(glyphs, seqs)
    with open(OUT, "wb") as f:
        f.write(blob)
    print("wrote %s: %d emoji (%d joined), %d bytes" % (OUT, len(glyphs), len(seqs), len(blob)))


if __name__ == "__main__":
    sys.exit(main())
