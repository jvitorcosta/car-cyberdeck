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
        self.assertEqual(g.ART_X, header_const("ART_X"))
        self.assertEqual(g.ART_W, header_const("ART_W"))
        self.assertEqual(g.H, header_const("ART_H"))
        self.assertEqual(g.LOGO_W, header_const("LOGO_W"))
        self.assertEqual(g.LOGO_H, header_const("LOGO_H"))
        self.assertEqual(g.COLOURS, header_const("COLOURS"))
        self.assertEqual(g.ART_X + g.ART_W, header_const("W"))   # the strip reaches the right edge
        # the strip lies inside the girl's cel box once the art is scaled to the full height
        self.assertLessEqual(g.SRC_CEL_X * g.FULL_W / g.SRC_W, g.FULL_W - g.ART_W)


@NO_PIL
class Pack(unittest.TestCase):
    def test_header_palette_cels_logo_and_fonts(self):
        palette = [(i, 255 - i, 7) for i in range(256)]
        cels = [bytes([k]) * (g.ART_W * g.H) for k in range(19)]
        logo = bytes([200]) * (g.LOGO_W * g.LOGO_H)
        glyph = ("1", 2, bytes([0, 255, 128, 255, 0, 64]))
        fonts = [(3, 1, 2, [glyph])] * 4
        blob = g.pack(palette, cels, logo, fonts)
        self.assertEqual(blob[:4], b"SKR2")
        self.assertEqual(struct.unpack_from("<8H", blob, 4), (160, 160, 240, 19, 4, 108, 26, 0))
        self.assertEqual(blob[20:23], bytes((0, 255, 7)))
        at = 20 + 256 * 3
        self.assertEqual(blob[at + 5 * g.ART_W * g.H], 5)   # cel 5's first index
        at += 19 * g.ART_W * g.H
        self.assertEqual(blob[at], 200)                     # the logo
        at += g.LOGO_W * g.LOGO_H
        self.assertEqual(struct.unpack_from("<BbBB", blob, at), (3, 1, 1, 2))
        ch, w, _, off = struct.unpack_from("<BBHI", blob, at + 4)
        self.assertEqual((chr(ch), w), ("1", 2))
        self.assertEqual(blob[off:off + 6], glyph[2])

    def test_layers_quantize_to_one_palette(self):
        with tempfile.TemporaryDirectory() as d:
            bd = Image.new("RGB", (g.SRC_W, g.SRC_H), (255, 40, 112))
            bd.paste((255, 255, 255), (100, 30, 200, 60))   # white "letters" in the logo box
            bd.save(os.path.join(d, "suki-layer-backdrop.png"))
            for i, t in enumerate(["39.5", "40", "41"] + [str(42 + k) for k in range(6)]):
                Image.new("RGB", (g.SRC_W - g.SRC_CEL_X, g.SRC_H), (250, 215, 190 - i)).save(
                    os.path.join(d, "suki-layer-cel-%s.png" % t))
            backdrop, cels = g.layers(d)
            self.assertEqual(len(cels), 9)
            self.assertEqual(cels[0].size, (g.ART_W, g.H))
            palette, cidx = g.quantize(cels)
            self.assertEqual(len(palette), 256)
            self.assertEqual(len(cidx[0]), g.ART_W * g.H)
            r, gg, b = palette[cidx[0][g.ART_W * 100 + 80]]
            self.assertTrue(r > 240 and gg > 200)   # the cel's skin stays skin
            alpha = g.logo(backdrop)
            self.assertEqual(len(alpha), g.LOGO_W * g.LOGO_H)
            self.assertEqual(alpha[0], 0)                                  # the pink: no letter
            self.assertEqual(max(alpha), 255)                              # the white: a letter

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
