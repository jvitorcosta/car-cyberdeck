"""Host tests for tools/gen_montage.py (no ffmpeg needed). Run: python tools/test_gen_montage.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_montage as gm  # noqa: E402


class Length(unittest.TestCase):
    def test_photos_take_about_five_seconds(self):
        s = gm.photo_frames() / gm.FPS
        self.assertGreaterEqual(s, 4.5)
        self.assertLessEqual(s, 5.5)

    def test_end_card_about_two_and_a_half_seconds(self):
        s = (gm.END_IN + gm.END_HOLD + gm.END_OUT) / gm.FPS
        self.assertGreaterEqual(s, 2.0)
        self.assertLessEqual(s, 3.0)


class EndCard(unittest.TestCase):
    def test_caption_in_red_under_a_raised_logo(self):
        f = gm.end_filter(100, 40, 0, 0, text="POWERED BY HONDA")
        self.assertIn("drawtext", f)
        self.assertIn("POWERED BY HONDA", f)
        self.assertEqual(f.count(gm.END_RED), 2)   # the frame and the text
        self.assertIn("(H-h)/2-16", f)

    # The owner's logo art already reads "POWERED by HONDA": by default no caption repeats it.
    def test_no_caption_by_default(self):
        f = gm.end_filter(100, 40, 0, 0)
        self.assertNotIn("drawtext", f)
        self.assertIn("(H-h)/2-6", f)


if __name__ == "__main__":
    unittest.main()
