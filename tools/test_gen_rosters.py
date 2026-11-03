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
        off, size, name = struct.unpack_from("<II16s", blob, gr.HEAD + gr.ENTRY * i)
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
        blob = gr.build([("bem", [0.5] * 3), ("okaeri", [-0.25] * 4)])
        magic, version, es = entries(blob)
        self.assertEqual((magic, version, len(es)), (b"SNDR", 1, 2))
        for (off, size, name), samples in zip(es, (3, 4)):
            self.assertEqual(off % 2, 0)
            self.assertEqual(size, 2 * samples)
            self.assertLessEqual(off + size, len(blob))
        self.assertEqual(es[0][2], b"bem".ljust(16, b"\0"))
        self.assertEqual(struct.unpack_from("<h", blob, es[1][0])[0], round(-0.25 * 32767))

    def test_long_name_truncated(self):
        self.assertEqual(gr.clip_name("okaerinasaimattetayo_01_16k.wav"), "okaerinasaimatt")
        self.assertEqual(gr.clip_name("listen/now_bemvindo.wav"), "now_bemvindo")
        _, _, es = entries(gr.build([("x" * 20, [0.0])]))
        self.assertEqual(es[0][2], b"x" * 15 + b"\0")

    def test_too_long_clip(self):
        with self.assertRaises(SystemExit):
            gr.build([("long", [0.0] * int(gr.RATE * 4.01))])
        gr.build([("ok", [0.0] * (gr.RATE * 4))])

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
                w.writeframes(struct.pack("<%dh" % 800, *([1000, 1000, -1000, -1000] * 200)))
            [(name, x)] = gr.load(d, ["a_16k.wav"])
            self.assertEqual(name, "a")
            self.assertAlmostEqual(max(abs(v) for v in x), 0.95, places=2)   # normalised


if __name__ == "__main__":
    unittest.main()
