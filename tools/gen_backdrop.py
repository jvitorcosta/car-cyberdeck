"""Build data/backdrop.bin: Green Hill Zone behind the S3's firmware-update screen.
Run: python tools/gen_backdrop.py [--png preview.png]

Rebuilds the background of Green Hill Zone from Sonic the Hedgehog (Mega Drive, 1991) out of the
s1disasm disassembly (github.com/sonicretro/s1disasm, pinned below, as tools/gen_idle_sprite.py):
the zone's 8x8 art (Nemesis), its 16x16 blocks (Enigma), its 256x256 chunks (Kosinski), the
background layout and the palette. The files are fetched once into sonic-sprites/ghz/
(git-ignored); the art is SEGA's, used locally, never committed. Without data/backdrop.bin the
update screen keeps its battle box. --png also writes a preview (needs Pillow).

Format "BKD1": u16 width, u16 height (LE), then width x height RGB565 pixels row by row, each as
2 bytes big-endian (read as a little-endian uint16 it is already the byte order pushImage wants).
"""
import os
import struct
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_idle_sprite import REV, md_to_rgb565  # noqa: E402  (same pinned disassembly)

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "sonic-sprites", "ghz")
OUT = os.path.join(ROOT, "data", "backdrop.bin")
FILES = {   # local name: path in the disassembly
    "art.nem": "artnem/8x8 - GHZ (Combined).nem",
    "blocks.eni": "map16/GHZ.eni",
    "chunks.kos": "map256/GHZ.kos",
    "layout.bin": "levels/ghzbg.bin",
    "zone.pal": "palette/Green Hill Zone.bin",
    "sonic.pal": "palette/Sonic.bin",
}
WINDOW = (1100, 16, 1420, 256)  # x0, y0, x1, y1 of the rendered layout: clouds, hills, the waterfall, the lake


def nemesis(d):
    """Nemesis-compressed 8x8 tiles -> [tile][row][x] colour indices (0..15)."""
    hdr = d[0] << 8 | d[1]
    xor, ntiles = hdr & 0x8000, hdr & 0x7FFF
    pos, table, nyb = 2, {}, 0
    b = d[pos]; pos += 1
    while b != 0xFF:
        if b & 0x80:                      # a new colour for the codes that follow
            nyb = b & 0x0F
            b = d[pos]; pos += 1
            continue
        count, length = ((b >> 4) & 7) + 1, b & 0x0F
        table[(length, d[pos])] = (nyb, count)
        pos += 1
        b = d[pos]; pos += 1
    bitpos = pos * 8

    def bits(n):
        nonlocal bitpos
        v = 0
        for _ in range(n):
            v = v << 1 | (d[bitpos >> 3] >> (7 - (bitpos & 7))) & 1
            bitpos += 1
        return v

    total, nybs = ntiles * 64, []
    while len(nybs) < total:
        code = length = 0
        while True:
            code = code << 1 | bits(1)
            length += 1
            if length == 6 and code == 0x3F:      # inline: 3-bit count, 4-bit colour
                count = bits(3) + 1
                nybs += [bits(4)] * count
                break
            if (length, code) in table:
                v, count = table[(length, code)]
                nybs += [v] * count
                break
            if length > 8:
                raise ValueError("nemesis: no code matches")
    rows = [nybs[i:i + 8] for i in range(0, total, 8)]
    if xor:
        prev = [0] * 8
        for i, r in enumerate(rows):
            rows[i] = prev = [a ^ p for a, p in zip(r, prev)]
    return [rows[t * 8:t * 8 + 8] for t in range(ntiles)]


def kosinski(d):
    """Kosinski-compressed bytes -> bytes."""
    out, pos = bytearray(), 2
    desc, left = d[0] | d[1] << 8, 16

    def bit():
        nonlocal desc, left, pos
        b = desc & 1
        desc >>= 1
        left -= 1
        if left == 0:
            desc, left = d[pos] | d[pos + 1] << 8, 16
            pos += 2
        return b

    while True:
        if bit():
            out.append(d[pos]); pos += 1
            continue
        if bit():                                  # full form: 13-bit offset, 3-bit or byte count
            lo, hi = d[pos], d[pos + 1]; pos += 2
            offset = ((hi & 0xF8) << 5 | lo) - 0x2000
            count = hi & 7
            if count:
                count += 2
            else:
                count = d[pos]; pos += 1
                if count == 0:
                    break
                if count == 1:
                    continue
                count += 1
        else:                                      # inline: 2-bit count, byte offset
            count = (bit() << 1 | bit()) + 2
            offset = d[pos] - 0x100; pos += 1
        for _ in range(count):
            out.append(out[len(out) + offset])
    return bytes(out)


