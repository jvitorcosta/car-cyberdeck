"""Host tests for tools/gen_pokeface.py (no network; needs Pillow 12.1+, skipped without it, as on
CI). Run: python tools/test_gen_pokeface.py"""
import os
import sys
import unittest

try:
    from PIL import Image
except ImportError:
    Image = None
if Image is None or not hasattr(Image.Image, "get_flattened_data"):
    raise unittest.SkipTest("gen_pokeface needs Pillow 12.1+ (pip install -r tools/requirements.txt)")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_pokeface as g  # noqa: E402

PAL = [(0, 0, 0), (248, 64, 32), (16, 200, 16)] + [(0, 0, 0)] * 13


def paletted(w, h):
    im = Image.new("P", (w, h), 0)
    im.putpalette([c for rgb in PAL for c in rgb])
    return im


class Palette(unittest.TestCase):
    def test_read_jasc(self):
        self.assertEqual(g.read_jasc("JASC-PAL\n0100\n2\n0 0 0\n248 64 32\n"), [(0, 0, 0), (248, 64, 32)])


class Frames(unittest.TestCase):
    def test_two_frame_stack(self):
        im = paletted(64, 128)
        im.putpixel((0, 0), 1)
        im.putpixel((0, 64), 2)
        f = g.frames(im)
        self.assertEqual(len(f), 2)
        self.assertEqual([f[0].getpixel((0, 0)), f[1].getpixel((0, 0))], [1, 2])

    def test_single_frame_sheet(self):
        im = paletted(64, 64)
        im.putpixel((5, 5), 1)
        f = g.frames(im)
        self.assertEqual(len(f), 2)
        self.assertEqual(f[0].tobytes(), f[1].tobytes())
        self.assertEqual(g.sheet([g.colourise(x, PAL) for x in f]).size, (128, 64))

    def test_colourise_index0_transparent(self):
        im = paletted(2, 1)
        im.putpixel((1, 0), 1)
        out = g.colourise(im, PAL)
        self.assertEqual(out.getpixel((0, 0))[3], 0)
        self.assertEqual(out.getpixel((1, 0)), (255, 85, 0, 255))

    def test_sheet_mirrors(self):
        a = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        a.putpixel((0, 10), (255, 0, 0, 255))
        s = g.sheet([a, a])
        self.assertEqual(s.getpixel((63, 10)), (255, 0, 0, 255))
        self.assertEqual(s.getpixel((127, 10)), (255, 0, 0, 255))
        self.assertEqual(s.getpixel((0, 10))[3], 0)


class Strict(unittest.TestCase):
    def test_colourise_rejects_rgba(self):
        with self.assertRaises(ValueError):
            g.colourise(Image.new("RGBA", (2, 2)), PAL)

    def test_colourise_rejects_index_beyond_palette(self):
        im = paletted(2, 1)
        im.putpixel((1, 0), 5)
        with self.assertRaises(ValueError):
            g.colourise(im, PAL[:3])


class Font(unittest.TestCase):
    def test_read_charmap(self):
        self.assertEqual(g.read_charmap("'A' = BB\n'0' = A1\n@ a comment\n"), {"A": 0xBB, "0": 0xA1})

    def test_read_widths(self):
        src = "const u8 gFontNormalLatinGlyphWidths[] = {\n 3, 6,\n 5 };\nconst u8 gOther[] = { 9 };"
        self.assertEqual(g.read_widths(src), [3, 6, 5])

    def test_glyph_cut(self):
        sheet = Image.new("RGBA", (256, 256), (255, 255, 255, 255))
        code = 0xBB
        x0, y0 = (code % 16) * 16, (code // 16) * 16
        sheet.putpixel((x0 + 1, y0 + 2), g.FONT_INK + (255,))
        gl = g.glyph(sheet, code, 6)
        self.assertEqual(gl.width, 6)
        self.assertEqual(gl.getpixel((1, 2)), (255, 255, 255, 255))
        self.assertEqual(gl.getpixel((0, 0))[3], 0)

    def test_glyphs_cover_the_appeared_line(self):
        for c in "A wild appeared! shiny":
            self.assertIn(c, g.GLYPHS)

    def test_degree_glyph_synthesised(self):
        text, _ = g.bmfont({"°": g.degree(10)}, 1, "t")
        self.assertIn("char id=176 ", text)

    def test_bmfont_scale(self):
        gl = Image.new("RGBA", (2, 3), (255, 255, 255, 255))
        text, page = g.bmfont({"A": gl}, 2, "t")
        self.assertIn("width=4 height=6", text)
        self.assertIn("xadvance=6", text)
        self.assertIn('file="t.png"', text)
        self.assertGreaterEqual(page.height, 6)


class Field(unittest.TestCase):
    def test_day_field_is_vivid_and_solid(self):
        im = g.field(night=False)
        self.assertEqual(im.size, (260, 260))
        # solid colours, no checker: pale dithered colours washed out on the watch's screen
        self.assertEqual(im.getpixel((10, 10)), (0x55, 0xAA, 0xFF))           # sky
        self.assertEqual(im.getpixel((11, 10)), (0x55, 0xAA, 0xFF))
        self.assertEqual(im.getpixel((10, g.HORIZON)), (0x00, 0x55, 0x00))    # horizon
        self.assertEqual(im.getpixel((10, 200)), (0x00, 0xAA, 0x00))          # grass
        self.assertEqual(im.getpixel((11, 200)), (0x00, 0xAA, 0x00))

    def test_night_field_is_dark(self):
        im = g.field(night=True)
        self.assertTrue(all(max(im.getpixel((x, y))) <= 0x55 for x, y in [(10, 10), (11, 10), (10, 200), (11, 200)]))

    def test_gen_xml_has_fields(self):
        xml = g.gen_xml()
        self.assertIn('<bitmap id="FieldDay" filename="field_day.png"', xml)
        self.assertIn('<bitmap id="FieldNight" filename="field_night.png"', xml)

    def test_fields_carry_their_palette(self):
        xml = g.gen_xml()
        day = xml[xml.index('id="FieldDay"'):xml.index('id="FieldNight"')]
        self.assertEqual(day.count("<color>"), 3)
        for c in ("55AAFF", "005500", "00AA00"):
            self.assertIn("<color>%s</color>" % c, day)


class Resources(unittest.TestCase):
    def test_gen_xml_has_shiny_rewards(self):
        xml = g.gen_xml()
        self.assertIn('<bitmap id="Shiny2" filename="mon2s.png"', xml)
        self.assertIn('filename="mon20s.png"', xml)
        self.assertEqual(g.REWARDS, [2, 5, 8, 11, 14, 17, 20])

    def test_species(self):
        self.assertEqual(len(g.SPECIES), 21)
        self.assertEqual(g.SPECIES[0], ("blaziken", "BLAZIKEN"))
        self.assertEqual(g.SPECIES[3], ("ho_oh", "HO-OH"))
        self.assertEqual(g.SPECIES[20], ("zygarde", "ZYGARDE"))

    def test_gen_xml(self):
        xml = g.gen_xml()
        self.assertEqual(xml.count("<bitmap "), 30)
        self.assertEqual(xml.count("<font "), 3)
        for s in ('id="Mon0"', 'id="Mon20"', 'id="EmeraldTime"', 'filename="emerald2.fnt"', 'filename="emerald3.fnt"'):
            self.assertIn(s, xml)


if __name__ == "__main__":
    unittest.main()
