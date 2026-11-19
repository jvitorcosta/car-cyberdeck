import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import serial_health  # noqa: E402

# A yellow deck capture (capture_serial.py timestamps) that panics on a short heap, then reboots.
YELLOW = """\
    0.4 [boot] reset reason: power-on
    0.5 [crash] 1: panic after 312s: heap 16012 (min 15120, block 2048), 79 failed allocs (last 67 B), at link
   10.0 [mem] heap 41600 (min 30100, block 22500, fails 0)  stack left: link 2400, voice 1800, deck 2300, music 0, loop 2500
   20.0 [mem] heap 18100 (min 15900, block 4096, fails 3)  stack left: link 2350, voice 1790, deck 2290, music 1500, loop 2480
   20.5 [mem] alloc of 67 B failed (caps 0x1800, malloc) at link
   21.0 Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled.
   21.1 Backtrace: 0x400d1234:0x3ffb1230
   22.0 rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
   22.4 [boot] reset reason: panic
"""

# A black deck plain monitor log (no timestamps), healthy.
BLACK = """\
[boot] reset reason: power-on
[mem] scene-start free=154000 largest=110000 stackfree=3984
[mem] poll free=121000 largest=98000 stackfree=3700
[mem] art free=118500 largest=96000 stackfree=3528
"""

OBD = "[mem] heap 120000 (min 98000)  stack left: obd-link 1800, obd-ask 2100, net 1500, ota 900, loop 2000\n"


class SerialHealthTest(unittest.TestCase):
    def test_yellow_crash_is_found_with_its_heap(self):
        s = serial_health.analyze(YELLOW.splitlines(True))
        self.assertEqual(s["deck"], "yellow")
        self.assertEqual([n for _, n in s["fatal"]], ["panic (Guru Meditation)"])
        self.assertEqual(s["resets"], [(22.0, "0xc SW_CPU_RESET")])
        self.assertEqual([r for _, r in s["boot_reasons"]], ["power-on", "panic"])
        self.assertEqual(s["crash_ring"][0][0], "1")
        self.assertEqual(s["alloc_fails"], [67])
        self.assertEqual(s["heap"]["free_min"], 18100)
        self.assertEqual(s["heap"]["largest_min"], 4096)
        self.assertEqual(s["heap"]["min_ever"], 15900)
        self.assertEqual(s["stacks"]["voice"], 1790)
        self.assertEqual(s["stacks"]["music"], 0)

    def test_black_healthy_log_has_no_problems(self):
        s = serial_health.analyze(BLACK.splitlines(True))
        self.assertEqual(s["deck"], "black")
        self.assertEqual(serial_health.problems(s), [])
        self.assertEqual(s["heap"]["largest_min"], 96000)
        self.assertEqual(s["stacks"], {"scene-start": 3984, "poll": 3700, "art": 3528})
        self.assertIn("verdict: no crash", serial_health.report(s))

    def test_black_allocfail_counts_are_summed(self):
        s = serial_health.analyze(["[mem] poll free=30000 largest=9700 stackfree=3700 allocfail=12 (last 16717 B caps=0x1800)\n",
                                   "mbedtls_ssl_handshake returned -32512\n"])
        self.assertEqual(s["heap"]["allocfail_counted"], 12)
        self.assertEqual(s["tls_errors"], 1)
        self.assertTrue(serial_health.problems(s))

    def test_obd_line_has_no_block(self):
        s = serial_health.analyze([OBD])
        self.assertEqual(s["deck"], "obd")
        self.assertIsNone(s["heap"]["largest_min"])
        self.assertEqual(s["stacks"]["ota"], 900)

    def test_brownout_and_task_watchdog(self):
        s = serial_health.analyze(["Brownout detector was triggered\n",
                                   "E (5000) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:\n"])
        self.assertEqual([n for _, n in s["fatal"]], ["brownout", "task watchdog"])

    def test_strict_exit_code(self):
        with tempfile.TemporaryDirectory() as d:
            bad, good = os.path.join(d, "bad.log"), os.path.join(d, "good.log")
            with open(bad, "w", encoding="utf-8") as f:
                f.write(YELLOW)
            with open(good, "w", encoding="utf-8") as f:
                f.write(BLACK)
            stdout = sys.stdout
            sys.stdout = open(os.devnull, "w")
            try:
                self.assertEqual(serial_health.main([bad, "--strict"]), 1)
                self.assertEqual(serial_health.main([good, "--strict"]), 0)
                self.assertEqual(serial_health.main([bad]), 0)
            finally:
                sys.stdout.close()
                sys.stdout = stdout


if __name__ == "__main__":
    unittest.main()
