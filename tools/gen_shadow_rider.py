"""Build data/shadow.bin: Shadow skating on the S3's firmware-update rail.
Run: python tools/gen_shadow_rider.py

Cuts Shadow's running loop and one standing frame out of his Sonic Battle (GBA, 2003) sprite
sheet: the sheet ripped by QuadFactor, as carried by github.com/SingularMJ/Sonic-Battle-Unlimited-
Edition (pinned below). It is fetched once into sonic-sprites/shadow/ (git-ignored); the art is
SEGA's / Sonic Team's, used locally, never committed. Without data/shadow.bin the rail keeps its
two jet glows (spec docs/superpowers/specs/2026-10-22-shadow-update-screen-design.md, stage 2).

Format "SHR1": u8 run frames, u8 w, u8 h, u8 0, 16 x u16 RGB565 palette (LE, index 0
transparent), then run + 1 frames (the run loop, then the standing frame) as w x h 4-bit indices,
row by row, high nibble = left pixel. Frames share one box, aligned bottom-right (his feet and
the front of his body stay put; the flames trail left).
"""
import os
import struct
import sys
import urllib.request
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SHEET = os.path.join(ROOT, "sonic-sprites", "shadow", "Shadow.png")
OUT = os.path.join(ROOT, "data", "shadow.bin")
REPO, REV = "SingularMJ/Sonic-Battle-Unlimited-Edition", "1627f5c4ea1a8d45167e1bc631a4ffbeba38adb4"
SHEET_PATH = "Sonic Battle Prototype/Sprites/Shadow.png"
SHEET_BG = (215, 0, 204)        # the sheet's magenta
RUN_ROW, RUN_FRAMES = (98, 136), list(range(4, 12))   # "Running": the full-speed skate, 8 frames
IDLE_ROW, STAND = (8, 44), 0                           # "Idle": his first frame, for "Update failed"
SCAN_W = 1000                                          # sprites sit left of the row labels
BODY_MIN = 150                                         # pixels: smaller bits are loose flame puffs


def read_png(data):
    """An 8-bit RGBA, non-interlaced PNG -> (w, h, rows of (r, g, b, a))."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat = 8, b""
    w = h = 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
            if (depth, ctype, inter) != (8, 6, 0):
                raise ValueError("only 8-bit RGBA, non-interlaced")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw, stride, rows, prev = zlib.decompress(idat), w * 4, [], bytearray(w * 4)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(w)])
        prev = line
    return w, h, rows


def _fg(p):
    return p[3] > 0 and p[:3] != SHEET_BG


def cut_row(px, y0, y1, x_max):
    """A sheet row's frames as boxes (x0, y0, x1, y1), left to right: each body (8-connected,
    BODY_MIN pixels or more) with the loose bits (flame puffs) whose centre is nearest it."""
    seen, comps = set(), []
    for y in range(y0, y1):
        for x in range(x_max):
            if (x, y) in seen or not _fg(px[y][x]):
                continue
            stack, pts = [(x, y)], []
            seen.add((x, y))
            while stack:
                cx, cy = stack.pop()
                pts.append((cx, cy))
                for nx in (cx - 1, cx, cx + 1):
                    for ny in (cy - 1, cy, cy + 1):
                        if y0 <= ny < y1 and 0 <= nx < x_max and (nx, ny) not in seen and _fg(px[ny][nx]):
                            seen.add((nx, ny))
                            stack.append((nx, ny))
            comps.append(pts)
    box = lambda pts: [min(p[0] for p in pts), min(p[1] for p in pts),
                       max(p[0] for p in pts) + 1, max(p[1] for p in pts) + 1]
    bodies = sorted(box(c) for c in comps if len(c) >= BODY_MIN)
    for c in comps:
        if len(c) >= BODY_MIN or not bodies:
            continue
        cx = sum(p[0] for p in c) / len(c)
        b, s = min(bodies, key=lambda b: abs((b[0] + b[2]) / 2 - cx)), box(c)
        b[0], b[1], b[2], b[3] = min(b[0], s[0]), min(b[1], s[1]), max(b[2], s[2]), max(b[3], s[3])
    return [tuple(b) for b in bodies]


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def build(px, run_boxes, stand_box):
    """The SHR1 file: run_boxes then stand_box, aligned bottom-right in their shared box."""
    boxes = list(run_boxes) + [stand_box]
    w = max(x1 - x0 for x0, _, x1, _ in boxes)
    h = max(y1 - y0 for _, y0, _, y1 in boxes)
    palette, frames = [], b""
    for x0, y0, x1, y1 in boxes:
        grid = [[0] * w for _ in range(h)]
        for y in range(y0, y1):
            for x in range(x0, x1):
                p = px[y][x]
                if not _fg(p):
                    continue
                c = rgb565(*p[:3])
                if c not in palette:
                    palette.append(c)
                    if len(palette) > 15:
                        raise ValueError("more than 15 colours: they don't fit a 4-bit palette")
                grid[h - (y1 - y)][w - (x1 - x)] = palette.index(c) + 1
        for row in grid:
            row = row + [0] * (len(row) & 1)
            frames += bytes(row[i] << 4 | row[i + 1] for i in range(0, len(row), 2))
    pal = [0] + palette + [0] * (15 - len(palette))
    return b"SHR1" + struct.pack("<BBBB", len(run_boxes), w, h, 0) + struct.pack("<16H", *pal) + frames


def fetch():
    if os.path.isfile(SHEET):
        return
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    url = "https://raw.githubusercontent.com/%s/%s/%s" % (REPO, REV, SHEET_PATH.replace(" ", "%20"))
    print("gen_shadow_rider: fetching", SHEET_PATH)
    with urllib.request.urlopen(url, timeout=30) as r, open(SHEET, "wb") as f:
        f.write(r.read())


def main():
    try:
        fetch()
    except OSError as e:
        sys.exit("gen_shadow_rider: couldn't fetch the sheet (%s)" % e)
    w, h, px = read_png(open(SHEET, "rb").read())
    run = cut_row(px, *RUN_ROW, SCAN_W)
    idle = cut_row(px, *IDLE_ROW, SCAN_W)
    blob = build(px, [run[i] for i in RUN_FRAMES], idle[STAND])
    with open(OUT, "wb") as f:
        f.write(blob)
    print("gen_shadow_rider: wrote %s (%d run frames, %dx%d, %d B)" % (OUT, blob[4], blob[5], blob[6], len(blob)))


if __name__ == "__main__":
    main()
