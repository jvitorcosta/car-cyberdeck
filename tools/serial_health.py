"""Health summary of a deck's serial log: resets, crashes, heap and stacks.

    python tools/serial_health.py <log> [--strict]

<log> is a capture from tools/capture_serial.py (each line starts with seconds since the start)
or a plain `pio device monitor` log. It reads the lines all three firmwares print:
  - resets: the ROM's `rst:0x.. (REASON)`, `[boot] reset reason: …`, the crash ring's `[crash] …`;
  - crashes: `Guru Meditation Error`, `abort() was called`, `Backtrace:`, the task watchdog,
    `Brownout detector was triggered`;
  - heap: the black deck's `[mem] <where> free=… largest=… stackfree=… [allocfail=N]`, the yellow
    deck's `[mem] heap … (min …, block …, fails …)  stack left: link …`, the OBD deck's
    `[mem] heap … (min …)  stack left: …`, and `[mem] alloc of N B failed`;
  - TLS: mbedTLS `-32512` (SSL_ALLOC_FAILED, the heap) and other `-0x…`/`-3xxxx` handshake codes.
--strict exits 1 when the log holds a crash, a brownout or a failed allocation (for scripts).
Standard library only (CI runs tools/test_serial_health.py with unittest).
"""
import re
import sys

ROM_RST = re.compile(r"rst:0x([0-9a-f]+) \(([A-Z0-9_]+)\)")
BOOT_REASON = re.compile(r"\[boot\] reset reason: (.+)")
CRASH = re.compile(r"\[crash\] (last run|\d+): (.+)")
BLACK_MEM = re.compile(r"\[mem\] (\S+) free=(\d+) largest=(\d+) stackfree=(\d+)(?: allocfail=(\d+))?")
HEAP_MEM = re.compile(r"\[mem\] heap (\d+) \(min (\d+)(?:, block (\d+))?(?:, fails (\d+))?\)\s+stack left: (.+)")
ALLOC_FAIL = re.compile(r"\[mem\] alloc of (\d+) B failed")
STACK = re.compile(r"([\w-]+) (\d+)")
TLS_ERR = re.compile(r"(-32512|-0x7F00|SSL_ALLOC_FAILED|mbedtls_ssl_handshake returned -?(?:0x)?[0-9A-Fa-f]+)")

# Lines that mean the firmware crashed (or the supply sagged), with a short name each.
FATAL = [
    ("Guru Meditation Error", "panic (Guru Meditation)"),
    ("abort() was called", "abort()"),
    ("task_wdt: Task watchdog got triggered", "task watchdog"),
    ("Brownout detector was triggered", "brownout"),
    ("Stack canary watchpoint triggered", "stack overflow (canary)"),
    ("***ERROR*** A stack overflow in task", "stack overflow"),
]


def strip_time(raw):
    """A capture_serial.py line is '   12.3 text'; a plain monitor line is just 'text'."""
    m = re.match(r"\s*(\d+\.\d+) (.*)", raw)
    return (float(m.group(1)), m.group(2)) if m else (None, raw)


