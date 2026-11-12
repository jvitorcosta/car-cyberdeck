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


class Clips(unittest.TestCase):
    def setUp(self):
        import tempfile
        self.dir = tempfile.mkdtemp()
        open(os.path.join(self.dir, "02-front-close.jpg"), "wb").close()

    def test_a_still_without_a_clip(self):
        inp, n, rate, moving = gm.source(self.dir, "02-front-close.jpg", 11, 0.005)
        self.assertEqual((inp[-1], n, rate, moving), (os.path.join(self.dir, "02-front-close.jpg"), 11, 0.005, False))

    # The clip replaces the still from its start, for its frames, with the same total push-in.
    def test_a_clip_replaces_its_still(self):
        os.makedirs(os.path.join(self.dir, "clips"))
        open(os.path.join(self.dir, "clips", "02-front-close.mp4"), "wb").close()
        inp, n, rate, moving = gm.source(self.dir, "02-front-close.jpg", 11, 0.005)
        start, frames = gm.CLIPS["02-front-close"][:2]
        self.assertTrue(moving)
        self.assertEqual(inp[:2], ["-ss", "%.2f" % start])
        self.assertEqual(n, frames)
        self.assertAlmostEqual(rate * n, 0.005 * 11)

    def test_a_clip_shot_runs_one_input_frame_per_output_frame(self):
        f = gm.shot_filter(282, 655, 556, 0.87, 20, 0.003, -1, False, clip=True)
        self.assertTrue(f.startswith("fps=%d," % gm.FPS))
        self.assertIn(":d=1:", f)

    def test_moving_shots_stay_short(self):
        s = (sum(c[1] for c in gm.CLIPS.values()) - gm.XFADE * (len(gm.CLIPS) - 1)) / gm.FPS
        self.assertLessEqual(s, 8.5)


class Wink(unittest.TestCase):
    # The clip's open-eye frames, its last one held, the wink frame (-1), then held open again.
    def test_wink_sequence(self):
        self.assertEqual(gm.wink_sequence(10, 3, 2, 2), [0, 1, 2, 2, 2, -1, -1, 2, 2, 2])

    def test_wink_shot_keeps_the_clip_length(self):
        for stem, (opened, hold, wink) in gm.WINK.items():
            n = gm.CLIPS[stem][1]
            seq = gm.wink_sequence(n, opened, hold, wink)
            self.assertEqual(len(seq), n)
            self.assertEqual(seq.count(-1), wink)

    def test_a_wink_frame_makes_the_clip_a_sequence(self):
        import tempfile
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "clips"))
        for f in ("06-headlight-detail.mp4", "06-headlight-detail-wink.png"):
            open(os.path.join(d, "clips", f), "wb").close()
        self.assertEqual(gm.wink_frame(d, "06-headlight-detail"),
                         os.path.join(d, "clips", "06-headlight-detail-wink.png"))
        self.assertIsNone(gm.wink_frame(d, "02-front-close"))


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
