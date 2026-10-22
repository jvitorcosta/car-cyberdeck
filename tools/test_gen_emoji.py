"""Host tests for tools/gen_emoji.py (no network, no Pillow). Run: python tools/test_gen_emoji.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_emoji as ge  # noqa: E402

BR = 0x110000 + 1 * 26 + 17      # the same key as test_glyphrun's flag


class KeyOf(unittest.TestCase):
    def test_single_and_selector(self):
        self.assertEqual(0x1F525, ge.key_of([0x1F525]))
        self.assertEqual(0x2764, ge.key_of([0x2764, 0xFE0F]))

    def test_skin_tone_and_zwj_fall_back_to_the_base(self):
        self.assertEqual(0x1F44D, ge.key_of([0x1F44D, 0x1F3FD]))
        self.assertEqual(0x1F468, ge.key_of([0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467]))
        self.assertEqual(0x1F3F3, ge.key_of([0x1F3F3, 0xFE0F, 0x200D, 0x1F308]))

    def test_flags(self):
        self.assertEqual(BR, ge.key_of([0x1F1E7, 0x1F1F7]))
        self.assertIsNone(ge.key_of([0x1F1E7]))
        self.assertEqual("1f1e7-1f1f7", ge.twemoji_name(BR))
        self.assertEqual("1f525", ge.twemoji_name(0x1F525))

    def test_text_never_reaches_the_emoji_font(self):
        self.assertIsNone(ge.key_of([0x31, 0xFE0F, 0x20E3]))    # keycap 1: drawn as "1"
        self.assertIsNone(ge.key_of([0xA9, 0xFE0F]))            # (c): Latin-1 path
        self.assertEqual(0x1F3F4, ge.key_of([0x1F3F4, 0xE0067, 0xE0062, 0xE007F]))


class Pack(unittest.TestCase):
    def test_layout(self):
        px = [0] * 256
        px[0], px[1], px[255] = 1, 2, 2
        blob = ge.pack([(0x1F525, [0xF800, 0x07E0], px), (0x2764, [0x001F], [1] * 256)])
        self.assertEqual(b"EMJ2", blob[:4])
        self.assertEqual((2, 0), struct.unpack_from("<HH", blob, 4))
        self.assertEqual((0x2764, 0x1F525), struct.unpack_from("<II", blob, 8))   # sorted
        g1 = 8 + 8 + 160
        self.assertEqual(160 * 2 + 16, len(blob))
        self.assertEqual((0, 0xF800, 0x07E0), struct.unpack_from("<3H", blob, g1))   # 0 = clear
        self.assertEqual(0x12, blob[g1 + 32])                      # high nibble = left pixel
        self.assertEqual(0x02, blob[g1 + 32 + 127])
        self.assertEqual(0x11, blob[8 + 8 + 32])

    def test_sequence_table(self):
        seqs = [(0x1F3F3, 0x1F308), (0x1F408, 0x2B1B)]
        glyphs = [(ge.SEQ_BASE + i, [], [0] * 256) for i in range(2)]
        blob = ge.pack(glyphs, seqs)
        self.assertEqual((2, 2), struct.unpack_from("<HH", blob, 4))
        self.assertEqual((0x1F3F3, 0x1F308, 0, 0, 0x1F408, 0x2B1B, 0, 0),
                         struct.unpack_from("<8I", blob, 8 + 8))
        self.assertEqual(8 + 8 + 32 + 2 * 160, len(blob))
        with self.assertRaises(ValueError):
            ge.pack(glyphs, list(reversed(seqs)))                  # not ascending
        with self.assertRaises(ValueError):
            ge.pack(glyphs, [(1, 2, 3, 4, 5)])                     # too long

    def test_rejects_too_many_colours_and_duplicates(self):
        with self.assertRaises(ValueError):
            ge.pack([(1, list(range(16)), [0] * 256)])
        with self.assertRaises(ValueError):
            ge.pack([(1, [], [0] * 256), (1, [], [0] * 256)])


class Joined(unittest.TestCase):
    def test_parts_drop_zwj_selectors_and_tones(self):
        self.assertEqual((0x1F408, 0x2B1B), ge.joined_parts([0x1F408, 0x200D, 0x2B1B]))
        self.assertEqual((0x1F3F3, 0x1F308), ge.joined_parts([0x1F3F3, 0xFE0F, 0x200D, 0x1F308]))
        self.assertEqual((0x1F469, 0x1F4BB), ge.joined_parts([0x1F469, 0x1F3FD, 0x200D, 0x1F4BB]))

    def test_list_has_the_248_without_tones(self):
        seqs = ge.read_zwj(ge.ZWJ_LIST)
        self.assertEqual(248, len(seqs))
        parts = [ge.joined_parts(s) for s, _ in seqs]
        self.assertEqual(len(parts), len(set(parts)))
        self.assertTrue(all(2 <= len(p) <= ge.SEQ_MAX for p in parts))
        self.assertIn(([0x1F408, 0x200D, 0x2B1B], "1f408-200d-2b1b"), seqs)

    def test_rgb565(self):
        self.assertEqual(0xFFFF, ge.rgb565(255, 255, 255))
        self.assertEqual(0xF800, ge.rgb565(255, 0, 0))


class Tiers(unittest.TestCase):
    def test_list_reads_in_rank_order_without_duplicates(self):
        keys = ge.read_tiers(ge.FREQ, 0)
        self.assertEqual([0x1F602, 0x2764], keys)
        keys = ge.read_tiers(ge.FREQ, 10)
        self.assertEqual(len(keys), len(set(keys)))
        self.assertIn(0x1F525, keys)


if __name__ == "__main__":
    unittest.main()
