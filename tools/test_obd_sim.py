"""Host tests for tools/obd_sim.py's reply table (no BLE needed). Run: python -m pytest tools/test_obd_sim.py -q"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from obd_sim import answer  # noqa: E402


def test_speed_reply():
    assert answer("010D", {"speed": 60}) == "41 0D 3C"


def test_atrv():
    assert answer("ATRV", {"volts": 12.6}) == "12.6V"
    assert answer("atrv", {}) == "12.6V"


def test_unsupported_is_no_data():
    assert answer("012F", {}) == "NO DATA"
    assert answer("0105", {}) == "NO DATA"


def test_clear_codes_refused():
    assert answer("04", {}) == "?"
    assert answer("0902", {}) == "?"


def _bits(reply, block=0x00):
    b = bytes.fromhex(reply.replace(" ", ""))
    assert b[:2] == bytes([0x41, block])
    return int.from_bytes(b[2:6], "big")


def test_supported_scan_matches_city():
    bits = _bits(answer("0100", {}))

    def has(pid):
        return bool(bits >> (32 - pid) & 1)

    for pid in (0x0C, 0x0D, 0x10, 0x06, 0x07):
        assert has(pid)
    for pid in (0x05, 0x0F):
        assert not has(pid)
    assert not _bits(answer("0120", {}), 0x20) >> (32 - 0x0F) & 1      # 2F: no fuel level
    assert has(0x20)                       # later blocks exist


def test_setup_commands():
    assert answer("ATZ", {}) == "ELM327 v1.5"
    for c in ("ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATDPN"):
        assert answer(c, {}) == "OK"
    assert answer("AT E0", {}) == "OK"


def test_values():
    assert answer("010C", {"rpm": 2000}) == "41 0C 1F 40"
    assert answer("0167", {"coolant": 90}) == "41 67 01 82"
    assert answer("0110", {}) == "41 10 01 5E"
    assert answer("0142", {}) == "41 42 36 B0"
    assert answer("0107", {"ltft": 10}) == "41 07 8D"
    assert answer("0152", {}) == "41 52 4D"


def test_status_byte():
    assert answer("0101", {}).startswith("41 01 00")
    assert answer("0101", {"mil": True, "codes": ["P0301", "P0420"]}).startswith("41 01 82")


def test_trouble_codes():
    assert answer("03", {}) == "43 00"
    assert answer("03", {"codes": ["P0301", "U0100"]}) == "43 02 03 01 C1 00"
    assert answer("07", {"pending": ["P0420"]}) == "47 01 04 20"
    assert answer("07", {}) == "47 00"


def test_silent_means_ignition_off():
    s = {"silent": True}
    assert answer("010D", s) == "NO DATA"
    assert answer("03", s) == "NO DATA"
    assert answer("ATRV", s) == "12.6V"
