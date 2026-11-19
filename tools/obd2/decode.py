"""Decode ELM327 replies and deck logs into readable OBD-II values, offline. Never talks to a car.

    python tools/obd2/decode.py "41 0C 1A F8"            -> RPM 1726.0 rpm
    python tools/obd2/decode.py "7E8 06 41 00 BE 3E B8 11"  -> supported PIDs
    python tools/obd2/decode.py "43 02 01 33 00 00"      -> stored codes P0133
    python tools/obd2/decode.py --log capture.txt        -> every [obd] line of a deck log, decoded

Takes what the adapter printed before its '>' prompt: spaces or none, with or without CAN headers
(ATH1: "7E8 03 41 0D 2A" or 29-bit "18DAF110 03 41 0D 2A"), several ECUs on separate lines, multi-frame "0:" / "1:" lines.
The formulas are SAE J1979 / ISO 15031-5 (the same ones src/util/car/obd.cpp uses). Pure functions,
tested by tools/obd2/test_decode.py. Skill: .claude/skills/obd2/SKILL.md
"""
import re
import sys

# pid -> (name, unit, data bytes, formula(A, B, C, D))
PIDS = {
    0x01: ("Monitor status", "", 4, None),
    0x03: ("Fuel system status", "", 2, lambda a, b, c, d: a),
    0x04: ("Engine load", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x05: ("Coolant temp", "C", 1, lambda a, b, c, d: a - 40),
    0x06: ("Short fuel trim B1", "%", 1, lambda a, b, c, d: (a - 128) * 100 / 128),
    0x07: ("Long fuel trim B1", "%", 1, lambda a, b, c, d: (a - 128) * 100 / 128),
    0x0B: ("Intake MAP", "kPa", 1, lambda a, b, c, d: a),
    0x0C: ("RPM", "rpm", 2, lambda a, b, c, d: (256 * a + b) / 4),
    0x0D: ("Speed", "km/h", 1, lambda a, b, c, d: a),
    0x0E: ("Timing advance", "deg", 1, lambda a, b, c, d: a / 2 - 64),
    0x0F: ("Intake air temp", "C", 1, lambda a, b, c, d: a - 40),
    0x10: ("MAF", "g/s", 2, lambda a, b, c, d: (256 * a + b) / 100),
    0x11: ("Throttle", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x13: ("O2 sensors present", "", 1, lambda a, b, c, d: a),
    0x15: ("O2 B1S2 voltage", "V", 2, lambda a, b, c, d: a / 200),
    0x1C: ("OBD standard", "", 1, lambda a, b, c, d: a),
    0x1F: ("Run time since start", "s", 2, lambda a, b, c, d: 256 * a + b),
    0x21: ("Distance with MIL on", "km", 2, lambda a, b, c, d: 256 * a + b),
    0x23: ("Fuel rail pressure (gauge)", "kPa", 2, lambda a, b, c, d: 10 * (256 * a + b)),
    0x2E: ("Commanded EVAP purge", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x2F: ("Fuel level", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x30: ("Warm-ups since codes cleared", "", 1, lambda a, b, c, d: a),
    0x31: ("Distance since codes cleared", "km", 2, lambda a, b, c, d: 256 * a + b),
    0x33: ("Barometric pressure", "kPa", 1, lambda a, b, c, d: a),
    0x34: ("O2 S1 lambda / current", "ratio", 4, lambda a, b, c, d: (256 * a + b) * 2 / 65536),
    0x41: ("Monitor status this cycle", "", 4, None),
    0x42: ("Control module voltage", "V", 2, lambda a, b, c, d: (256 * a + b) / 1000),
    0x43: ("Absolute load", "%", 2, lambda a, b, c, d: (256 * a + b) * 100 / 255),
    0x44: ("Commanded lambda", "ratio", 2, lambda a, b, c, d: (256 * a + b) * 2 / 65536),
    0x45: ("Relative throttle", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x46: ("Ambient air temp", "C", 1, lambda a, b, c, d: a - 40),
    0x47: ("Absolute throttle B", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x49: ("Accelerator pedal D", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x4A: ("Accelerator pedal E", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x4C: ("Commanded throttle actuator", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x51: ("Fuel type", "", 1, lambda a, b, c, d: a),
    0x52: ("Ethanol fuel", "%", 1, lambda a, b, c, d: a * 100 / 255),
    0x56: ("Long secondary O2 trim B1", "%", 1, lambda a, b, c, d: (a - 128) * 100 / 128),
    0x5C: ("Engine oil temp", "C", 1, lambda a, b, c, d: a - 40),
    0x5E: ("Engine fuel rate", "L/h", 2, lambda a, b, c, d: (256 * a + b) / 20),
    0x67: ("Coolant temp (sensor 1)", "C", 3, lambda a, b, c, d: b - 40 if a & 1 else None),
    0x68: ("Intake air temp (sensor 1)", "C", 3, lambda a, b, c, d: b - 40 if a & 1 else None),
    0x6C: ("Commanded throttle / position", "%", 2, None),
}
SUPPORT_BLOCKS = (0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0)

# A reply ID: 11-bit 7E8..7EF, or 29-bit 18DAF1xx (Hondas, the City included); PCI length may follow.
_HEADER = re.compile(r"^(7E[89A-F]|18DAF1[0-9A-F]{2})")


def hex_line(line):
    """One adapter line as upper-case hex digits, or None when it isn't a data line.
    Drops spaces and a multi-frame "0:" prefix."""
    s = line.strip().upper().replace(" ", "")
    if len(s) >= 2 and s[1] == ":":
        s = s[2:]
    return s if s and re.fullmatch(r"[0-9A-F]+", s) else None


def strip_header(h, marker):
    """Data after marker ("41"+pid, "43"...) when it starts the line or follows a 7Ex header
    (and its length byte). None otherwise."""
    if h.startswith(marker):
        return h[len(marker):]
    m = _HEADER.match(h)
    if m:
        for cut in (m.end(), m.end() + 2):
            if h[cut:].startswith(marker):
                return h[cut + len(marker):]
    return None


def lines(raw):
    return [ln for ln in re.split(r"[\r\n|]+", raw) if ln.strip()]


def payload(h):
    """A hex line without its 7Ex header and length byte: the reply itself ("410C1AF8")."""
    m = _HEADER.match(h)
    if not m or h.startswith("4"):
        return h
    rest = h[m.end():]
    return rest if rest[:1] == "4" else rest[2:]


def status(raw):
    """The adapter's own status word (NO DATA, UNABLE TO CONNECT, ...) when no line holds a reply."""
    if any(payload(h)[:1] == "4" for h in map(hex_line, lines(raw)) if h):
        return None
    up = raw.upper()
    for word in ("NO DATA", "UNABLE TO CONNECT", "CAN ERROR", "BUS INIT", "BUS ERROR", "STOPPED",
                 "BUFFER FULL", "?"):
        if word in up:
            return word
    return None


def mode01(raw, pid):
    """Every ECU's value for mode 01 pid: [(value or None, unit)]. Empty when none answered."""
    name, unit, nbytes, f = PIDS.get(pid, (f"PID {pid:02X}", "", 0, None))
    out = []
    for ln in lines(raw):
        h = hex_line(ln)
        if not h:
            continue
        data = strip_header(h, f"41{pid:02X}")
        if data is None or len(data) < 2 * max(nbytes, 1):
            continue
        b = [int(data[i:i + 2], 16) for i in range(0, len(data) - 1, 2)] + [0, 0, 0, 0]
        out.append((f(*b[:4]) if f and nbytes else None, unit))
    return out


def supported(raw, block):
    """Supported PIDs from a 01<block> reply (block 00, 20, 40...), ORed over every ECU."""
    bits, any_ = 0, False
    for ln in lines(raw):
        h = hex_line(ln)
        data = strip_header(h, f"41{block:02X}") if h else None
        if data and len(data) >= 8:
            bits |= int(data[:8], 16)
            any_ = True
    if not any_:
        return None
    return [block + i + 1 for i in range(32) if bits >> (31 - i) & 1]


def dtc(b0, b1):
    """Two DTC bytes as "P0301"."""
    return "PCBU"[b0 >> 6] + str((b0 >> 4) & 3) + f"{b0 & 15:X}{b1 >> 4:X}{b1 & 15:X}"


def codes(raw, mode="03"):
    """Codes from a mode 03 / 07 / 0A reply, de-duplicated over ECUs. None for no such reply.
    On CAN the reply is <mode+40> <count> <2 bytes per code>; multi-frame lines are joined."""
    marker = f"{int(mode, 16) + 0x40:02X}"
    joined, singles = "", []
    for ln in lines(raw):
        h = hex_line(ln)
        if not h:
            continue
        if ln.strip()[1:2] == ":":
            joined += h
        else:
            singles.append(h)
    found, seen = [], False
    for h in singles + ([joined] if joined else []):
        data = strip_header(h, marker)
        if data is None or len(data) < 2:
            continue
        seen = True
        n = int(data[:2], 16)
        data = data[2:]
        for k in range(n):
            pair = data[4 * k:4 * k + 4]
            if len(pair) < 4:
                break
            b0, b1 = int(pair[:2], 16), int(pair[2:], 16)
            if (b0, b1) != (0, 0) and dtc(b0, b1) not in found:
                found.append(dtc(b0, b1))
    if seen:
        return found
    return [] if "NO DATA" in raw.upper() else None


def describe(raw, request=None):
    """One human line for a reply (and the request that asked it, when known)."""
    word = status(raw)
    if word:
        return word
    req = (request or "").strip().upper().replace(" ", "")
    if req.startswith("AT"):
        return raw.strip()
    first = next((hex_line(ln) for ln in lines(raw) if hex_line(ln)), None)
    if not first:
        return raw.strip()
    body = payload(first)
    if req in ("03", "07", "0A") or body[:2] in ("43", "47", "4A"):
        mode = req or f"{int(body[:2], 16) - 0x40:02X}"
        found = codes(raw, mode)
        return "codes: " + (" ".join(found) if found else "none") if found is not None else raw.strip()
    if body.startswith("41") and len(body) >= 4:
        pid = int(body[2:4], 16)
        if pid in SUPPORT_BLOCKS:
            pids = supported(raw, pid)
            return "supported: " + " ".join(f"{p:02X}" for p in pids) if pids is not None else raw.strip()
        name = PIDS.get(pid, (f"PID {pid:02X}",))[0]
        vals = mode01(raw, pid)
        shown = [f"{v:.2f}".rstrip("0").rstrip(".") + (f" {u}" if u else "") if v is not None else "n/a"
                 for v, u in vals]
        return f"{name}: " + ", ".join(shown) if shown else raw.strip()
    return raw.strip()


_LOG = re.compile(r'\[obd\] (\S+) -> "(.*)"')


def decode_log(text):
    """Every '[obd] <cmd> -> "<raw>"' line of a deck serial log, decoded. Raw uses '|' for newlines."""
    out = []
    for line in text.splitlines():
        m = _LOG.search(line)
        if m:
            out.append(f"{m.group(1)}: {describe(m.group(2).replace('|', chr(10)), m.group(1))}")
    return out


def main(argv):
    if len(argv) >= 2 and argv[0] == "--log":
        with open(argv[1], encoding="utf-8", errors="replace") as f:
            for row in decode_log(f.read()):
                print(row)
        return 0
    if not argv:
        print(__doc__)
        return 2
    print(describe(" ".join(argv).replace("\\n", "\n").replace("\\r", "\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
