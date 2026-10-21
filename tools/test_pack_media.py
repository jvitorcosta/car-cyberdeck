"""Host tests for tools/pack_media.py. Run: python tools/test_pack_media.py"""
import os
import struct
import sys
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pack_media as pm  # noqa: E402


def entries(blob):
    magic, version, n, size = struct.unpack_from("<8sHHI", blob, 0)
    out = []
    for i in range(n):
        name, off, length, crc = struct.unpack_from("<20sIII", blob, pm.HEAD + pm.ENTRY * i)
        out.append((name.rstrip(b"\0").decode(), off, length, crc))
    return magic, version, size, out


class PackTest(unittest.TestCase):
    def test_layout_matches_mediapack(self):
        blob = pm.pack([("a.pcm", b"hello"), ("b.bin", b"\x01\x02\x03")])
        magic, version, size, files = entries(blob)
        self.assertEqual((magic, version, size), (b"CDMEDIA1", 1, len(blob)))
        self.assertEqual([f[0] for f in files], ["a.pcm", "b.bin"])
        for name, off, length, crc in files:
            self.assertEqual(off % 4, 0)
            self.assertEqual(zlib.crc32(blob[off:off + length]), crc)
        self.assertEqual(blob[files[0][1]:files[0][1] + 5], b"hello")

    def test_empty_pack_is_just_the_header(self):
        self.assertEqual(len(pm.pack([])), pm.HEAD)

    def test_long_names_are_refused(self):
        with self.assertRaises(SystemExit):
            pm.pack([("x" * pm.NAME + ".bin", b"")])

    def test_media_partition_is_read_from_the_table(self):
        self.assertEqual(pm.partition(), (0x610000, 0x9E0000))


if __name__ == "__main__":
    unittest.main()
