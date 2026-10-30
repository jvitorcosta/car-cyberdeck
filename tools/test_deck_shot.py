import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from deck_shot import HEIGHT, WIDTH, missing_rows, parse_rows, rgb888  # noqa: E402


def row(y, pixel="ffff"):
    return "R%03d %s\n" % (y, pixel * WIDTH)


def test_rows_parse_and_log_lines_between_them_are_skipped():
    rows = parse_rows([row(0), "[mem] heap 90000 (min 86000)\n", row(1, "0000")])
    assert sorted(rows) == [0, 1]
    assert rows[0] == b"\xff\xff" * WIDTH


def test_a_cut_row_counts_as_missing():
    rows = parse_rows([row(0), row(1)[:100] + "\n", "R999 " + "ffff" * WIDTH + "\n"])
    assert sorted(rows) == [0]
    assert missing_rows(rows)[0] == 1
    assert len(missing_rows(rows)) == HEIGHT - 1


def test_rgb565_expands_to_full_range():
    rows = parse_rows([row(y, "f800") for y in range(HEIGHT)])  # pure red
    assert missing_rows(rows) == []
    picture = rgb888(rows)
    assert len(picture) == WIDTH * HEIGHT * 3
    assert picture[:3] == b"\xff\x00\x00"
    rows = parse_rows([row(y, "07e0") for y in range(HEIGHT)])  # pure green
    assert rgb888(rows)[:3] == b"\x00\xff\x00"
