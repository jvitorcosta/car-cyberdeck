"""Build data/idle.bin: Sonic waiting on the new deck's "Nothing playing" screen.
Run: python tools/gen_idle_sprite.py

Rebuilds the original Sonic the Hedgehog (Mega Drive, 1991) standing and waiting frames from the
s1disasm disassembly (github.com/sonicretro/s1disasm, pinned below): his uncompressed art, the
sprite mappings, the dynamic tile loads (DPLC) and his palette. The files are fetched once into
sonic-sprites/ (git-ignored); the art is SEGA's, used locally, never committed. Without
data/idle.bin the screen keeps Pikachu (tools/idle_clip.py writes an empty one for the build).

Format "IDL1": u8 frames, u8 w, u8 h, u8 0, 16 x u16 RGB565 palette (LE, index 0 transparent),
then each frame as w x h 4-bit indices, row by row, high nibble = left pixel. All frames share
one box, so the feet stay put. Frame order: Stand, Wait1, Wait2, Wait3 (util/idlesprite).
"""
import os
import re
import struct
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "sonic-sprites")
OUT = os.path.join(ROOT, "data", "idle.bin")
REV = "15dfa3a6ee4c53aeb482233e493755c84b4f3479"   # s1disasm commit
FILES = {   # local name: path in the disassembly
    "Sonic.unc": "artunc/Sonic.unc",
    "maps.asm": "_maps/Sonic.asm",
    "dplc.asm": "_maps/Sonic - Dynamic Gfx Script.asm",
    "palette.bin": "palette/Sonic.bin",
}
FRAMES = ["Stand", "Wait1", "Wait2", "Wait3"]
ORIGIN = 32                    # a frame's (0, 0) in the 64 x 64 render grid
SIZE = 64
LEVELS = [0, 52, 87, 116, 144, 172, 206, 255]   # the Mega Drive's 3-bit levels, measured


def md_to_rgb565(word):
    """A Mega Drive colour (0000BBB0GGG0RRR0) as RGB565."""
    r, g, b = (LEVELS[(word >> s) & 7] for s in (1, 5, 9))
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def _num(s):
    s = s.strip()
    neg = s.startswith("-")
    s = s.lstrip("-")
    v = int(s[1:], 16) if s.startswith("$") else int(s)
    return -v if neg else v


def _block(text, label):
    m = re.search(r"^" + re.escape(label) + r":.*?\n(.*?)^" + re.escape(label) + r"_End", text, re.S | re.M)
    if not m:
        raise ValueError("no " + label)
    return m.group(1)


def pieces(maps_text, frame):
    """(x, y, w, h, tile) of each sprite piece of MS_<frame> (no flips in these frames)."""
    out = []
    for line in _block(maps_text, "MS_" + frame).splitlines():
        if "spritePiece" in line:
            v = [_num(a) for a in line.split("spritePiece")[1].split(";")[0].split(",")]
            out.append((v[0], v[1], v[2], v[3], v[4]))
    return out


def loads(dplc_text, frame):
    """(count, first art tile) of each tile load of SonPLC_<frame>, in VRAM order."""
    out = []
    for line in _block(dplc_text, "SonPLC_" + frame).splitlines():
        if "dplcEntry" in line:
            v = [_num(a) for a in line.split("dplcEntry")[1].split(";")[0].split(",")]
            out.append((v[0], v[1]))
    return out


def render(art, frame_pieces, frame_loads):
    """A 64 x 64 grid of palette indices ([y][x], 0 = transparent), the frame's origin at ORIGIN."""
    vram = []
    for count, start in frame_loads:
        vram.extend(art[32 * t:32 * t + 32] for t in range(start, start + count))
    grid = [[0] * SIZE for _ in range(SIZE)]
    for x, y, w, h, first in frame_pieces:
        for cx in range(w):
            for cy in range(h):
                tile = vram[first + cx * h + cy]          # a piece's tiles go down each column
                for py in range(8):
                    for px in range(8):
                        b = tile[py * 4 + px // 2]
                        c = b >> 4 if px % 2 == 0 else b & 15
                        if c:
                            grid[ORIGIN + y + cy * 8 + py][ORIGIN + x + cx * 8 + px] = c
    return grid


def union_box(grids):
    """(x0, y0, w, h) around every opaque pixel of every grid; w padded to even (4-bit pairs)."""
    xs = [x for g in grids for row in g for x, c in enumerate(row) if c]
    ys = [y for g in grids for y, row in enumerate(g) if any(row)]
    x0, y0 = min(xs), min(ys)
    w, h = max(xs) - x0 + 1, max(ys) - y0 + 1
    return x0, y0, w + (w & 1), h


def pack(grids, box, palette565):
    x0, y0, w, h = box
    blob = b"IDL1" + struct.pack("<BBBB", len(grids), w, h, 0) + struct.pack("<16H", *palette565)
    for g in grids:
        for y in range(y0, y0 + h):
            row = [g[y][x] if 0 <= y < len(g) and x < len(g[y]) else 0 for x in range(x0, x0 + w)]
            blob += bytes(row[i] << 4 | row[i + 1] for i in range(0, w, 2))
    return blob


def fetch():
    os.makedirs(SRC, exist_ok=True)
    for name, path in FILES.items():
        dest = os.path.join(SRC, name)
        if os.path.isfile(dest):
            continue
        url = "https://raw.githubusercontent.com/sonicretro/s1disasm/%s/%s" % (REV, path.replace(" ", "%20"))
        print("gen_idle_sprite: fetching", path)
        with urllib.request.urlopen(url, timeout=30) as r, open(dest, "wb") as f:
            f.write(r.read())


def main():
    try:
        fetch()
    except OSError as e:
        sys.exit("gen_idle_sprite: couldn't fetch the disassembly (%s)" % e)
    art = open(os.path.join(SRC, "Sonic.unc"), "rb").read()
    maps = open(os.path.join(SRC, "maps.asm"), encoding="utf-8").read()
    dplc = open(os.path.join(SRC, "dplc.asm"), encoding="utf-8").read()
    pal = open(os.path.join(SRC, "palette.bin"), "rb").read()
    palette = [0] + [md_to_rgb565(pal[2 * i] << 8 | pal[2 * i + 1]) for i in range(1, 16)]
    grids = [render(art, pieces(maps, f), loads(dplc, f)) for f in FRAMES]
    box = union_box(grids)
    blob = pack(grids, box, palette)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    print("gen_idle_sprite: %d frames, %dx%d, %d bytes -> %s" % (len(grids), box[2], box[3], len(blob), OUT))


if __name__ == "__main__":
    main()
