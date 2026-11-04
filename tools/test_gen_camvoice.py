"""Host tests for tools/gen_camvoice.py. Run: python tools/test_gen_camvoice.py"""
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_camvoice as gc  # noqa: E402

TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_camvoice.py")
HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "util", "car", "camclip.h")


def camclip_h():
    """camclip.h's Slot names (in order) and its layout constants."""
    text = open(HEADER, encoding="utf-8").read()
    body = re.search(r"enum class Slot : uint8_t \{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"//[^\n]*", "", body)
    names = [n.strip() for n in body.split(",") if n.strip() and n.strip() != "COUNT"]
    consts = {k: int(v) for k, v in re.findall(r"\b(RATE|HEAD|ENTRY|VERSION) = (\d+)", text)}
    return names, consts


def snake(name):
    """camclip's PascalCase slot -> gen_camvoice's name (GpsFirst -> GPS_FIRST)."""
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).upper()


def load(blob):
    """A mirror of camclip::Table::load: [(offset, samples)] per slot, None when not a table."""
    count = len(gc.SLOTS)
    if len(blob) < gc.HEAD or blob[:4] != b"CAMV" or struct.unpack_from("<H", blob, 4)[0] != gc.VERSION:
        return None
    n = struct.unpack_from("<H", blob, 6)[0]
    table = gc.HEAD + gc.ENTRY * n
    if len(blob) < table:
        return None
    out = [(0, 0)] * count
    for i in range(min(n, count)):
        off, size = struct.unpack_from("<II", blob, gc.HEAD + gc.ENTRY * i)
        if not size or size % 2 or off < table or off > len(blob) or size > len(blob) - off:
            continue
        out[i] = (off, size // 2)
    return out


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


class CamclipTest(unittest.TestCase):
    """The file's layout, locked to src/util/car/camclip.h (what the deck reads)."""

    def test_slots_and_constants_match_the_header(self):
        names, consts = camclip_h()
        self.assertEqual([snake(n) for n in names], [s for s, _ in gc.SLOTS])
        self.assertEqual(consts, {"RATE": gc.RATE, "HEAD": gc.HEAD, "ENTRY": gc.ENTRY,
                                  "VERSION": gc.VERSION})

    def test_round_trip_through_the_readers_rules(self):
        clips = [b""] * len(gc.SLOTS)
        clips[0] = b"\x01\x00\x02\x00\x03\x00"
        clips[5] = b"\xff\x7f"
        clips[-1] = b"\x00\x80\x01\x00"
        blob = gc.build(clips)
        self.assertEqual(blob[:8], b"CAMV" + struct.pack("<HH", 1, len(gc.SLOTS)))
        read = load(blob)
        self.assertIsNotNone(read)
        for i, pcm in enumerate(clips):
            off, samples = read[i]
            self.assertEqual(samples, len(pcm) // 2, gc.SLOTS[i][0])
            self.assertEqual(blob[off:off + 2 * samples], pcm)
            if not pcm:   # an empty slot is 0/0 in the table
                self.assertEqual(struct.unpack_from("<II", blob, gc.HEAD + gc.ENTRY * i), (0, 0))

    def test_reader_refuses_a_bad_header(self):
        blob = gc.build([b"\x01\x00"] * len(gc.SLOTS))
        self.assertIsNone(load(b"X" + blob[1:]))
        self.assertIsNone(load(blob[:gc.HEAD + gc.ENTRY]))


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
