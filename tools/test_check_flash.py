import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "check_flash.py")


def run(size, *args):
    """check_flash.py on a file of `size` bytes: (exit code, stdout, stderr)."""
    with tempfile.TemporaryDirectory() as d:
        image = os.path.join(d, "fw.bin")
        with open(image, "wb") as f:
            f.write(b"\0" * size)
        p = subprocess.run([sys.executable, "-I", SCRIPT, image] + list(args), capture_output=True, text=True)
        return p.returncode, p.stdout, p.stderr


class CheckFlash(unittest.TestCase):
    def test_under_and_exactly_at_the_default_limit_pass(self):
        self.assertEqual(run(500, "1000")[0], 0)
        self.assertEqual(run(970, "1000")[0], 0)   # 97.0 % is not over 97 %

    def test_one_byte_over_the_default_limit_fails(self):
        code, out, err = run(971, "1000")
        self.assertNotEqual(code, 0)
        self.assertIn("over the limit", err)
        self.assertIn("97.1 %", out)

    def test_a_full_partition_fails(self):
        self.assertNotEqual(run(1000, "1000")[0], 0)
        self.assertNotEqual(run(1001, "1000")[0], 0)

    def test_the_limit_argument_moves_the_line(self):
        self.assertEqual(run(990, "1000", "99")[0], 0)
        self.assertNotEqual(run(991, "1000", "99")[0], 0)
        self.assertNotEqual(run(600, "1000", "50")[0], 0)

    def test_partition_size_takes_hex_like_the_ci_call(self):
        self.assertEqual(run(0x300000 // 2, "0x300000")[0], 0)
        self.assertNotEqual(run(0x300000 - 1, "0x300000")[0], 0)

    def test_missing_arguments_print_usage_and_fail(self):
        p = subprocess.run([sys.executable, "-I", SCRIPT], capture_output=True, text=True)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("check_flash.py", p.stderr)

    def test_a_missing_image_fails_instead_of_passing(self):
        p = subprocess.run([sys.executable, "-I", SCRIPT, os.path.join(tempfile.gettempdir(), "no-such.bin"),
                            "1000"], capture_output=True, text=True)
        self.assertNotEqual(p.returncode, 0)


if __name__ == "__main__":
    unittest.main()
