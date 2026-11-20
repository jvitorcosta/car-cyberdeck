"""Host tests for tools/media_sizes.py. Run: python tools/test_media_sizes.py"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import media_sizes as ms  # noqa: E402


class RideArtTest(unittest.TestCase):
    def write(self, data):
        fd, path = tempfile.mkstemp(suffix=".bin")
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        self.addCleanup(os.remove, path)
        return path

    def test_real_art_passes(self):
        self.assertEqual(ms.ride_art_problem(self.write(b"SKR2" + bytes(64))), "")

    def test_zero_filled_stand_in_is_refused(self):
        # the stand-in placeholders() writes, and the copy that blanked Ride on 2026-11-20
        self.assertIn("stand-in", ms.ride_art_problem(self.write(bytes(738414))))

    def test_missing_file_is_refused(self):
        self.assertIn("missing", ms.ride_art_problem(os.path.join(tempfile.gettempdir(), "no-such-ride.bin")))


if __name__ == "__main__":
    unittest.main()
