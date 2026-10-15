"""Host tests for tools/gen_avatar.py (no ffmpeg needed). Run: python tools/test_gen_avatar.py"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_avatar as ga  # noqa: E402


class Pack(unittest.TestCase):
    def test_header_and_size(self):
        f = bytes(ga.FRAME)
        blob = ga.pack("ZERO TWO", [(0, [f]), (1, [f, f])])
        self.assertEqual(blob[:4], b"AVT1")
        self.assertEqual(blob[4:6], bytes([2, 0]))
        self.assertEqual(blob[6:18], b"ZERO TWO\0\0\0\0")
        self.assertEqual(blob[18:22], bytes([0, 1, 1, 2]))
        self.assertEqual(len(blob), 22 + 3 * ga.FRAME)

    def test_rgb565_high_byte_first(self):
        self.assertEqual(ga.rgb565be(bytes([255, 0, 0])), b"\xf8\x00")
        self.assertEqual(ga.rgb565be(bytes([0, 0, 255])), b"\x00\x1f")
        self.assertEqual(ga.rgb565be(bytes([255, 255, 255, 0, 0, 0])), b"\xff\xff\x00\x00")


class Validate(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        for f in ("a.png", "big.png"):
            open(os.path.join(self.dir, f), "wb").close()

    def size_of(self, path):
        return (92, 92) if path.endswith("a.png") else (400, 300)

    def check(self, manifest):
        return ga.validate(manifest, self.dir, self.size_of)

    def bad(self, manifest, words):
        with self.assertRaises(ga.ManifestError) as cm:
            self.check(manifest)
        self.assertIn(words, str(cm.exception))

    def test_good(self):
        name, moods = self.check({"name": "Z", "moods": {
            "smug": {"file": "a.png"},
            "talk": {"frames": ["a.png", "big.png"], "crop": [300, 0, 0]}}})
        self.assertEqual(name, "Z")
        self.assertEqual([m for m, _ in moods], [0, 1])
        self.assertEqual(len(moods[1][1]), 2)

    def test_errors(self):
        self.bad({"name": "", "moods": {"smug": {"file": "a.png"}}}, "name")
        self.bad({"name": "THIRTEEN CHAR", "moods": {"smug": {"file": "a.png"}}}, "name")
        self.bad({"name": "Z", "moods": {}}, "no moods")
        self.bad({"name": "Z", "moods": {"angry": {"file": "a.png"}}}, "unknown mood")
        self.bad({"name": "Z", "moods": {"smug": {}}}, "exactly one")
        self.bad({"name": "Z", "moods": {"smug": {"file": "a.png", "frames": ["a.png"]}}}, "exactly one")
        self.bad({"name": "Z", "moods": {"talk": {"frames": []}}}, "frames")
        self.bad({"name": "Z", "moods": {"talk": {"frames": ["a.png"] * 9}}}, "frames")
        self.bad({"name": "Z", "moods": {"smug": {"file": "nope.png"}}}, "missing file")
        self.bad({"name": "Z", "moods": {"smug": {"file": "big.png"}}}, "needs \"crop\"")
        self.bad({"name": "Z", "moods": {"smug": {"file": "big.png", "crop": [300, 200, 0]}}}, "outside")


if __name__ == "__main__":
    unittest.main()