def analyze(lines):
    """The summary of an iterable of log lines, as a dict (see report() for the fields)."""
    out = {"lines": 0, "resets": [], "boot_reasons": [], "crash_ring": [], "fatal": [],
           "alloc_fails": [], "tls_errors": 0, "heap": None, "stacks": {}, "deck": None}
    free, largest, heap_min = [], [], []
    alloc_counted = 0
    for raw in lines:
        t, s = strip_time(raw.rstrip("\r\n"))
        out["lines"] += 1
        m = ROM_RST.search(s)
        if m:
            out["resets"].append((t, "0x%s %s" % (m.group(1), m.group(2))))
        m = BOOT_REASON.search(s)
        if m:
            out["boot_reasons"].append((t, m.group(1).strip()))
        m = CRASH.search(s)
        if m:
            out["crash_ring"].append((m.group(1), m.group(2).strip()))
        for needle, name in FATAL:
            if needle in s:
                out["fatal"].append((t, name))
        m = ALLOC_FAIL.search(s)
        if m:
            out["alloc_fails"].append(int(m.group(1)))
        if TLS_ERR.search(s):
            out["tls_errors"] += 1
        m = BLACK_MEM.search(s)
        if m:
            out["deck"] = out["deck"] or "black"
            free.append(int(m.group(2)))
            largest.append(int(m.group(3)))
            note_stack(out["stacks"], m.group(1), int(m.group(4)))
            alloc_counted += int(m.group(5) or 0)
            continue
        m = HEAP_MEM.search(s)
        if m:
            out["deck"] = out["deck"] or ("yellow" if m.group(3) is not None else "obd")
            free.append(int(m.group(1)))
            heap_min.append(int(m.group(2)))
            if m.group(3) is not None:
                largest.append(int(m.group(3)))
            for name, n in STACK.findall(m.group(5)):
                note_stack(out["stacks"], name, int(n))
    if free:
        out["heap"] = {
            "samples": len(free), "free_min": min(free), "free_max": max(free),
            "largest_min": min(largest) if largest else None,
            "largest_max": max(largest) if largest else None,
            "min_ever": min(heap_min) if heap_min else None,
            "allocfail_counted": alloc_counted,
        }
    return out


def note_stack(stacks, name, n):
    """Keeps the lowest free stack seen per task (bytes or words, as the line printed it)."""
    stacks[name] = min(n, stacks.get(name, n))


def problems(summary):
    """The findings that should fail --strict."""
    found = [name for _, name in summary["fatal"]]
    if summary["alloc_fails"]:
        found.append("%d failed allocations" % len(summary["alloc_fails"]))
    heap = summary["heap"]
    if heap and heap["allocfail_counted"]:
        found.append("%d failed allocations ([mem] allocfail)" % heap["allocfail_counted"])
    return found


def report(summary):
    """The summary as readable text."""
    r = ["lines: %d  deck: %s" % (summary["lines"], summary["deck"] or "?")]
    for t, what in summary["resets"]:
        r.append("reset (ROM): %s%s" % (what, at(t)))
    for t, what in summary["boot_reasons"]:
        r.append("boot reason: %s%s" % (what, at(t)))
    for which, line in summary["crash_ring"]:
        r.append("crash ring %s: %s" % (which, line))
    for t, name in summary["fatal"]:
        r.append("FATAL %s%s" % (name, at(t)))
    if summary["alloc_fails"]:
        sizes = summary["alloc_fails"]
        r.append("failed allocations: %d (largest %d B, smallest %d B)" % (len(sizes), max(sizes), min(sizes)))
    if summary["tls_errors"]:
        r.append("TLS errors: %d (-32512 = mbedTLS could not allocate: internal heap)" % summary["tls_errors"])
    heap = summary["heap"]
    if heap:
        line = "heap: %d samples, free %d-%d" % (heap["samples"], heap["free_min"], heap["free_max"])
        if heap["largest_min"] is not None:
            line += ", largest block %d-%d" % (heap["largest_min"], heap["largest_max"])
        if heap["min_ever"] is not None:
            line += ", lowest ever %d" % heap["min_ever"]
        r.append(line)
    if summary["stacks"]:
        r.append("lowest free stack: " + ", ".join("%s %d" % kv for kv in sorted(summary["stacks"].items())))
    bad = problems(summary)
    r.append("verdict: " + ("PROBLEMS: " + "; ".join(bad) if bad else "no crash, brownout or failed allocation"))
    return "\n".join(r)


def at(t):
    return "" if t is None else " at %.1fs" % t


def main(argv):
    args = [a for a in argv if a != "--strict"]
    if len(args) != 1:
        sys.exit(__doc__)
    with open(args[0], encoding="utf-8", errors="replace") as f:
        summary = analyze(f)
    print(report(summary))
    return 1 if "--strict" in argv and problems(summary) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
