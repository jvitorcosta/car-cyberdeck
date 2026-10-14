"""Host tests for tools/gen_intro.py (no ffmpeg needed). Run: python tools/test_gen_intro.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_intro as gi  # noqa: E402


class Pack(unittest.TestCase):
    def test_mtg1_layout(self):
        frames = [b"\xff\xd8abc", b"\xff\xd8de"]
        blob = gi.pack(frames, 10)
        self.assertEqual(blob[:4], b"MTG1")
        count, fps = struct.unpack_from("<HH", blob, 4)
        self.assertEqual((count, fps), (2, 10))
        off0, size0, off1, size1 = struct.unpack_from("<IIII", blob, 8)
        self.assertEqual((off0, size0), (8 + 16, 5))
        self.assertEqual((off1, size1), (8 + 16 + 5, 4))
        self.assertEqual(blob[off1:off1 + size1], b"\xff\xd8de")

    def test_empty(self):
        self.assertEqual(gi.pack([], 10), b"MTG1" + struct.pack("<HH", 0, 10))


if __name__ == "__main__":
    unittest.main()
