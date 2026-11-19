import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dlog_join  # noqa: E402

HEADER = "t_utc,ms,kmh,rpm,coolant_c,iat_c,throttle_pct,pedal_pct,load_pct,volt,nv\n"


class JoinTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.drive = pathlib.Path(self.tmp.name) / "drive"
        self.trip = self.drive / "20261119-225001"
        self.trip.mkdir(parents=True)

    def tearDown(self):
        self.tmp.cleanup()

    def part(self, n, rows, header=HEADER, trip=None):
        (trip or self.trip).joinpath(f"p{n:04d}.csv").write_text(header + "".join(r + "\n" for r in rows))

    def test_parts_join_in_part_order_with_one_header(self):
        self.part(10, ["c"])
        self.part(2, ["b"])
        self.part(1, ["a"])
        (self.trip / "linkup1.txt").write_text("01A4 -> NO DATA\n")
        self.assertEqual(dlog_join.join(self.trip), HEADER + "a\nb\nc\n")

    def test_a_different_header_is_refused(self):
        self.part(1, ["a"])
        self.part(2, ["b"], header="t_utc,ms\n")
        with self.assertRaises(ValueError):
            dlog_join.join(self.trip)

    def test_no_parts_is_an_error(self):
        with self.assertRaises(ValueError):
            dlog_join.join(self.trip)

    def test_latest_picks_the_newest_trip(self):
        self.part(1, ["old"])
        newer = self.drive / "20261120-080000"
        newer.mkdir()
        self.part(1, ["new"], trip=newer)
        self.assertEqual(dlog_join.latest(self.drive), newer)

    def test_main_writes_the_file(self):
        self.part(1, ["a"])
        out = pathlib.Path(self.tmp.name) / "trip.csv"
        self.assertEqual(dlog_join.main([str(self.drive), "--latest", "-o", str(out)]), 0)
        self.assertEqual(out.read_text(), HEADER + "a\n")


if __name__ == "__main__":
    unittest.main()
