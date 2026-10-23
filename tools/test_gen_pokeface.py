"""Host tests for tools/gen_pokeface.py (no network). Run: python tools/test_gen_pokeface.py"""
import os
import sys
import unittest

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_pokeface as g  # noqa: E402


def tagged(i, size=(8, 8)):
    """A frame whose top-left pixel's red channel carries its index, so picks can be read back."""
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    im.putpixel((0, 0), (i, 0, 0, 255))
    return im


class PickFrames(unittest.TestCase):
    def test_pick_frames_spread(self):
        got = [f.getpixel((0, 0))[0] for f in g.pick_frames([tagged(i) for i in range(12)], 6)]
        self.assertEqual(got, [0, 2, 4, 6, 8, 10])

    def test_short_gif_cycles(self):
        got = [f.getpixel((0, 0))[0] for f in g.pick_frames([tagged(i) for i in range(4)], 6)]
        self.assertEqual(got, [0, 1, 2, 3, 0, 1])


class Cut(unittest.TestCase):
    def test_crop_union(self):
        a = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
        b = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
        a.putpixel((10, 20), (255, 0, 0, 255))
        b.putpixel((50, 70), (0, 255, 0, 255))
        out = g.crop_union([a, b])
        self.assertEqual([im.size for im in out], [(41, 51), (41, 51)])
        self.assertEqual(out[0].getpixel((0, 0)), (255, 0, 0, 255))
        self.assertEqual(out[1].getpixel((40, 50)), (0, 255, 0, 255))

    def test_halve_is_nearest(self):
        im = Image.new("RGBA", (4, 4))
        for y in range(4):
            for x in range(4):
                im.putpixel((x, y), (255, 0, 0, 255) if (x + y) % 2 else (0, 0, 255, 255))
        out = g.halve(im)
        self.assertEqual(out.size, (2, 2))
        self.assertLessEqual(set(out.getdata()), {(255, 0, 0, 255), (0, 0, 255, 255)})


class Convert(unittest.TestCase):
    def frames(self):
        out = []
        for i in range(3):
            im = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
            for y in range(20, 60):
                for x in range(30, 70):
                    im.putpixel((x, y), (0x60, 0x10, 0xF0, 255))
            out.append(im)
        return out

    def test_convert_full_size_keeps_pixels(self):
        out = g.convert(self.frames(), half=False)
        self.assertEqual(len(out), g.FRAMES)
        self.assertEqual(out[0].size, (40, 40))
        self.assertEqual(out[0].getpixel((0, 0)), (0x55, 0x00, 0xFF, 255))

    def test_convert_half(self):
        self.assertEqual(g.convert(self.frames(), half=True)[0].size, (20, 20))

    def test_small_ones_full_big_ones_half(self):
        half = {name: h for name, _, _, h, _ in g.POKEMON}
        self.assertEqual(half, {"charmander": False, "charmeleon": False, "charizard": True,
                                "charizard-megax": True, "charizard-megay": True})

    def test_mega_y_is_the_still_gen5_sprite(self):
        src = {name: s for name, _, _, _, s in g.POKEMON}
        self.assertEqual(src["charizard-megay"], "https://play.pokemonshowdown.com/sprites/gen5/charizard-megay.png")
        self.assertEqual(src["charmander"], "https://play.pokemonshowdown.com/sprites/gen5ani/charmander.gif")


class Colour(unittest.TestCase):
    def test_snap(self):
        im = Image.new("RGBA", (2, 1))
        im.putpixel((0, 0), (0x60, 0x10, 0xF0, 255))
        im.putpixel((1, 0), (1, 2, 3, 100))
        out = g.snap(im)
        self.assertEqual(out.getpixel((0, 0)), (0x55, 0x00, 0xFF, 255))
        self.assertEqual(out.getpixel((1, 0))[3], 0)

    def test_darken(self):
        im = Image.new("RGBA", (1, 1), (0xFF, 0xAA, 0x55, 255))
        self.assertEqual(g.darken(im).getpixel((0, 0)), (0xAA, 0x55, 0x00, 255))


class Tail(unittest.TestCase):
    def test_tail_is_the_flame_core_centre(self):
        im = Image.new("RGBA", (40, 30), (255, 170, 0, 255))   # an orange body
        for y in range(4, 9):
            for x in range(30, 35):
                im.putpixel((x, y), (255, 255, 0, 255))        # the yellow core
        self.assertEqual(g.tail(im, blue=False), [12, -9])     # centre (32, 6) vs frame centre (20, 15)

    def test_blue_tail_ignores_the_mouth_flames(self):
        im = Image.new("RGBA", (50, 40), (0, 0, 0, 255))
        for x, y in [(5, 5), (6, 5), (44, 24), (46, 26)]:      # mouth on the left, tail on the right
            im.putpixel((x, y), (170, 255, 255, 255))
        self.assertEqual(g.tail(im, blue=True), [20, 5])

    def test_no_flame_found_points_at_the_top(self):
        self.assertEqual(g.tail(Image.new("RGBA", (40, 30), (0, 0, 0, 255)), blue=False), [0, -15])


class Xml(unittest.TestCase):
    def test_tails_json(self):
        xml = g.sprites_xml([[[1, 2]] * 6] * 5)
        self.assertIn('<jsonData id="Tails">[[[1, 2], [1, 2], [1, 2], [1, 2], [1, 2], [1, 2]], ', xml)

    def test_sprites_xml_lists_every_frame(self):
        xml = g.sprites_xml([[[0, 0]] * 6] * 5)
        self.assertIn('<bitmap id="Charmander0" filename="charmander_0.png" dithering="none"/>', xml)
        self.assertIn('<bitmap id="MegaY5" filename="charizard-megay_5.png" dithering="none"/>', xml)
        self.assertIn('<bitmap id="MegaXSleep" filename="charizard-megax_sleep.png" dithering="none"/>', xml)
        self.assertEqual(xml.count("<bitmap "), 33)


if __name__ == "__main__":
    unittest.main()
