"""Host tests for tools/gen_slz_backdrop.py (no network). Run: python tools/test_gen_slz_backdrop.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_slz_backdrop as g  # noqa: E402


class Kosinski(unittest.TestCase):
    def test_kosinski_literals_and_end(self):
        # descriptor 1,1 (two literals), 0,1 (full copy) whose count byte 0 ends the stream
        self.assertEqual(g.kosinski(bytes([0x0B, 0x00, 0x41, 0x42, 0x00, 0xF0, 0x00])), b"AB")

    def test_kosinski_inline_copy(self):
        # a literal, then an inline copy of 4 from offset -1, then the end
        self.assertEqual(g.kosinski(bytes([0x49, 0x00, 0x41, 0xFF, 0x00, 0xF0, 0x00])), b"AAAAA")


class Enigma(unittest.TestCase):
    def test_enigma_incrementing_and_common(self):
        # 8-bit inline values, no flag bits, incrementing 0x0010, common 0x0000:
        # 3 incrementing, 2 common, end
        data = bytes([8, 0, 0x00, 0x10, 0x00, 0x00, 0x09, 0x1F, 0xE0])
        self.assertEqual(g.enigma(data), [0x10, 0x11, 0x12, 0, 0])


class Nemesis(unittest.TestCase):
    def test_nemesis_one_code(self):
        # one tile; code "0" (1 bit) = colour 5 eight times; eight of them
        self.assertEqual(g.nemesis(bytes([0x00, 0x01, 0x85, 0x71, 0x00, 0xFF, 0x00])), [[[5] * 8] * 8])

    def test_nemesis_xor_mode(self):
        # the same stream in XOR mode: each row is XORed with the one above
        tile = g.nemesis(bytes([0x80, 0x01, 0x85, 0x71, 0x00, 0xFF, 0x00]))[0]
        self.assertEqual(tile, [[5] * 8 if y % 2 == 0 else [0] * 8 for y in range(8)])


class Output(unittest.TestCase):
    def test_pack_header_and_byte_order(self):
        self.assertEqual(g.pack(2, 1, [0x1234, 0xABCD]),
                         b"BKD1" + bytes([2, 0, 1, 0, 0x12, 0x34, 0xAB, 0xCD]))

    def test_md_colour(self):
        self.assertEqual(g.md_to_rgb565(0x0EEE), 0xFFFF)

    def test_window_is_the_screen(self):
        x0, y0, x1, y1 = g.WINDOW
        self.assertEqual((x1 - x0, y1 - y0), (320, 240))


if __name__ == "__main__":
    unittest.main()
