"""Host tests for tools/gen_tune.py. Run: python tools/test_gen_tune.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_tune as gt  # noqa: E402


def varlen(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.insert(0, 0x80 | (n & 0x7F))
        n >>= 7
    return bytes(out)


def track(events):
    """events: (delta, bytes) pairs -> an MTrk chunk with end-of-track."""
    body = b"".join(varlen(d) + e for d, e in events) + b"\x00\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(body)) + body


def midi(tracks, division=480):
    return b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks), division) + b"".join(tracks)


def on(n, ch=0, v=100):
    return bytes([0x90 | ch, n, v])


def off(n, ch=0):
    return bytes([0x80 | ch, n, 0])


TEMPO_120 = b"\xff\x51\x03" + (500000).to_bytes(3, "big")   # 500 ms a beat


class Parse(unittest.TestCase):
    def test_notes_in_ms_with_the_tempo(self):
        data = midi([track([(0, TEMPO_120), (0, on(60)), (480, off(60)), (0, on(64)), (240, off(64))])])
        notes = gt.parse(data)
        self.assertEqual([(n.start, n.end, n.pitch) for n in notes], [(0, 500, 60), (500, 750, 64)])

    def test_running_status_and_velocity_zero_off(self):
        ev = [(0, TEMPO_120), (0, b"\x90\x3c\x64"), (480, b"\x3c\x00"), (0, b"\x3e\x64"), (480, b"\x3e\x00")]
        notes = gt.parse(midi([track(ev)]))
        self.assertEqual([(n.start, n.end, n.pitch) for n in notes], [(0, 500, 60), (500, 1000, 62)])

    def test_drums_are_dropped(self):
        data = midi([track([(0, on(36, ch=9)), (480, off(36, ch=9))])])
        self.assertEqual(gt.parse(data), [])


class Melody(unittest.TestCase):
    def test_skyline_keeps_the_top_note_of_a_chord(self):
        notes = [gt.Note(0, 500, 60, 0), gt.Note(0, 500, 67, 0), gt.Note(5, 500, 64, 1),
                 gt.Note(500, 1000, 62, 0)]
        self.assertEqual(gt.melody(notes), [(67, 500), (62, 500)])

    def test_gaps_become_rests(self):
        notes = [gt.Note(0, 200, 60, 0), gt.Note(500, 700, 62, 0)]
        self.assertEqual(gt.melody(notes), [(60, 200), (gt.REST, 300), (62, 200)])

    def test_one_track_only(self):
        notes = [gt.Note(0, 500, 80, 1), gt.Note(0, 500, 60, 0)]
        self.assertEqual(gt.melody(notes, track=0), [(60, 500)])

    def test_grace_notes_merge_into_the_next(self):
        notes = [gt.Note(0, 20, 59, 0), gt.Note(20, 500, 60, 0)]
        self.assertEqual(gt.melody(notes), [(60, 500)])

    def test_cut_at_max_ms(self):
        notes = [gt.Note(i * 500, i * 500 + 500, 60, 0) for i in range(10)]
        self.assertEqual(sum(ms for _, ms in gt.melody(notes, max_ms=2000)), 2000)


class Output(unittest.TestCase):
    def test_octaves_shift_into_the_music_box_range(self):
        self.assertEqual(gt.fit_octaves([(48, 100), (52, 100)]), [(72, 100), (76, 100)])
        self.assertEqual(gt.fit_octaves([(76, 100), (gt.REST, 50)]), [(76, 100), (gt.REST, 50)])

    def test_text_format(self):
        text = gt.to_text("Park", [(69, 250), (gt.REST, 100), (70, 50)], "source.mid")
        self.assertIn("tune Park\n", text)
        self.assertIn("A4 250 R 100 A#4 50", text)
        self.assertTrue(text.startswith("#"))

    def test_note_names(self):
        self.assertEqual(gt.note_name(60), "C4")
        self.assertEqual(gt.note_name(61), "C#4")
        self.assertEqual(gt.note_name(108), "C8")


if __name__ == "__main__":
    unittest.main()
