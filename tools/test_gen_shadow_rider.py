"""Host tests for tools/gen_shadow_rider.py (no network). Run: python tools/test_gen_shadow_rider.py"""
import os
import struct
import sys
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_shadow_rider as g  # noqa: E402

BG = g.SHEET_BG + (255,)


def png(rows, filters=None):
    """An 8-bit RGBA PNG of rows of (r, g, b, a); filters: one filter byte per row, with the row
    already encoded for it (tests pass raw rows with filter 0 unless they hand-encode)."""
    h, w = len(rows), len(rows[0]) // 4 if isinstance(rows[0], (bytes, bytearray)) else len(rows[0])
    raw = b""
    for i, r in enumerate(rows):
        data = r if isinstance(r, (bytes, bytearray)) else bytes(c for p in r for c in p)
        raw += bytes([filters[i] if filters else 0]) + data
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class Png(unittest.TestCase):
    def test_reads_rgba(self):
        w, h, px = g.read_png(png([[(1, 2, 3, 255), (4, 5, 6, 0)]]))
        self.assertEqual((w, h), (2, 1))
        self.assertEqual(px[0], [(1, 2, 3, 255), (4, 5, 6, 0)])

    def test_sub_and_up_filters(self):
        # row 0 Sub: second pixel stored as the difference from the first; row 1 Up: from row 0
        row0 = bytes([10, 20, 30, 255, 5, 5, 5, 0])           # -> (10,20,30,255), (15,25,35,255)
        row1 = bytes([1, 1, 1, 0, 0, 0, 0, 0])                # -> (11,21,31,255), (15,25,35,255)
        _, _, px = g.read_png(png([row0, row1], filters=[1, 2]))
        self.assertEqual(px[0], [(10, 20, 30, 255), (15, 25, 35, 255)])
        self.assertEqual(px[1], [(11, 21, 31, 255), (15, 25, 35, 255)])

    def test_paeth_filter(self):
        # row 1 Paeth over row 0: with a = left, b = up, c = up-left, first pixel predicts b
        row0 = bytes([10, 10, 10, 10, 20, 20, 20, 20])
        row1 = bytes([1, 1, 1, 1, 0, 0, 0, 0])
        _, _, px = g.read_png(png([row0, row1], filters=[0, 4]))
        self.assertEqual(px[1][0], (11, 11, 11, 11))
        self.assertEqual(px[1][1], (20, 20, 20, 20))    # a=11, b=20, c=10: p=21, nearest is b


class Cut(unittest.TestCase):
    def sheet(self):
        """A 60 x 20 sheet on the background: two 13x13 bodies and a loose 2x2 puff left of the
        second (a detached flame)."""
        px = [[BG] * 60 for _ in range(20)]
        for y in range(3, 16):
            for x in range(2, 15):
                px[y][x] = (200, 0, 0, 255)
            for x in range(40, 53):
                px[y][x] = (0, 0, 200, 255)
        for y in range(12, 14):
            for x in range(33, 35):
                px[y][x] = (255, 200, 0, 255)
        return px

    def test_bodies_with_loose_bits_joined(self):
        boxes = g.cut_row(self.sheet(), 0, 20, 60)
        self.assertEqual(boxes, [(2, 3, 15, 16), (33, 3, 53, 16)])


class Build(unittest.TestCase):
    def test_frames_aligned_bottom_right_with_a_shared_palette(self):
        px = [[BG] * 10 for _ in range(6)]
        px[5][1] = (255, 0, 0, 255)                         # frame A: 2 wide, 1 high, bottom row
        px[5][2] = (0, 0, 255, 255)
        px[3][7] = (255, 0, 0, 255)                         # frame B: 1 wide, 3 high
        px[4][7] = (255, 0, 0, 255)
        px[5][7] = (0, 0, 255, 255)
        blob = g.build(px, [(1, 5, 3, 6)], (7, 3, 8, 6))    # one run frame, then the standing one
        run, w, h, zero = struct.unpack("<BBBB", blob[4:8])
        self.assertEqual(blob[:4], b"SHR1")
        self.assertEqual((run, w, h, zero), (1, 2, 3, 0))
        pal = struct.unpack("<16H", blob[8:40])
        self.assertEqual(pal[1], 0xF800)                    # first colour met: red
        self.assertEqual(pal[2], 0x001F)                    # then blue
        frames = blob[40:]
        self.assertEqual(len(frames), 2 * (w // 2 + (w & 1)) * h)
        # frame A on the bottom row (y 2): red, blue -> nibbles 1, 2; rows above transparent
        self.assertEqual(frames[0:3], bytes([0x00, 0x00, 0x12]))
        # frame B right-aligned in the 2-wide box: column 1; red, red, blue from the top
        self.assertEqual(frames[3:6], bytes([0x01, 0x01, 0x02]))

    def test_too_many_colours_is_refused(self):
        px = [[(i * 10, 0, 0, 255) for i in range(17)]]
        with self.assertRaises(ValueError):
            g.build(px, [(0, 0, 17, 1)], (0, 0, 1, 1))

    def test_rgb565(self):
        self.assertEqual(g.rgb565(255, 255, 255), 0xFFFF)
        self.assertEqual(g.rgb565(0, 255, 0), 0x07E0)


if __name__ == "__main__":
    unittest.main()
