"""Host tests for tools/gen_howl.py (no ffmpeg needed). Run: python tools/test_gen_howl.py"""
import array
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_howl as gh  # noqa: E402


def pcm(*samples):
    return array.array("h", samples).tobytes()


class Normalize(unittest.TestCase):
    def test_loudest_sample_reaches_the_peak(self):
        out = array.array("h", gh.normalize(pcm(1000, -2000, 500), 0.5))
        self.assertEqual(max(abs(s) for s in out), int(32767 * 0.5))
        self.assertEqual(out[0], int(32767 * 0.5) // 2)          # proportions kept

    def test_silence_stays_silent(self):
        self.assertEqual(gh.normalize(pcm(0, 0), 0.95), pcm(0, 0))

    def test_never_clips(self):
        out = array.array("h", gh.normalize(pcm(-32768, 100), 1.0))
        self.assertTrue(all(-32768 <= s <= 32767 for s in out))


class Defaults(unittest.TestCase):
    def test_rate_and_cut(self):
        # 48 kHz like the intro (the S3's ES8311 has the clock row); a steady howl of the
        # Commons recording, past its yips
        self.assertEqual(gh.RATE, 48000)
        self.assertEqual((gh.START, gh.END), (13.0, 19.5))


if __name__ == "__main__":
    unittest.main()
