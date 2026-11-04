"""Host tests for tools/gen_camvoice.py. Run: python tools/test_gen_camvoice.py"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_camvoice as gc  # noqa: E402

TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_camvoice.py")


def write_wav(path, frames, rate=16000, channels=1, width=2):
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(width)
        w.setframerate(rate)
        w.writeframes(frames)


def slots(blob):
    magic, version, n = struct.unpack_from("<4sHH", blob, 0)
    out = [struct.unpack_from("<II", blob, gc.HEAD + gc.ENTRY * i) for i in range(n)]
    return magic, version, out


class BuildTest(unittest.TestCase):
    def test_layout_matches_camclip(self):
        blob = gc.build([b"\x01\x00\x02\x00", b"", b"\x03\x00"])
        magic, version, table = slots(blob)
        self.assertEqual((magic, version), (b"CAMV", 1))
        self.assertEqual(len(table), 3)
        (o0, s0), (o1, s1), (o2, s2) = table
        self.assertEqual((o0, s0), (gc.HEAD + gc.ENTRY * 3, 4))
        self.assertEqual((o1, s1), (0, 0))                      # an empty slot
        self.assertEqual(blob[o0:o0 + s0], b"\x01\x00\x02\x00")
        self.assertEqual(blob[o2:o2 + s2], b"\x03\x00")
        self.assertEqual(len(blob), o2 + s2)

    def test_slot_order_is_camclips(self):
        names = [s for s, _ in gc.SLOTS]
        self.assertEqual(names, ["PON", "GPS_FIRST", "TORISHIMARI", "GOCHUI", "SPD_CHANGE", "SPEEDNI",
                                 "GPS_NG", "GPS_OK", "L20", "L30", "L40", "L50", "L60", "L80"])
        self.assertEqual(dict(gc.SLOTS)["L60"], "cam_speed60ika_01_16k.wav")


class MainTest(unittest.TestCase):
    def run_tool(self, src, out):
        return subprocess.run([sys.executable, TOOL, src, out], capture_output=True, text=True)

    def test_missing_wavs_are_empty_slots(self):
        with tempfile.TemporaryDirectory() as d:
            write_wav(os.path.join(d, "chimevoice_chime_pon_16k.wav"), b"\x10\x00\x20\x00")
            write_wav(os.path.join(d, "cam_speed80ika_01_16k.wav"), b"\x30\x00")
            out = os.path.join(d, "camvoice.bin")
            r = self.run_tool(d, out)
            self.assertEqual(r.returncode, 0, r.stderr)
            blob = open(out, "rb").read()
            _, _, table = slots(blob)
            self.assertEqual(len(table), len(gc.SLOTS))
            self.assertEqual(table[0][1], 4)
            self.assertEqual(table[-1][1], 2)
            self.assertEqual(sum(1 for _, s in table if s), 2)
            o, s = table[-1]
            self.assertEqual(blob[o:o + s], b"\x30\x00")

    def test_refuses_wrong_format(self):
        for kw in ({"rate": 44100}, {"channels": 2}, {"width": 1}):
            with tempfile.TemporaryDirectory() as d:
                write_wav(os.path.join(d, "cam_gps-ng_01_16k.wav"), b"\x00\x00\x00\x00", **kw)
                out = os.path.join(d, "camvoice.bin")
                r = self.run_tool(d, out)
                self.assertNotEqual(r.returncode, 0, kw)
                self.assertIn("16 kHz mono 16-bit", r.stderr)
                self.assertFalse(os.path.exists(out))

    def test_no_such_directory(self):
        with tempfile.TemporaryDirectory() as d:
            r = self.run_tool(os.path.join(d, "nope"), os.path.join(d, "x.bin"))
            self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()
