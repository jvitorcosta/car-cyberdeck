# tools/test_gen_suki_hud.py
"""Host tests for tools/gen_suki_hud.py (no film art needed). Run: python tools/test_gen_suki_hud.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:   # the art tool needs Pillow; CI's tools job doesn't install it, so these skip there
    from PIL import Image
    import gen_suki_hud as g  # noqa: E402
except ImportError:
    Image = g = None
NO_PIL = unittest.skipIf(g is None, "needs Pillow (pip install pillow)")


@NO_PIL
class Pack(unittest.TestCase):
    def test_layout_matches_sukihud_h(self):
        bg = Image.new("RGB", (g.W, g.H), (255, 0, 0))
        glyph = Image.new("RGBA", (2, 3), (0, 0, 255, 128))
        fonts = [(3, 1, [("1", glyph), (" ", Image.new("RGBA", (1, 3), (0, 0, 0, 0)))])] * len(g.FONTS)
        blob = g.pack(bg, fonts)
        self.assertEqual(blob[:4], b"SUK1")
        self.assertEqual(struct.unpack_from("<HHHH", blob, 4), (320, 240, len(g.FONTS), 0))
        self.assertEqual(blob[12:14], b"\xf8\x00")   # red, big-endian
        at = 12 + 320 * 240 * 2
        h, track, n, zero = struct.unpack_from("<BBBB", blob, at)
        self.assertEqual((h, track, n, zero), (3, 1, 2, 0))
        ch, w, _, off = struct.unpack_from("<BBHI", blob, at + 4)
        self.assertEqual((chr(ch), w), ("1", 2))
        self.assertEqual(blob[off:off + 3], b"\x00\x1f\x80")   # blue, alpha 128
        ch, w, _, off = struct.unpack_from("<BBHI", blob, at + 4 + 8)   # the second glyph
        self.assertEqual((chr(ch), w), (" ", 1))
        self.assertTrue(off + 1 * 3 * 3 <= len(blob))
        # the tables come before every glyph's pixels; the pixels fill the rest exactly
        tables = len(g.FONTS) * (4 + 2 * 8)
        self.assertEqual(len(blob), at + tables + len(g.FONTS) * (2 * 3 * 3 + 1 * 3 * 3))

    def test_rgb565_is_big_endian(self):
        self.assertEqual(g.rgb565_be((255, 255, 255)), b"\xff\xff")
        self.assertEqual(g.rgb565_be((0, 255, 0)), b"\x07\xe0")


@NO_PIL
class Erase(unittest.TestCase):
    def test_ink_becomes_a_blend_of_its_ends(self):
        img = Image.new("RGB", (10, 1), (10, 10, 10))
        img.putpixel((9, 0), (40, 40, 40))
        for x in (4, 5):
            img.putpixel((x, 0), (255, 0, 0))
        g.erase_by_rows(img, (1, 0, 9, 1), lambda p: p[0] > 200, grow=0)
        self.assertEqual(img.getpixel((4, 0)), (10, 10, 10))   # its ends are both background
        self.assertEqual(img.getpixel((5, 0)), (10, 10, 10))


if __name__ == "__main__":
    unittest.main()
