import os
import sys

sys.path.insert(0, os.path.dirname(__file__))

from obd_probe import READ_ONLY, atrv_volts, looks_like_adapter, pick_service, pid42_volts  # noqa: E402

SERIAL = ("0000fff0-0000-1000-8000-00805f9b34fb",
          [("0000fff1-0000-1000-8000-00805f9b34fb", ["notify"]),
           ("0000fff2-0000-1000-8000-00805f9b34fb", ["write-without-response"])])
OTHER = ("0000180a-0000-1000-8000-00805f9b34fb",
         [("00002a29-0000-1000-8000-00805f9b34fb", ["notify"]),
          ("00002a24-0000-1000-8000-00805f9b34fb", ["write"])])


def test_only_read_only_commands_are_listed():
    for cmd in READ_ONLY:
        assert cmd.startswith("AT") or cmd[:2] in ("01",) or cmd in ("03", "07")
    assert not {"04", "ATH1", "0902"} & set(READ_ONLY)


def test_serial_service_wins_over_an_earlier_one():
    assert pick_service([OTHER, SERIAL]) == (SERIAL[1][0][0], SERIAL[1][1][0])


def test_unknown_128_bit_service_is_used_when_it_has_both():
    custom = ("e7810a71-73ae-499d-8c15-faa9aef0c3f2", [("bef8d6c9-9c21-4c9e-b632-bd58c1009f9f", ["notify", "write"])])
    assert pick_service([custom]) == (custom[1][0][0], custom[1][0][0])


def test_a_service_missing_notify_or_write_is_not_picked():
    assert pick_service([("0000fff0-0000-1000-8000-00805f9b34fb", [("a", ["read"]), ("b", ["write"])])]) is None


def test_adapter_names():
    assert looks_like_adapter("IOS-Vlink")
    assert looks_like_adapter("vGate iCar Pro")
    assert not looks_like_adapter("Honda Keyless")
    assert not looks_like_adapter(None)


def test_voltages():
    assert atrv_volts("14.1V") == 14.1
    assert atrv_volts("12V") == 12.0
    assert atrv_volts("NO DATA") is None
    assert abs(pid42_volts("41 42 37 FF") - 14.335) < 0.001
    assert pid42_volts("NO DATA") is None


# ---- the fuel-level research probe (--fuel) ----
from obd_probe import FUEL_PROBE, check_allowed, decode_fuel, is_29bit, plausible_litres, reply_bytes  # noqa: E402

CR = chr(13)


def test_fuel_probe_is_exactly_four_requests_and_no_write_service():
    assert FUEL_PROBE == ("ATCP 18", "ATSH DA60F1", "227029", "ATZ")
    # the request is a single read-data-by-identifier (service 22), nothing else is added to the list
    assert [c for c in FUEL_PROBE if not c.startswith("AT")] == ["227029"]
    assert not {"04", "ATH1", "0902", "2E7029", "2F7029", "317029", "107029"} & set(FUEL_PROBE)
    assert not {"ATCP 18", "ATSH DA60F1", "227029"} & set(READ_ONLY)   # the default run never sends them


def test_is_29bit_reads_atdpn():
    assert is_29bit("A7")
    assert is_29bit("7")
    assert is_29bit("ATDPN" + CR + "A7")
    assert not is_29bit("A0")   # no bus found yet: ask again after 0100
    assert not is_29bit("A9")
    assert not is_29bit("A6")
    assert not is_29bit("6")
    assert not is_29bit("NO DATA")
    assert not is_29bit("")


def test_reply_bytes_flattens_a_multi_frame_reply():
    raw = "01C" + CR + "0: 62 70 29 00 01 02" + CR + "1: 03 04 05 06 07 08 09" + CR + "2: 0A 0B 0C 0D 0E 0F 10"
    assert reply_bytes(raw)[:6] == [0x62, 0x70, 0x29, 0, 1, 2]
    assert len(reply_bytes(raw)) == 6 + 7 + 7


def test_decode_fuel_both_ways():
    # 62 70 29 then 12 filler bytes, then 0x0834 = 2100 -> 21.00 L after the header
    body = [0] * 12 + [0x08, 0x34] + [0] * 4
    raw = "62 70 29 " + " ".join("%02X" % b for b in body)
    got = decode_fuel(raw)
    assert abs(got["after_header"] - 21.0) < 1e-9
    # counted over the whole reply (header included) bytes 12-13 sit three bytes earlier in the body: another number
    assert got["with_header"] is not None and got["with_header"] != got["after_header"]
    assert plausible_litres(got["after_header"])


def test_decode_fuel_short_or_missing():
    assert decode_fuel("NO DATA") == {}
    assert decode_fuel("7F 22 31") == {}
    assert decode_fuel("") == {}
    short = decode_fuel("62 70 29 01 02")
    assert short["after_header"] is None and short["with_header"] is None


def test_plausible_litres():
    assert plausible_litres(0.0)
    assert plausible_litres(44.0)
    assert not plausible_litres(None)
    assert not plausible_litres(-1.0)
    assert not plausible_litres(300.0)


def test_gate_allows_the_fuel_requests_only_under_fuel():
    import pytest
    for cmd in ("ATCP 18", "ATSH DA60F1", "227029"):
        check_allowed(cmd, True)
        with pytest.raises(SystemExit):
            check_allowed(cmd, False)
    check_allowed("0100", False)
    check_allowed("ATZ", False)


def test_gate_refuses_the_neighbours():
    import pytest
    for cmd in ("227028", "22702A", "227030", "2270290", "227022", "ATSH DA61F1", "ATSH DA10F1", "ATCP 19",
                "04", "ATH1", "0902", "2E7029", "2F7029", "317029", "107029", "ATMA", "ATPP 0C SV 00"):
        with pytest.raises(SystemExit):
            check_allowed(cmd, True)
