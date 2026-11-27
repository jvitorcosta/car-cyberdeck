"""Tests for sd_media: the Pokédeck's SD card gets the real media, and never a stand-in."""
import os
import tempfile
import unittest
from unittest import mock

import sd_media


def put(folder, name, blob):
    with open(os.path.join(folder, name), "wb") as f:
        f.write(blob)


class CopyMedia(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.data = os.path.join(self.tmp.name, "data")
        self.card = os.path.join(self.tmp.name, "card")
        os.makedirs(self.data)
        os.makedirs(self.card)

    def tearDown(self):
        self.tmp.cleanup()

    def on_card(self, name):
        with open(os.path.join(self.card, sd_media.FOLDER, name), "rb") as f:
            return f.read()

    def test_copies_the_media_into_its_folder_and_leaves_the_rest_of_the_card(self):
        put(self.data, "intro.bin", b"MTG1" + bytes(4) + b"frames")
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        put(self.data, "notes.txt", b"not media")
        put(self.card, "photo.jpg", b"mine")
        copied, kept = sd_media.copy(self.data, self.card)
        self.assertEqual(sorted(copied), ["intro.bin", "intro.pcm"])
        self.assertEqual(kept, [])
        self.assertEqual(self.on_card("intro.pcm"), b"\x01\x02" * 100)
        self.assertEqual(sorted(os.listdir(self.card)), ["cardeck", "photo.jpg"])
        self.assertEqual(sorted(os.listdir(os.path.join(self.card, "cardeck"))), ["intro.bin", "intro.pcm"])

    def test_a_second_run_rewrites_only_what_changed(self):
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        put(self.data, "max.pcm", b"\x03\x04" * 100)
        sd_media.copy(self.data, self.card)
        put(self.data, "max.pcm", b"\x05\x06" * 100)   # the same size, other bytes
        copied, kept = sd_media.copy(self.data, self.card)
        self.assertEqual((copied, kept), (["max.pcm"], ["intro.pcm"]))
        self.assertEqual(self.on_card("max.pcm"), b"\x05\x06" * 100)

    def test_a_stand_in_stops_the_whole_copy(self):
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        put(self.data, "max.bin", b"MTG1" + bytes(8000))   # media_sizes.py's zero-filled stand-in
        with self.assertRaises(sd_media.Refused):
            sd_media.copy(self.data, self.card)
        self.assertEqual(os.listdir(self.card), [])

    def test_a_card_that_is_not_there_is_refused(self):
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        with self.assertRaises(sd_media.Refused):
            sd_media.copy(self.data, os.path.join(self.tmp.name, "nowhere"))

    def test_a_failed_write_keeps_the_card_s_good_file(self):
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        sd_media.copy(self.data, self.card)
        put(self.data, "intro.pcm", b"\x07\x08" * 100)
        with mock.patch("os.fsync", side_effect=OSError("card pulled")):
            with self.assertRaises(sd_media.Refused):
                sd_media.copy(self.data, self.card)
        self.assertEqual(self.on_card("intro.pcm"), b"\x01\x02" * 100)
        self.assertEqual(os.listdir(os.path.join(self.card, "cardeck")), ["intro.pcm"])

    def test_names_the_media_data_does_not_have(self):
        put(self.data, "intro.pcm", b"\x01\x02" * 100)
        left_out = sd_media.missing(self.data)
        self.assertIn("max.bin", left_out)
        self.assertNotIn("intro.pcm", left_out)

    def test_a_bare_drive_letter_means_its_root(self):
        self.assertEqual(sd_media.root("E:"), "E:" + os.sep)
        self.assertEqual(sd_media.root(self.card), self.card)

    def test_no_media_at_all_is_refused(self):
        with self.assertRaises(sd_media.Refused):
            sd_media.copy(self.data, self.card)


if __name__ == "__main__":
    unittest.main()
