"""Bench ELM327 adapter for the yellow deck's car health check: no car, no Vgate needed.

answer() is the whole reply table (pure, host-tested). Run this file as a script and the PC
advertises as a BLE peripheral named IOS-Vlink (service FFF0, FFF1 notify, FFF2 write) and answers
each command line with answer() + "\\r>". A small console changes the state live: speed N, rpm N,
volts X, coolant N, ltft X, code P0301, pending P0420, clear, mil on|off, engine on|off,
silent on|off, quit. "engine off" is the ignition off (the car answers NO DATA); "silent on" is a lost
link (the car answers nothing at all, so the deck's 2 s timeout runs).
Read-only like the firmware: mode 04 (clear codes) and anything unknown answer "?".
Needs `bless` (pip install -r tools/requirements.txt) only when run as a script.
"""
import re

# What the City answered on 2026-10-19 (three ECUs). 05 and 0F are missing (67/68 instead), and 2F.
CITY_PIDS = [0x01, 0x03, 0x04, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x11, 0x13, 0x15, 0x1C,
             0x1F, 0x21, 0x23, 0x2E, 0x30, 0x31, 0x33, 0x34, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
             0x47, 0x49, 0x4A, 0x4C, 0x52, 0x56, 0x67, 0x68, 0x6C]

DEFAULTS = {"speed": 0, "rpm": 800, "volts": 12.6, "coolant": 90, "ltft": 0.0, "stft": 0.0,
            "codes": [], "pending": [], "mil": False, "engine": True, "silent": False}


def _hex(*b):
    return " ".join(f"{x & 0xFF:02X}" for x in b)


def _support(block):
    """Bitmask reply for 0100/0120/0140/0160: bit 31 is PID block+1, bit 0 'next block has more'."""
    bits = 0
    for p in CITY_PIDS:
        if block < p <= block + 0x20:
            bits |= 1 << (31 - (p - block - 1))
    if any(p > block + 0x20 for p in CITY_PIDS):
        bits |= 1
    return _hex(0x41, block, *bits.to_bytes(4, "big"))


def _trim(pct):
    return round(128 + float(pct) * 128 / 100)


def _code_bytes(code):
    m = re.fullmatch(r"([PCBU])([0-3])([0-9A-F]{3})", code.upper())
    if not m:
        return None
    v = "PCBU".index(m.group(1)) << 14 | int(m.group(2)) << 12 | int(m.group(3), 16)
    return v >> 8, v & 0xFF


def _codes(mode, codes):
    pairs = [p for p in (_code_bytes(c) for c in codes) if p]
    return _hex(mode, len(pairs), *[b for p in pairs for b in p])


def _mode01(pid, s):
    if pid in (0x00, 0x20, 0x40, 0x60):
        return _support(pid)
    if pid not in CITY_PIDS:
        return "NO DATA"
    rpm4 = int(s["rpm"] * 4)
    maf = 350                                                # 3.5 g/s
    mv = 14000                                               # module volts 14.0
    table = {
        0x01: lambda: _hex(0x41, 0x01, (0x80 if s["mil"] else 0) | min(len(s["codes"]), 0x7F), 0, 0, 0),
        0x03: lambda: _hex(0x41, 0x03, 0x02, 0x00),
        0x04: lambda: _hex(0x41, 0x04, 0x40),
        0x06: lambda: _hex(0x41, 0x06, _trim(s["stft"])),
        0x07: lambda: _hex(0x41, 0x07, _trim(s["ltft"])),
        0x0B: lambda: _hex(0x41, 0x0B, 35),
        0x0C: lambda: _hex(0x41, 0x0C, rpm4 >> 8, rpm4),
        0x0D: lambda: _hex(0x41, 0x0D, int(s["speed"])),
        0x10: lambda: _hex(0x41, 0x10, maf >> 8, maf),
        0x11: lambda: _hex(0x41, 0x11, 20),
        0x42: lambda: _hex(0x41, 0x42, mv >> 8, mv),
        0x49: lambda: _hex(0x41, 0x49, 20),
        0x4A: lambda: _hex(0x41, 0x4A, 20),
        0x52: lambda: _hex(0x41, 0x52, 77),                  # 30 % ethanol
        0x67: lambda: _hex(0x41, 0x67, 0x01, int(s["coolant"]) + 40),
        0x68: lambda: _hex(0x41, 0x68, 0x01, 25 + 40),
    }
    return table.get(pid, lambda: _hex(0x41, pid, 0, 0))()


