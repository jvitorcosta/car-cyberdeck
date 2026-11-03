"""Host tests for tools/gen_rosters.py. Run: python tools/test_gen_rosters.py"""
import os
import struct
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_rosters as gr  # noqa: E402


def entries(blob):
    magic, version, n = struct.unpack_from("<4sHH", blob, 0)
    out = []
    for i in range(n):
        off, size, name = struct.unpack_from("<II%ds" % gr.NAME, blob, gr.HEAD + gr.ENTRY * i)
        out.append((off, size, name))
    return magic, version, out


class ParseTest(unittest.TestCase):
    def test_parse(self):
        text = "# picks\n[drive]\na.wav\n\n  b_16k.wav  # the clean cut\n[opener]\nlisten/c.wav\n[finale]\n"
        self.assertEqual(gr.parse(text), {"drive": ["a.wav", "b_16k.wav"], "opener": ["listen/c.wav"],
                                          "finale": []})
        with self.assertRaises(SystemExit):
            gr.parse("[intro]\na.wav\n")
        with self.assertRaises(SystemExit):
            gr.parse("a.wav\n")                    # a clip before any section


class BuildTest(unittest.TestCase):
    def test_build_layout(self):
        n = int(gr.RATE * gr.MIN_SECONDS)
        blob = gr.build([("bem", [0.5] * n), ("okaeri", [-0.25] * (n + 1))])
        magic, version, es = entries(blob)
        self.assertEqual((magic, version, len(es)), (b"SNDR", 1, 2))
        for (off, size, name), samples in zip(es, (n, n + 1)):
            self.assertEqual(off % 2, 0)
            self.assertEqual(size, 2 * samples)
            self.assertLessEqual(off + size, len(blob))
        self.assertEqual(es[0][2], b"bem".ljust(gr.NAME, b"\0"))
        self.assertEqual(struct.unpack_from("<h", blob, es[1][0])[0], round(-0.25 * 32767))

    def test_long_name_truncated(self):
        # the owner's longest: unique once cut (ami_okaerinasai_02 is another clip)
        self.assertEqual(gr.clip_name("ami_okaerinasaimattetayo_01_16k.wav"), "ami_okaerinasaimattetay")
        self.assertEqual(gr.clip_name("listen/now_bemvindo.wav"), "now_bemvindo")
        _, _, es = entries(gr.build([("x" * 30, [0.0] * gr.RATE)]))
        self.assertEqual(es[0][2], b"x" * 23 + b"\0")

    def test_too_long_clip(self):
        with self.assertRaises(SystemExit):
            gr.build([("long", [0.0] * int(gr.RATE * 4.01))])
        gr.build([("ok", [0.0] * (gr.RATE * 4))])

    def test_duplicate_names(self):
        # the log and SOUND_FORCE name clips: two of one name can't be told apart
        with self.assertRaises(SystemExit) as cm:
            gr.build([("ikuyo", [0.1] * 8000), ("ikuyo", [0.1] * 8000)])
        self.assertIn("ikuyo", str(cm.exception.code))

    def test_too_short_clip(self):
        # under 0.2 s is a click, not a greeting (an empty or mis-cut WAV)
        with self.assertRaises(SystemExit):
            gr.build([("blip", [0.1] * int(gr.RATE * 0.19))])
        gr.build([("ok", [0.1] * int(gr.RATE * 0.2))])

    def test_missing_wav(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(SystemExit) as cm:
                gr.load(d, ["nope.wav"])
            self.assertIn("nope.wav", str(cm.exception.code))

    def test_load_converts(self):
        with tempfile.TemporaryDirectory() as d:
            with wave.open(os.path.join(d, "a_16k.wav"), "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(32000)
                w.writeframes(struct.pack("<%dh" % 8000, *([1000] * 8 + [-1000] * 8) * 500))
            [(name, x)] = gr.load(d, ["a_16k.wav"])
            self.assertEqual(name, "a")
            self.assertAlmostEqual(gr.rms(x[:-gr.FADE]), gr.LOUDNESS, places=3)   # levelled


class LevelTest(unittest.TestCase):
    def test_dense_clip_levelled_by_loudness(self):
        # Bem-vindo, moreno: speech over a music bed, loud on average. Pushing its peak to 95 %
        # made it ~2.5x louder than today's clips and the speaker rasped.
        x = gr.level([0.4, -0.4] * 4000)
        self.assertAlmostEqual(gr.rms(x), gr.LOUDNESS, places=3)

    def test_peaky_clip_capped_at_full_scale(self):
        x = gr.level([0.9] + [0.001] * 7999)
        self.assertAlmostEqual(max(abs(v) for v in x), gr.gg.PEAK, places=3)

    def test_quiet_tail_kept(self):
        # a word's natural decay at -40 dB stays; the 2 % trim (-34 dB) cut it short
        x = gr.trim([0.0] * 100 + [0.5] * 1000 + [0.005] * 400 + [0.0] * 100)
        self.assertEqual(len(x), 1400)

    def test_fade_out(self):
        x = gr.fade([0.5] * 2000)
        self.assertEqual(x[-1], 0.0)
        self.assertEqual(x[-gr.FADE - 1], 0.5)
        self.assertLess(x[-gr.FADE // 2], 0.5)


if __name__ == "__main__":
    unittest.main()
