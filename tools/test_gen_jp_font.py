"""Host tests for tools/gen_jp_font.py (no network). Run: python -I tools/test_gen_jp_font.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_jp_font as gj  # noqa: E402


class Subset(unittest.TestCase):
    def test_level1_kanji(self):
        k = gj.jis_level1()
        self.assertEqual(2965, len(k))
        self.assertEqual(len(k), len(set(k)))
        self.assertEqual(0x4E9C, k[0])                      # 亜, the first (row 16)
        self.assertEqual(0x8155, k[-1])                     # 腕, the last (row 47)
        for ch in "米津玄師愛夢": self.assertIn(ord(ch), k)
        self.assertNotIn(ord("们"), k)                       # Chinese only
        self.assertNotIn(ord("鬱"), k)                       # level 2

    def test_wanted(self):
        w = gj.wanted()
        self.assertEqual(w, sorted(set(w)))
        for ch in "々〆ヶあゝゞアヴヶㇰ。「」・ｱﾟＡ！～": self.assertIn(ord(ch), w)
        self.assertNotIn(0x3040, w)                          # unassigned
        self.assertNotIn(0x3097, w)
        self.assertNotIn(ord("한"), w)


class Pack(unittest.TestCase):
    def test_layout(self):
        wide, half = bytes(range(32)), bytes(range(100, 116))
        blob = gj.pack([(0x3042, 16, wide), (0xFF71, 8, half)])
        self.assertEqual(b"JFNT", blob[:4])
        self.assertEqual(2, struct.unpack_from("<H", blob, 4)[0])
        self.assertEqual((0x3042, 0xFF71), struct.unpack_from("<HH", blob, 6))
        self.assertEqual(bytes([16, 8]), blob[10:12])
        self.assertEqual(wide, blob[12:44])
        self.assertEqual(half + bytes(16), blob[44:76])
        self.assertEqual(76, len(blob))

    def test_rejects_bad_entries(self):
        g = bytes(32)
        with self.assertRaises(ValueError):
            gj.pack([(0x3043, 16, g), (0x3042, 16, g)])      # not ascending
        with self.assertRaises(ValueError):
            gj.pack([(0x20000, 16, g)])                      # outside the BMP
        with self.assertRaises(ValueError):
            gj.pack([(0x3042, 8, g)])                        # 32 B for an 8 px glyph


class ReadUfnt(unittest.TestCase):
    def test_reads_both_widths(self):
        hdr = 6 + 13 * 2
        blob = b"UFNT" + struct.pack("<H", 2)
        blob += struct.pack("<IIBI", 0x3042, 0x3043, 16, hdr)
        blob += struct.pack("<IIBI", 0xFF71, 0xFF71, 8, hdr + 64)
        blob += bytes(range(64)) + bytes(range(16))
        g = gj.read_ufnt(blob)
        self.assertEqual((16, bytes(range(32, 64))), g[0x3043])
        self.assertEqual((8, bytes(range(16))), g[0xFF71])


if __name__ == "__main__":
    unittest.main()
