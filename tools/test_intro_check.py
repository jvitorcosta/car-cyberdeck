import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from intro_check import HEADER, check, constants, effects_off, parse  # noqa: E402

with open(HEADER, encoding="utf-8") as f:
    C = constants(f.read())


def log(light, ms, n, of, what, sent="no link"):
    return f"[intro] {light} +{ms} ms key {n}/{of}: {what}, {sent}\n"


def good_log():
    look = "colour 70,0,128"
    return [
        "[intro] replay\n",
        log("INSIDE", 0, 1, 4, "colour 0,0,0"),
        log("OUTSIDE", 1, 1, 5, "colour 0,0,0"),
        "[mem] heap 90000 (min 86000)\n",
        log("INSIDE", 310, 2, 4, f"fill speed {C['FILL_SPEED']} effect {C['FILL_MODE']}"),
        log("OUTSIDE", 3305, 2, 5, "colour 79,0,145"),
        log("INSIDE", 4302, 3, 4, look),
        log("OUTSIDE", 4310, 3, 5, "colour 113,0,206"),
        log("INSIDE", 4605, 4, 4, look),
        log("OUTSIDE", 5300, 4, 5, look),
        log("OUTSIDE", 6311, 5, 5, look),
    ]


def test_the_header_gives_every_constant():
    assert set(C) == {"FILL_MS", "IN_GAP_MS", "OUT_GAP_MS", "FILL_MODE", "FILL_SPEED"}


def test_a_right_welcome_passes():
    keys, skipped = parse(good_log())
    assert len(keys["INSIDE"]) == 4 and len(keys["OUTSIDE"]) == 5
    assert keys["INSIDE"][1]["fill"] and not keys["INSIDE"][1]["sent"]
    assert check(keys, skipped, C) == []


def test_a_late_key_fails():
    lines = good_log()
    lines[6] = log("INSIDE", 4700, 3, 4, "colour 70,0,128")
    keys, skipped = parse(lines)
    assert any("INSIDE key 3: at 4700 ms" in p for p in check(keys, skipped, C))


def test_a_missing_fill_fails():
    lines = [line for line in good_log() if "fill" not in line]
    keys, skipped = parse(lines)
    assert any("INSIDE: 3 keys logged" in p for p in check(keys, skipped, C))


def test_an_end_that_isnt_the_look_twice_fails():
    lines = good_log()
    lines[-1] = log("OUTSIDE", 6311, 5, 5, "colour 0,0,0")
    keys, skipped = parse(lines)
    assert any("OUTSIDE: doesn't end on its look twice" in p for p in check(keys, skipped, C))


def test_a_light_left_off_is_reported():
    lines = [line for line in good_log() if "OUTSIDE" not in line] + ["[intro] OUTSIDE: skipped (off)\n"]
    keys, skipped = parse(lines)
    assert any("OUTSIDE: skipped (off)" in p for p in check(keys, skipped, C))


def test_a_build_with_the_effects_off_passes_with_no_welcome():
    lines = ["[intro] replay\n", "[intro] INSIDE: skipped (effects off)\n",
             "[intro] OUTSIDE: skipped (effects off)\n"]
    keys, skipped = parse(lines)
    assert effects_off(skipped) and keys == {}
    assert check(keys, skipped, C) == []


def test_effects_off_on_one_light_only_still_fails():
    lines = [line for line in good_log() if "INSIDE" not in line] + ["[intro] INSIDE: skipped (effects off)\n"]
    keys, skipped = parse(lines)
    assert not effects_off(skipped)
    assert any("INSIDE: skipped (effects off)" in p for p in check(keys, skipped, C))
