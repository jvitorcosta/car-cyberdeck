"""Spike (2026-10-22), the base of tools/gen_slz_backdrop.py: render Star Light Zone's background layout from s1disasm files to a PNG.
Usage: python -I slz_spike.py <src dir> <out.png> [layout file]"""
import sys
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
layout_name = sys.argv[3] if len(sys.argv) > 3 else "slzbg.bin"
rd = lambda n: open(f"{src}/{n}", "rb").read()


def nemesis(d):
    hdr = d[0] << 8 | d[1]
    xor, ntiles = hdr & 0x8000, hdr & 0x7FFF
    pos = 2
    table = {}
    nyb = 0
    b = d[pos]; pos += 1
    while b != 0xFF:
        if b & 0x80:
            nyb = b & 0x0F
            b = d[pos]; pos += 1
            continue
        count = ((b >> 4) & 7) + 1
        length = b & 0x0F
        code = d[pos]; pos += 1
        table[(length, code)] = (nyb, count)
        b = d[pos]; pos += 1
    bitpos = pos * 8
    def bits(n):
        nonlocal bitpos
        v = 0
        for _ in range(n):
            v = v << 1 | (d[bitpos >> 3] >> (7 - (bitpos & 7))) & 1
            bitpos += 1
        return v
    nybs = []
    total = ntiles * 64
    while len(nybs) < total:
        code, length = 0, 0
        while True:
            code = code << 1 | bits(1); length += 1
            if length == 6 and code == 0x3F:
                count = bits(3) + 1; v = bits(4)
                nybs += [v] * count
                break
            if (length, code) in table:
                v, count = table[(length, code)]
                nybs += [v] * count
                break
            if length > 8:
                raise ValueError("bad nemesis code")
    nybs = nybs[:total]
    rows = [nybs[i:i + 8] for i in range(0, total, 8)]
    if xor:
        prev = [0] * 8
        for i, r in enumerate(rows):
            rows[i] = [a ^ b for a, b in zip(r, prev)]
            prev = rows[i]
    return [rows[t * 8:t * 8 + 8] for t in range(ntiles)]


def kosinski(d):
    out = bytearray(); pos = 0
    desc = d[pos] | d[pos + 1] << 8; pos += 2; left = 16
    def bit():
        nonlocal desc, left, pos
        b = desc & 1; desc >>= 1; left -= 1
        if left == 0:
            desc = d[pos] | d[pos + 1] << 8; pos += 2; left = 16
        return b
    while True:
        if bit():
            out.append(d[pos]); pos += 1; continue
        if bit():
            lo, hi = d[pos], d[pos + 1]; pos += 2
            offset = ((hi & 0xF8) << 5 | lo) - 0x2000
            count = hi & 7
            if count:
                count += 2
            else:
                count = d[pos]; pos += 1
                if count == 0: break
                if count == 1: continue
                count += 1
        else:
            count = (bit() << 1 | bit()) + 2
            offset = d[pos] - 0x100; pos += 1
        for _ in range(count):
            out.append(out[len(out) + offset])
    return bytes(out)


def enigma(d):
    plen, mask = d[0], d[1]
    inc = d[2] << 8 | d[3]; common = d[4] << 8 | d[5]
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
            if mask & (1 << bitno) and bits(1): v |= flag
        return v | bits(plen)
    out = []
    while True:
        if bits(1) == 0:
            kind = bits(1); count = bits(4) + 1
            for _ in range(count):
                if kind == 0: out.append(inc); inc = (inc + 1) & 0xFFFF
                else: out.append(common)
        else:
            mode = bits(2); count = bits(4)
            if mode == 3:
                if count == 0xF: break
                for _ in range(count + 1): out.append(inline())
            else:
                v = inline()
                for _ in range(count + 1):
                    out.append(v)
                    if mode == 1: v = (v + 1) & 0xFFFF
                    elif mode == 2: v = (v - 1) & 0xFFFF
    return out


LEVELS = [0, 52, 87, 116, 144, 172, 206, 255]
def md(w):
    return tuple(LEVELS[(w >> s) & 7] for s in (1, 5, 9))
pal = rd("Sonic.bin") + rd("Star Light Zone.bin")
colors = [md(pal[i] << 8 | pal[i + 1]) for i in range(0, len(pal), 2)]
while len(colors) < 64: colors.append((255, 0, 255))

tiles = nemesis(rd("8x8 - SLZ.nem"))
blocks16 = enigma(rd("SLZ.eni"))
chunks = kosinski(rd("SLZ.kos"))
lay = rd(layout_name)
w, h = lay[0] + 1, lay[1] + 1
print(f"tiles {len(tiles)} blocks {len(blocks16)//4} chunks {len(chunks)//512} layout {w}x{h}")

img = Image.new("RGB", (w * 256, h * 256), (0, 0, 0))
px = img.load()
def put_tile(word, x0, y0, fx, fy):
    t = word & 0x7FF; line = (word >> 13) & 3
    hf = bool(word & 0x0800) ^ fx; vf = bool(word & 0x1000) ^ fy
    if t >= len(tiles): return
    for y in range(8):
        row = tiles[t][7 - y if vf else y]
        for x in range(8):
            c = row[7 - x if hf else x]
            px[x0 + x, y0 + y] = colors[line * 16 + c] if c else colors[line * 16]
for ly in range(h):
    for lx in range(w):
        cid = lay[2 + ly * w + lx] & 0x7F
        if cid == 0: continue
        base = (cid - 1) * 512
        for by in range(16):
            for bx in range(16):
                cw = chunks[base + (by * 16 + bx) * 2] << 8 | chunks[base + (by * 16 + bx) * 2 + 1]
                bi = cw & 0x3FF; cfx = bool(cw & 0x0800); cfy = bool(cw & 0x1000)
                for ty in range(2):
                    for tx in range(2):
                        sx = 1 - tx if cfx else tx; sy = 1 - ty if cfy else ty
                        word = blocks16[bi * 4 + sy * 2 + sx]
                        put_tile(word, lx * 256 + bx * 16 + tx * 8, ly * 256 + by * 16 + ty * 8, cfx, cfy)
img.save(out)
print("wrote", out, img.size)