def enigma(d):
    """Enigma-compressed tile-map words -> list of 16-bit words."""
    plen, mask = d[0], d[1]
    inc, common = d[2] << 8 | d[3], d[4] << 8 | d[5]
    bitpos = 6 * 8

    def bits(n):
        nonlocal bitpos
        v = 0
        for _ in range(n):
            v = v << 1 | (d[bitpos >> 3] >> (7 - (bitpos & 7))) & 1
            bitpos += 1
        return v

    def inline():
        v = 0
        for bitno, flag in ((4, 0x8000), (3, 0x4000), (2, 0x2000), (1, 0x1000), (0, 0x0800)):
            if mask & (1 << bitno) and bits(1):
                v |= flag
        return v | bits(plen)

    out = []
    while True:
        if bits(1) == 0:                           # 00: incrementing, 01: common, count + 1
            kind, count = bits(1), bits(4) + 1
            for _ in range(count):
                if kind == 0:
                    out.append(inc); inc = (inc + 1) & 0xFFFF
                else:
                    out.append(common)
            continue
        mode, count = bits(2), bits(4)             # 100 repeat, 101 up, 110 down, 111 each
        if mode == 3:
            if count == 0xF:
                break
            out += [inline() for _ in range(count + 1)]
            continue
        v = inline()
        for _ in range(count + 1):
            out.append(v)
            v = (v + 1) & 0xFFFF if mode == 1 else (v - 1) & 0xFFFF if mode == 2 else v
    return out


def render(src_dir):
    """The background layout's WINDOW as (width, height, RGB565 pixels row by row)."""
    rd = lambda n: open(os.path.join(src_dir, n), "rb").read()  # noqa: E731
    pal = rd("sonic.pal") + rd("zone.pal")          # palette lines 0 (Sonic) and 1..3 (the zone)
    colours = [md_to_rgb565(pal[i] << 8 | pal[i + 1]) for i in range(0, len(pal), 2)]
    colours += [0] * (64 - len(colours))
    tiles = nemesis(rd("art.nem"))
    blocks = enigma(rd("blocks.eni"))
    chunks = kosinski(rd("chunks.kos"))
    lay = rd("layout.bin")
    lw = lay[0] + 1
    x0, y0, x1, y1 = WINDOW
    out = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            cid = lay[2 + (y >> 8) * lw + (x >> 8)] & 0x7F
            if cid == 0:
                out.append(0)
                continue
            i = (cid - 1) * 512 + (((y & 255) >> 4) * 16 + ((x & 255) >> 4)) * 2
            cw = chunks[i] << 8 | chunks[i + 1]       # chunk -> 16x16 block (+ flips)
            cfx, cfy = bool(cw & 0x0800), bool(cw & 0x1000)
            bx, by = (x & 15), (y & 15)
            if cfx: bx = 15 - bx
            if cfy: by = 15 - by
            word = blocks[(cw & 0x3FF) * 4 + (by >> 3) * 2 + (bx >> 3)]   # block -> 8x8 tile
            tx, ty = bx & 7, by & 7
            if word & 0x0800: tx = 7 - tx
            if word & 0x1000: ty = 7 - ty
            t = word & 0x7FF
            c = tiles[t][ty][tx] if t < len(tiles) else 0
            out.append(colours[((word >> 13) & 3) * 16 + c])
    return x1 - x0, y1 - y0, out


def pack(w, h, pixels):
    return b"BKD1" + struct.pack("<HH", w, h) + b"".join(struct.pack(">H", p) for p in pixels)


def fetch():
    os.makedirs(SRC, exist_ok=True)
    for name, path in FILES.items():
        dest = os.path.join(SRC, name)
        if os.path.isfile(dest):
            continue
        url = "https://raw.githubusercontent.com/sonicretro/s1disasm/%s/%s" % (
            REV, path.replace(" ", "%20").replace("(", "%28").replace(")", "%29"))
        print("gen_backdrop: fetching", path)
        with urllib.request.urlopen(url, timeout=30) as r, open(dest, "wb") as f:
            f.write(r.read())


def main():
    try:
        fetch()
    except OSError as e:
        sys.exit("gen_backdrop: couldn't fetch the disassembly (%s)" % e)
    w, h, px = render(SRC)
    data = pack(w, h, px)
    with open(OUT, "wb") as f:
        f.write(data)
    print("gen_backdrop: wrote %s (%dx%d, %d B)" % (OUT, w, h, len(data)))
    if "--png" in sys.argv:
        from PIL import Image
        img = Image.new("RGB", (w, h))
        img.putdata([((p >> 11) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3) for p in px])
        img.save(sys.argv[sys.argv.index("--png") + 1])


if __name__ == "__main__":
    main()
