"""Tests for tools/obd2/decode.py (python -m unittest discover -s tools -p "test_*.py")."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import decode  # noqa: E402

# Mode 01 PIDs the City 2022 listed on 2026-10-19 (old adapter, three ECUs): docs/superpowers/specs/
# 2026-10-28-car-health-check-design.md. 20 / 40 / 60 are the "next block" bits.
CITY = [0x01, 0x03, 0x04, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x11, 0x13, 0x15, 0x1C, 0x1F,
        0x21, 0x23, 0x2E, 0x30, 0x31, 0x33, 0x34, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x49,
        0x4A, 0x4C, 0x52, 0x56, 0x67, 0x68, 0x6C]


def bitmap(block, pids):
    bits = 0
    for p in pids:
        if block < p <= block + 0x20:
            bits |= 1 << (31 - (p - block - 1))
    return f"41 {block:02X} " + " ".join(f"{bits >> s & 0xFF:02X}" for s in (24, 16, 8, 0))


class Mode01(unittest.TestCase):
    def test_rpm_and_speed(self):
        self.assertEqual(decode.mode01("41 0C 1A F8", 0x0C), [(1726.0, "rpm")])
        self.assertEqual(decode.mode01("410D3C", 0x0D), [(60, "km/h")])

    def test_headers_and_two_ecus(self):
        raw = "7E8 03 41 0D 2A\n7E9 03 41 0D 2B"
        self.assertEqual([v for v, _ in decode.mode01(raw, 0x0D)], [42, 43])

    def test_29_bit_headers(self):
        # the City answers on 29-bit IDs (engine 0x10, transmission 0x1E)
        raw = "18DAF110 04 41 0C 0A 3C\n18DAF11E 04 41 0C 0A 3C"
        self.assertEqual([v for v, _ in decode.mode01(raw, 0x0C)], [655.0, 655.0])
        self.assertEqual(decode.describe("18DAF11004410C0A3C"), "RPM: 655 rpm")

    def test_searching_prefix_is_skipped(self):
        self.assertEqual(decode.describe("SEARCHING...\n41 05 7B"), "Coolant temp: 83 C")

    def test_extended_temperature_needs_the_sensor_bit(self):
        self.assertEqual(decode.mode01("41 67 01 7A 00", 0x67)[0][0], 0x7A - 40)
        self.assertIsNone(decode.mode01("41 67 00 7A 00", 0x67)[0][0])

    def test_trims_volts_and_ethanol(self):
        self.assertAlmostEqual(decode.mode01("41 06 80", 0x06)[0][0], 0.0)
        self.assertAlmostEqual(decode.mode01("41 07 70", 0x07)[0][0], -12.5)
        self.assertAlmostEqual(decode.mode01("41 42 37 FF", 0x42)[0][0], 14.335)
        self.assertAlmostEqual(decode.mode01("41 52 4D", 0x52)[0][0], 30.196, places=2)

    def test_cut_off_reply_is_nothing(self):
        self.assertEqual(decode.mode01("41 0C 1A", 0x0C), [])

    def test_other_pid_is_ignored(self):
        self.assertEqual(decode.mode01("41 0D 3C", 0x0C), [])


class Supported(unittest.TestCase):
    def test_city_round_trip(self):
        got = []
        for block in (0x00, 0x20, 0x40, 0x60):
            got += decode.supported(bitmap(block, CITY), block)
        self.assertEqual([p for p in got if p % 0x20], CITY)

    def test_ecus_are_ored(self):
        raw = "7E8 06 41 00 80 00 00 00\n7E9 06 41 00 00 00 00 01"
        self.assertEqual(decode.supported(raw, 0), [0x01, 0x20])

    def test_no_reply(self):
        self.assertIsNone(decode.supported("NO DATA", 0))


class Codes(unittest.TestCase):
    def test_dtc_letters(self):
        self.assertEqual(decode.dtc(0x01, 0x33), "P0133")
        self.assertEqual(decode.dtc(0x43, 0x00), "C0300")
        self.assertEqual(decode.dtc(0x81, 0x00), "B0100")
        self.assertEqual(decode.dtc(0xC1, 0x55), "U0155")

    def test_stored_pending_permanent(self):
        self.assertEqual(decode.codes("43 02 03 01 01 33", "03"), ["P0301", "P0133"])
        self.assertEqual(decode.codes("47 01 04 20", "07"), ["P0420"])
        self.assertEqual(decode.codes("4A 00", "0A"), [])

    def test_no_codes_and_wrong_mode(self):
        self.assertEqual(decode.codes("NO DATA", "03"), [])
        self.assertIsNone(decode.codes("47 01 04 20", "03"))

    def test_two_ecus_same_code_once(self):
        self.assertEqual(decode.codes("43 01 03 01\n43 01 03 01", "03"), ["P0301"])

    def test_multi_frame(self):
        raw = "00A\n0: 43 04 01 33 03 01\n1: 04 20 01 71 00 00 00"
        self.assertEqual(decode.codes(raw, "03"), ["P0133", "P0301", "P0420", "P0171"])


class Describe(unittest.TestCase):
    def test_status_words(self):
        for word in ("NO DATA", "UNABLE TO CONNECT", "CAN ERROR", "?"):
            self.assertEqual(decode.describe(word), word)

    def test_codes_line(self):
        self.assertEqual(decode.describe("43 01 03 01"), "codes: P0301")
        self.assertEqual(decode.describe("NO DATA", "03"), "NO DATA")

    def test_log(self):
        log = ('[obd] 0100 -> "41 00 BE 3E B8 11"\n[boot] noise\n'
               '[obd] 010C -> "SEARCHING...|41 0C 0B B8"\n[obd] ATZ -> "ELM327 v1.5"\n')
        rows = decode.decode_log(log)
        self.assertEqual(len(rows), 3)
        self.assertTrue(rows[0].startswith("0100: supported: 01 03"))
        self.assertEqual(rows[1], "010C: RPM: 750 rpm")
        self.assertEqual(rows[2], "ATZ: ELM327 v1.5")


if __name__ == "__main__":
    unittest.main()
