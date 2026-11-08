"""Reads, or sets, the Bluetooth address a PS3 controller (DualShock 3) connects to: its "master".

The yellow deck's PS3 controller connects to whatever address is stored in it, so write the deck's
there once, over USB from the PC (the deck prints it at boot: "[pad] deck address ..."):

    python tools/ds3_pair.py                     # shows the controller's master and its own address
    python tools/ds3_pair.py aa:bb:cc:dd:ee:ff   # sets the master, then shows it

Needs pyusb and a libusb-1.0 backend (pip install pyusb libusb-package), and on Windows the
controller on a libusb-compatible driver (libusbK or WinUSB, e.g. installed with Zadig).
"""
import sys

import usb.backend.libusb1
import usb.core

SONY, DS3 = 0x054C, 0x0268
REPORT_MASTER, REPORT_OWN = 0x03F5, 0x03F2  # HID feature reports 0xF5 (master) and 0xF2 (own address)


def backend():
    """libusb-1.0 only: libusb0 (pyusb's fallback) fails the controller's control transfers."""
    try:
        import libusb_package  # bundles libusb-1.0 for Windows

        return libusb_package.get_libusb1_backend()
    except ImportError:
        pass
    try:
        import pathlib
        import platform

        import libusb  # the "libusb" package bundles the DLL too

        arch = {"AMD64": "x86_64", "x86": "x86", "ARM64": "arm64"}.get(platform.machine(), "x86_64")
        dll = pathlib.Path(libusb.__file__).parent / "_platform" / "windows" / arch / "libusb-1.0.dll"
        if dll.exists():
            return usb.backend.libusb1.get_backend(find_library=lambda _: str(dll))
    except ImportError:
        pass
    be = usb.backend.libusb1.get_backend()
    if be is None:
        sys.exit("no libusb-1.0 found: pip install libusb-package")
    return be


def fmt(b):
    return ":".join(f"{x:02x}" for x in b)


def main():
    pad = usb.core.find(idVendor=SONY, idProduct=DS3, backend=backend())
    if pad is None:
        sys.exit("no PS3 controller on USB (or no libusb driver for it)")
    if len(sys.argv) > 1:
        mac = bytes(int(x, 16) for x in sys.argv[1].split(":"))
        if len(mac) != 6:
            sys.exit("the address is six bytes: aa:bb:cc:dd:ee:ff")
        pad.ctrl_transfer(0x21, 0x09, REPORT_MASTER, 0, b"\x01\x00" + mac)  # SET_REPORT
    master = pad.ctrl_transfer(0xA1, 0x01, REPORT_MASTER, 0, 8)  # GET_REPORT
    own = pad.ctrl_transfer(0xA1, 0x01, REPORT_OWN, 0, 17)
    print("master:", fmt(master[2:8]))
    print("own:   ", fmt(own[4:10]))


if __name__ == "__main__":
    main()
