# tools/test_gen_mcqueen_sprite.py
"""Host tests for tools/gen_mcqueen_sprite.py. Run: python tools/test_gen_mcqueen_sprite.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_mcqueen_sprite as gm  # noqa: E402


class Classify(unittest.TestCase):
    HEAD, TAIL = (190, 25, 199, 34), (0, 18, 4, 28)

    def k(self, rgba, x, y):
        return gm.classify(rgba, x, y, self.HEAD, self.TAIL, 30)

    def test_transparent(self):
        self.assertEqual(self.k((200, 0, 0, 0), 50, 10), 0)

    def test_lamps_win(self):
        self.assertEqual(self.k((250, 240, 120, 255), 195, 30), 4)
        self.assertEqual(self.k((200, 10, 10, 255), 2, 20), 5)

    def test_red_paint_above_and_below_the_shoulder(self):
        self.assertEqual(self.k((210, 20, 25, 255), 80, 10), 1)
        self.assertEqual(self.k((210, 20, 25, 255), 80, 40), 2)

    def test_other(self):
        self.assertEqual(self.k((30, 40, 60, 255), 80, 10), 3)     # glass
        self.assertEqual(self.k((240, 200, 40, 255), 80, 40), 3)   # the yellow bolt / 95


class Pack(unittest.TestCase):
    def test_layout(self):
        kinds = [4, 0, 0, 0, 0, 2]
        px = [0xF800, 0, 0, 0, 0, 0x07E0]
        b = gm.pack(3, 2, kinds, px, (0, 2, 1, 3, 0, 1))
        self.assertEqual(b[:4], b"CAR1")
        self.assertEqual(struct.unpack("<8H", b[4:20]), (3, 2, 0, 2, 1, 3, 0, 1))
        self.assertEqual(b[20:23], bytes([0x40, 0x00, 0x02]))
        self.assertEqual(struct.unpack("<6H", b[23:]), (0xF800, 0, 0, 0, 0, 0x07E0))
        self.assertEqual(len(b), 20 + 3 + 12)

    def test_rgb565(self):
        self.assertEqual(gm.rgb565(255, 0, 0), 0xF800)
        self.assertEqual(gm.rgb565(0, 255, 0), 0x07E0)
        self.assertEqual(gm.rgb565(255, 255, 255), 0xFFFF)


if __name__ == "__main__":
    unittest.main()
