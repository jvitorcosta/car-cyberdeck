import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import version  # noqa: E402  (outside PlatformIO it only defines functions)


class FromDescribe(unittest.TestCase):
    def test_describe_shapes_from_the_docstring(self):
        # (git describe, short sha) -> (full, short): the table in tools/version.py's header
        rows = [
            (("v1.2.0", "gabc1234"), ("1.2.0", "1.2.0")),
            (("v1.2.0-3-gabc1234", "gabc1234"), ("1.2.0+3.gabc1234", "1.2.0+")),
            (("v1.2.0-dirty", "gabc1234"), ("1.2.0+dirty", "1.2.0+")),
            (("v1.2.0-3-gabc1234-dirty", "gabc1234"), ("1.2.0+3.gabc1234.dirty", "1.2.0+")),
            (("v10.20.30", None), ("10.20.30", "10.20.30")),
        ]
        for args, want in rows:
            with self.subTest(args=args):
                self.assertEqual(version.from_describe(*args), want)

    def test_no_tag_falls_back_to_dev(self):
        self.assertEqual(version.from_describe("", "gabc1234"), ("0.0.0-dev+gabc1234", "0.0.0-dev"))
        self.assertEqual(version.from_describe(None, None), ("0.0.0-dev", "0.0.0-dev"))
        self.assertEqual(version.from_describe("abc1234", None), ("0.0.0-dev", "0.0.0-dev"))

    def test_names_that_are_not_release_tags_are_dev(self):
        for desc in ["2.1.0", "v2.1", "v2.1.x", "release-v2.1.0", "v2.1.0 "]:
            with self.subTest(desc=desc):
                self.assertEqual(version.from_describe(desc, None)[1], "0.0.0-dev")

    @unittest.expectedFailure   # F-22: a pre-release tag reads as 0.0.0-dev, below the release it leads to.
    def test_prerelease_tag_keeps_its_number(self):   # Drop the decorator once version.py handles it
        full, short = version.from_describe("v2.1.0-rc1-3-gabc1234", "gabc1234")
        self.assertTrue(full.startswith("2.1.0"), full)
        self.assertTrue(short.startswith("2.1.0"), short)


if __name__ == "__main__":
    unittest.main()
