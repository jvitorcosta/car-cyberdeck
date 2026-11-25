import hashlib
import os
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SIZES = (17, 19, 24, 36)


def body(path, ns):
    """A generated VT323 table without what is meant to differ between the decks: the comment
    header (it carries the gfxfont command line), the #include of the wrapper and its namespace."""
    out = []
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        lines = f.read().splitlines()
    for line in lines:
        if line.startswith("//") or line.startswith("#include"):
            continue
        out.append(line.replace(ns + "::", "NS::"))
    return "\n".join(out)


class FontTwins(unittest.TestCase):
    """src/black/fonts and src/yellow/fonts are generated copies (tools/gfxfont.py, same TTF, same
    sizes), told apart only by namespace. A hand edit or a regeneration of one deck must hit both."""

    def test_the_four_sizes_exist_on_both_decks(self):
        for n in SIZES:
            for deck in ("black", "yellow"):
                self.assertTrue(os.path.isfile(os.path.join(ROOT, "src", deck, "fonts", "vt323_%d.h" % n)))

    def test_black_and_yellow_tables_hash_the_same(self):
        for n in SIZES:
            with self.subTest(size=n):
                black = body("src/black/fonts/vt323_%d.h" % n, "lyricface")
                yellow = body("src/yellow/fonts/vt323_%d.h" % n, "termfont")
                self.assertGreater(len(black), 1000)   # a stub or an empty table is not a pass
                self.assertEqual(hashlib.sha256(black.encode()).hexdigest(),
                                 hashlib.sha256(yellow.encode()).hexdigest(),
                                 "vt323_%d differs between the decks: regenerate both with tools/gfxfont.py" % n)

    def test_each_table_is_from_the_ttf_at_its_own_size(self):
        for deck, ns in (("black", "lyricface"), ("yellow", "termfont")):
            for n in SIZES:
                with open(os.path.join(ROOT, "src", deck, "fonts", "vt323_%d.h" % n), encoding="utf-8") as f:
                    text = f.read()
                with self.subTest(deck=deck, size=n):
                    self.assertRegex(text, r"gfxfont\.py tools/fonts/VT323-Regular\.ttf %d " % n)
                    self.assertIn("%s::Font VT323_%d = {" % (ns, n), text)


if __name__ == "__main__":
    unittest.main()