def answer(cmd, state):
    """The ELM327 text for one command (no trailing CR, any case) before the '>' prompt, or None
    when nothing at all is sent (silent: a lost link, not even the prompt)."""
    c = cmd.replace(" ", "").upper()
    s = {**DEFAULTS, **state}
    if c.startswith("AT"):
        if c == "ATZ":
            return "ELM327 v1.5"
        if c == "ATRV":
            return f"{float(s['volts']):.1f}V"
        return "OK"
    mode01 = len(c) == 4 and c.startswith("01")
    if c in ("03", "07") or mode01:
        if s["silent"]:
            return None
        if not s["engine"]:
            return "NO DATA"
        if c == "03":
            return _codes(0x43, s["codes"])
        if c == "07":
            return _codes(0x47, s["pending"])
        try:
            return _mode01(int(c[2:], 16), s)
        except ValueError:
            return "?"
    return "?"


def reply_bytes(cmd, state):
    """What the adapter sends back for one command line: the answer and its "\\r>" prompt, or None."""
    a = answer(cmd, state)
    return None if a is None else (a + "\r>").encode()


def chunks(reply, size=20):
    """The reply (with its "\r>", str or bytes) as notification-sized slices, in order, like a real adapter."""
    b = reply if isinstance(reply, bytes) else reply.encode()
    return [b[i:i + size] for i in range(0, len(b), size)] or [b""]


def _console_cmd(line, st):
    """Applies one console line to the state; returns False to quit."""
    w = line.split()
    if not w:
        return True
    k, arg = w[0].lower(), (w[1] if len(w) > 1 else "")
    try:
        if k == "quit":
            return False
        if k in ("speed", "rpm", "coolant"):
            st[k] = int(arg)
        elif k in ("volts", "ltft"):
            st[k] = float(arg)
        elif k == "code":
            st["codes"].append(arg.upper())
        elif k == "pending":
            st["pending"].append(arg.upper())
        elif k == "clear":
            st["codes"].clear()
            st["pending"].clear()
        elif k in ("mil", "engine", "silent"):
            st[k] = arg.lower() == "on"
        else:
            print("speed|rpm|coolant N, volts|ltft X, code|pending P0301, clear, mil|engine|silent on|off, quit")
    except ValueError:
        print("bad value")
    return True


def _main():
    import asyncio
    from bless import BlessServer, GATTAttributePermissions, GATTCharacteristicProperties

    st = {k: (list(v) if isinstance(v, list) else v) for k, v in DEFAULTS.items()}
    SVC = "0000fff0-0000-1000-8000-00805f9b34fb"
    NOTIFY = "0000fff1-0000-1000-8000-00805f9b34fb"
    WRITE = "0000fff2-0000-1000-8000-00805f9b34fb"

    async def run():
        server = BlessServer(name="IOS-Vlink", loop=asyncio.get_running_loop())
        buf = bytearray()

        def on_write(characteristic, value, **_):
            buf.extend(value)
            while b"\r" in buf:
                i = buf.index(b"\r")
                cmd = bytes(buf[:i]).decode("ascii", "ignore")
                del buf[:i + 1]
                reply = reply_bytes(cmd, st)
                print(f"{cmd!r} -> {reply!r}")
                if reply is None:                         # silent: a lost link sends nothing
                    continue
                for part in chunks(reply):                # 20 bytes: the default ATT MTU of 23
                    server.get_characteristic(NOTIFY).value = bytearray(part)
                    server.update_value(SVC, NOTIFY)

        server.write_request_func = on_write
        await server.add_new_service(SVC)
        await server.add_new_characteristic(SVC, NOTIFY, GATTCharacteristicProperties.notify,
                                            None, GATTAttributePermissions.readable)
        await server.add_new_characteristic(
            SVC, WRITE,
            GATTCharacteristicProperties.write | GATTCharacteristicProperties.write_without_response,
            None, GATTAttributePermissions.writeable)
        await server.start()
        print("advertising as IOS-Vlink; type commands (quit to stop)")
        loop = asyncio.get_running_loop()
        while _console_cmd(await loop.run_in_executor(None, input, "> "), st):
            pass
        await server.stop()

    asyncio.run(run())


if __name__ == "__main__":
    _main()
