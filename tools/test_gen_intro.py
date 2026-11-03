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
        for bad in ("6", "9-3", "3-3", "a-b", "-1-4"):
            with self.assertRaises(ValueError):
                gi.segment(bad)


class Sound(unittest.TestCase):
    # each segment's sound is cut to its frames' length, so the seams stay in sync with the picture
    def test_fit_trims(self):
        pcm = bytes(range(200))                          # 100 samples
        self.assertEqual(gi.fit(pcm, frames=3, fps=10, rate=200), pcm[:120])   # 0.3 s = 60 samples

    def test_fit_pads_with_silence(self):
        self.assertEqual(gi.fit(b"", frames=1, fps=10, rate=40), b"" + bytes(6))


    def test_level_matches_the_greetings(self):
        # the kiss video's sound sat at 0.265 RMS (peaks near clipping), Max's at 0.068: both
        # now land at the boot rosters' loudness, so no clip jumps out after another
        loud = struct.pack("<%dh" % 4000, *([20000, -20000] * 2000))
        x = struct.unpack("<%dh" % 4000, gi.level(loud))
        rms = (sum(v * v for v in x) / len(x)) ** 0.5 / 32767
        self.assertAlmostEqual(rms, gi.gr.LOUDNESS, places=3)
        self.assertEqual(len(gi.level(loud)), len(loud))

    def test_level_keeps_silence(self):
        self.assertEqual(gi.level(bytes(8)), bytes(8))


if __name__ == "__main__":
    unittest.main()
