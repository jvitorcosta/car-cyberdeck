# tools/gen_mcqueen_sprite.py
"""Build data/mcqueen.bin: the boot scene's pixel Lightning McQueen (S3 opening C), from a PNG.
Run: python tools/gen_mcqueen_sprite.py <mcqueen.png> [--shoulder 30] [--head 186,24,199,36]
                                       [--tail 0,18,5,30] [--wheels 46,160,52] [--beam 200,31] [--tail-glow 21]

The PNG is a 200 x 66 side view facing right, transparent background, wheels left out (the
scene draws them so they spin, at --wheels). A fan sprite of a Disney/Pixar character: the PNG
and data/mcqueen.bin are git-ignored. Standard library only. Format "CAR1": util/carbin.h.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_status_sprite import decode_png  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "mcqueen.bin")
W, H = 200, 66
NONE, UPPER, LOWER, OTHER, HEAD, TAIL = 0, 1, 2, 3, 4, 5


def inside(box, x, y):
    return box[0] <= x <= box[2] and box[1] <= y <= box[3]


def classify(rgba, x, y, head, tail, shoulder):
    r, g, b, a = rgba
    if a < 128:
        return NONE
    if inside(head, x, y):
        return HEAD
    if inside(tail, x, y):
        return TAIL
    if r >= 140 and g <= 90 and b <= 90:
        return UPPER if y < shoulder else LOWER
    return OTHER


def rgb565(r, g, b):
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3


def pack(w, h, kinds, px, anchors):
    head = b"CAR1" + struct.pack("<8H", w, h, *anchors)
    nib = bytearray((len(kinds) + 1) // 2)
    for i, k in enumerate(kinds):
        nib[i >> 1] |= (k << 4) if i % 2 == 0 else k
    return head + bytes(nib) + struct.pack("<%dH" % len(px), *px)


def box(s):
    return tuple(int(v) for v in s.split(","))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("png")
    ap.add_argument("--shoulder", type=int, default=30)
    ap.add_argument("--head", type=box, default=(186, 24, 199, 36))
    ap.add_argument("--tail", type=box, default=(0, 18, 5, 30))
    ap.add_argument("--wheels", type=box, default=(46, 160, 52))
    ap.add_argument("--beam", type=box, default=(200, 31))
    ap.add_argument("--tail-glow", type=int, default=21)
    a = ap.parse_args()
    w, h, rows = decode_png(open(a.png, "rb").read())
    if (w, h) != (W, H):
        sys.exit("gen_mcqueen_sprite: the PNG must be %d x %d (it is %d x %d)" % (W, H, w, h))
    kinds, px = [], []
    for y in range(h):
        for x in range(w):
            k = classify(rows[y][x], x, y, a.head, a.tail, a.shoulder)
            kinds.append(k)
            px.append(rgb565(*rows[y][x][:3]) if k else 0)
    anchors = (a.wheels[0], a.wheels[1], a.wheels[2], a.beam[0], a.beam[1], a.tail_glow)
    blob = pack(w, h, kinds, px, anchors)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    print("gen_mcqueen_sprite: %s  %d bytes, %d opaque pixels" % (os.path.relpath(OUT), len(blob),
                                                                  sum(1 for k in kinds if k)))


if __name__ == "__main__":
    main()
