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
