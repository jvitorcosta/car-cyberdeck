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


class Segments(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(gi.segment("6-99"), (6.0, 99.0))
        self.assertEqual(gi.segment("0-6.5"), (0.0, 6.5))

    def test_rejects_bad(self):
        for bad in ("6", "9-3", "a-b", "-1-4"):
            with self.assertRaises(ValueError):
                gi.segment(bad)

    def test_default_order(self):
        # the wallpaper loops: its story starts at 11.8 s; its first seconds end the kiss
        self.assertEqual(gi.DEFAULT_SEGMENTS, [(11.8, 99.0), (0.0, 3.0)])


class Sound(unittest.TestCase):
    # each segment's sound is cut to its frames' length, so the seams stay in sync with the picture
    def test_fit_trims(self):
        pcm = bytes(range(200))                          # 100 samples
        self.assertEqual(gi.fit(pcm, frames=3, fps=10, rate=200), pcm[:120])   # 0.3 s = 60 samples

    def test_fit_pads_with_silence(self):
        self.assertEqual(gi.fit(b"", frames=1, fps=10, rate=40), b"" + bytes(6))

    def test_defaults(self):
        self.assertEqual((gi.FPS, gi.RATE), (20, 48000))


if __name__ == "__main__":
    unittest.main()
