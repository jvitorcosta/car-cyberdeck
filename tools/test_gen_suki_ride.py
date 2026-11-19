# tools/test_gen_suki_ride.py
"""Host tests for tools/gen_suki_ride.py (no film art needed). Run: python tools/test_gen_suki_ride.py"""
import os
import re
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:   # the art tool needs Pillow; CI's tools job doesn't install it, so these skip there
    from PIL import Image
    import gen_suki_ride as g  # noqa: E402
except ImportError:
    Image = g = None
NO_PIL = unittest.skipIf(g is None, "needs Pillow (pip install pillow)")
HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "util", "car", "sukiride.h")


def header_const(name):
    text = open(HEADER, encoding="utf-8").read()
    m = re.search(r"\b%s = (\d+)" % name, text)
    return int(m.group(1))


@NO_PIL
class Layout(unittest.TestCase):
    def test_constants_match_sukiride_h(self):
        self.assertEqual(g.W, header_const("ART_W"))
        self.assertEqual(g.H, header_const("ART_H"))
        self.assertEqual(g.CEL_X, header_const("CEL_X"))
        self.assertEqual(g.CEL_W, header_const("CEL_W"))
        self.assertEqual(g.COLOURS, header_const("COLOURS"))
        self.assertEqual(g.W - g.CEL_X, g.CEL_W)   # the strip reaches the right edge


@NO_PIL
class Pack(unittest.TestCase):
    def test_header_palette_indices_and_fonts(self):
        palette = [(i, 255 - i, 7) for i in range(256)]
        bg = bytes([1]) * (g.W * g.H)
        cels = [bytes([k]) * (g.CEL_W * g.H) for k in range(19)]
        glyph = ("1", 2, bytes([0, 255, 128, 255, 0, 64]))
        fonts = [(3, 1, 2, [glyph])] * 4
        blob = g.pack(palette, bg, cels, fonts)
        self.assertEqual(blob[:4], b"SKR1")
        self.assertEqual(struct.unpack_from("<6H", blob, 4), (320, 182, 193, 127, 19, 4))
        self.assertEqual(blob[16:19], bytes((0, 255, 7)))
        at = 16 + 256 * 3
        self.assertEqual(blob[at], 1)
        at += g.W * g.H
        self.assertEqual(blob[at + 5 * g.CEL_W * g.H], 5)   # cel 5's first index
        at += 19 * g.CEL_W * g.H
        self.assertEqual(struct.unpack_from("<BbBB", blob, at), (3, 1, 1, 2))
        ch, w, _, off = struct.unpack_from("<BBHI", blob, at + 4)
        self.assertEqual((chr(ch), w), ("1", 2))
        self.assertEqual(blob[off:off + 6], glyph[2])

    def test_layers_quantize_to_one_palette(self):
        with tempfile.TemporaryDirectory() as d:
            Image.new("RGB", (g.SRC_W, g.SRC_H), (255, 40, 112)).save(os.path.join(d, "suki-layer-backdrop.png"))
            for i, t in enumerate(["39.5", "40", "41"] + [str(42 + k) for k in range(6)]):
                Image.new("RGB", (g.SRC_W - g.SRC_CEL_X, g.SRC_H), (250, 215, 190 - i)).save(
                    os.path.join(d, "suki-layer-cel-%s.png" % t))
            bd, cels = g.layers(d)
            self.assertEqual(bd.size, (g.W, g.H))
            self.assertEqual(len(cels), 9)
            self.assertEqual(cels[0].size, (g.CEL_W, g.H))
            palette, bidx, cidx = g.quantize(bd, cels)
            self.assertEqual(len(palette), 256)
            self.assertEqual(len(bidx), g.W * g.H)
            self.assertEqual(len(cidx[0]), g.CEL_W * g.H)
            r, gg, b = palette[bidx[0]]
            self.assertTrue(r > 240 and gg < 60)   # the pink stays pink in the palette

    def test_glyphs_are_alpha_cells(self):
        font = next((f for f in g.DEFAULT_FONTS if os.path.isfile(f)), None)
        if not font:
            self.skipTest("no default font on this machine")
        gl, cell, cap = g.glyphs(font, 12, "0-")
        self.assertEqual(cell, 12)
        self.assertTrue(0 < cap <= cell)
        for ch, w, px in gl:
            self.assertEqual(len(px), w * cell)
            self.assertTrue(max(px) > 200, ch)   # something drawn


if __name__ == "__main__":
    unittest.main()
