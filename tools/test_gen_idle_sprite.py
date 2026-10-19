"""Host tests for tools/gen_idle_sprite.py (no network). Run: python tools/test_gen_idle_sprite.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_idle_sprite as g  # noqa: E402


def tile(fill):
    """One 8x8 4bpp tile, every pixel colour `fill`."""
    return bytes([(fill << 4) | fill]) * 32


class Palette(unittest.TestCase):
    def test_mega_drive_levels_to_rgb565(self):
        # 0000BBB0GGG0RRR0: black, white, pure blue at full, red at level 3 (116)
        self.assertEqual(g.md_to_rgb565(0x0000), 0x0000)
        self.assertEqual(g.md_to_rgb565(0x0EEE), 0xFFFF)
        self.assertEqual(g.md_to_rgb565(0x0E00), 0x001F)
        self.assertEqual(g.md_to_rgb565(0x0006), (116 >> 3) << 11)


class Mappings(unittest.TestCase):
    MAPS = "MS_Stand:\tspriteHeader\n\tspritePiece\t-$10, -$14, 3, 1, 0, 0, 0, 0, 0\t; x\n" \
           "\tspritePiece\t-8, $C, 2, 2, 3, 0, 0, 0, 0\nMS_Stand_End\n"
    DPLC = "SonPLC_Stand:\tdplcHeader\n\tdplcEntry\t3, 0\n\tdplcEntry\t4, $B\nSonPLC_Stand_End\n"

    def test_pieces_and_loads_parse_hex_and_signs(self):
        self.assertEqual(g.pieces(self.MAPS, "Stand"), [(-16, -20, 3, 1, 0), (-8, 12, 2, 2, 3)])
        self.assertEqual(g.loads(self.DPLC, "Stand"), [(3, 0), (4, 11)])

    def test_tiles_go_down_each_column_first(self):
        art = b"".join(tile(i) for i in range(1, 16)) + tile(15)
        pieces = [(0, 0, 2, 2, 0)]                        # 2 x 2 tiles: VRAM 0,1 in column 0
        grid = g.render(art, pieces, [(4, 0)])
        self.assertEqual(grid[g.ORIGIN][g.ORIGIN], 1)              # top-left: VRAM tile 0
        self.assertEqual(grid[g.ORIGIN + 8][g.ORIGIN], 2)          # below it: tile 1
        self.assertEqual(grid[g.ORIGIN][g.ORIGIN + 8], 3)          # right of it: tile 2

    def test_dplc_loads_art_tiles_in_order(self):
        art = b"".join(tile(i) for i in range(1, 16))
        grid = g.render(art, [(0, 0, 1, 1, 1)], [(1, 4), (1, 9)])  # VRAM 1 = art tile 9
        self.assertEqual(grid[g.ORIGIN][g.ORIGIN], 10)


class Pack(unittest.TestCase):
    def test_frames_share_one_box_and_pack_4bpp(self):
        a = [[0] * 8 for _ in range(8)]
        b = [[0] * 8 for _ in range(8)]
        a[2][1] = 5                                       # (x=1, y=2)
        b[4][4] = 7                                       # (x=4, y=4)
        x0, y0, w, h = g.union_box([a, b])
        self.assertEqual((x0, y0, w, h), (1, 2, 4, 3))
        blob = g.pack([a, b], (x0, y0, w, h), [0] * 16)
        self.assertEqual(blob[:4], b"IDL1")
        self.assertEqual(struct.unpack_from("<BBB", blob, 4), (2, 4, 3))
        body = blob[8 + 32:]
        self.assertEqual(len(body), 2 * (4 // 2) * 3)
        self.assertEqual(body[0] >> 4, 5)                 # frame a, (0,0) of the box
        self.assertEqual(body[6 + 2 * 2 + 1] & 15, 7)     # frame b, last pixel (3,2)

    def test_odd_width_pads_to_whole_bytes(self):
        a = [[0] * 8 for _ in range(8)]
        a[0][0] = a[0][2] = 1
        x0, y0, w, h = g.union_box([a])
        self.assertEqual(w, 4)                            # 3 wide, padded to even


if __name__ == "__main__":
    unittest.main()
