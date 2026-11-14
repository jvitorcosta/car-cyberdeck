"""Check the yellow deck's welcome on the desk, over USB: no car, no lights needed.
    python tools/intro_check.py <port>

Sends "INTRO" to the deck (replayIntro in src/yellow/ledpanel.cpp), which plays the welcome on each
lit light and logs every key it sends ("[intro] INSIDE +300 ms key 2/4: fill speed 50 effect 58,
no link"). This holds that log to the timeline src/util/lights/ledintro.h describes: each light's
keys, their times (within TOLERANCE_MS), the inside light's fill, and both ending dark-to-look on
the same colour twice. Prints the timeline and PASS or FAIL. The deck doesn't reset (DTR/RTS stay
low). With the lights linked (in the car) the same command plays the welcome for real. While the
controllers' effects are off (LED_EFFECTS 0, src/util/yellow/elkcmd.h) the deck skips the welcome
on both lights ("skipped (effects off)"), and that passes.
Needs pyserial (tools/requirements.txt).
"""
import os
import re
import sys
import time

KEY = re.compile(r"\[intro\] (INSIDE|OUTSIDE) \+(\d+) ms key (\d+)/(\d+): "
                 r"(?:fill speed (\d+) effect (\d+)|colour (\d+),(\d+),(\d+)), (sent|no link)")
SKIP = re.compile(r"\[intro\] (INSIDE|OUTSIDE): skipped \((.+)\)")
TOLERANCE_MS = 150  # the loop's 10 ms pass, plus the serial port's own lag
LISTEN_S = 9
NEVER_PORTS = {"COM12"}  # the black deck: this is the yellow deck's tool
HEADER = os.path.join(os.path.dirname(__file__), "..", "src", "util", "lights", "ledintro.h")


def constants(text):
    """ledintro.h's timing constants, by name."""
    out = {}
    for name in ("FILL_MS", "IN_GAP_MS", "OUT_GAP_MS", "FILL_MODE", "FILL_SPEED"):
        m = re.search(r"constexpr \w+ " + name + r" = (\d+);", text)
        if m:
            out[name] = int(m.group(1))
    return out


def expected(c):
    """{light: [(ms, fill?)]}: what ledintro::plan sends."""
    gap_in, gap_out, fill = c["IN_GAP_MS"], c["OUT_GAP_MS"], c["FILL_MS"]
    delay = gap_in + fill - gap_out
    return {
        "INSIDE": [(0, False), (gap_in, True), (gap_in + fill, False), (2 * gap_in + fill, False)],
        "OUTSIDE": [(0, False)] + [(delay + k * gap_out, False) for k in range(4)],
    }


def parse(lines):
    """({light: [key dict]}, {light: why skipped}) from the deck's log; other lines are ignored."""
    keys, skipped = {}, {}
    for line in lines:
        m = KEY.search(line)
        if m:
            fill = m.group(5) is not None
            keys.setdefault(m.group(1), []).append({
                "ms": int(m.group(2)), "n": int(m.group(3)), "of": int(m.group(4)), "fill": fill,
                "mode": int(m.group(6)) if fill else None, "speed": int(m.group(5)) if fill else None,
                "rgb": None if fill else (int(m.group(7)), int(m.group(8)), int(m.group(9))),
                "sent": m.group(10) == "sent"})
            continue
        m = SKIP.search(line)
        if m:
            skipped[m.group(1)] = m.group(2)
    return keys, skipped


def effects_off(skipped):
    """True when the deck skipped the welcome on both lights because its effects are off
    (LED_EFFECTS 0 in src/util/yellow/elkcmd.h): each light just takes its look."""
    return all(skipped.get(light) == "effects off" for light in ("INSIDE", "OUTSIDE"))


def check(keys, skipped, c):
    """The problems found (an empty list: the welcome is right)."""
    problems = []
    if effects_off(skipped):
        return problems  # a build without the controllers' effects has no welcome: nothing to hold
    for light, want in expected(c).items():
        if light in skipped:
            problems.append(f"{light}: skipped ({skipped[light]}): switch it on and music mode off, then run again")
            continue
        got = keys.get(light, [])
        if len(got) != len(want):
            problems.append(f"{light}: {len(got)} keys logged, {len(want)} expected")
            continue
        for k, (ms, fill) in zip(got, want):
            if abs(k["ms"] - ms) > TOLERANCE_MS:
                problems.append(f"{light} key {k['n']}: at {k['ms']} ms, {ms} expected")
            if k["fill"] != fill:
                problems.append(f"{light} key {k['n']}: {'fill' if k['fill'] else 'colour'}, "
                                f"{'fill' if fill else 'colour'} expected")
            if k["fill"] and (k["mode"] != c["FILL_MODE"] or k["speed"] != c["FILL_SPEED"]):
                problems.append(f"{light} key {k['n']}: effect {k['mode']} speed {k['speed']}, "
                                f"{c['FILL_MODE']} / {c['FILL_SPEED']} expected")
        if got[0]["rgb"] != (0, 0, 0):
            problems.append(f"{light}: starts at {got[0]['rgb']}, dark expected")
        if got[-1]["rgb"] != got[-2]["rgb"] or got[-1]["rgb"] in (None, (0, 0, 0)):
            problems.append(f"{light}: doesn't end on its look twice ({got[-2]['rgb']}, {got[-1]['rgb']})")
    return problems


def timeline(keys):
    rows = []
    for light in ("INSIDE", "OUTSIDE"):
        for k in keys.get(light, []):
            what = f"fill (effect {k['mode']}, speed {k['speed']})" if k["fill"] else "colour %d,%d,%d" % k["rgb"]
            rows.append((k["ms"], f"{k['ms'] / 1000:5.1f} s  {light:<7}  key {k['n']}/{k['of']}  {what}"
                                  f"  [{'sent' if k['sent'] else 'no link'}]"))
    return [r for _, r in sorted(rows)]


def capture(port):
    import serial

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    s.dtr = False  # opening the port would reset the CYD
    s.rts = False
    s.open()
    try:
        s.reset_input_buffer()
        s.write(b"INTRO\n")
        lines, until = [], time.time() + LISTEN_S
        while time.time() < until:
            line = s.readline().decode("utf-8", errors="replace")
            if line:
                lines.append(line)
        return lines
    finally:
        s.close()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    port = sys.argv[1]
    if port.upper() in NEVER_PORTS:
        sys.exit(f"{port} is the black deck; this tool is for the yellow deck")
    with open(HEADER, encoding="utf-8") as f:
        c = constants(f.read())
    lines = capture(port)
    if not any("[intro] replay" in line for line in lines):
        sys.exit("FAIL: the deck didn't answer INTRO (an older firmware, or the wrong port)")
    keys, skipped = parse(lines)
    if effects_off(skipped):
        print("the welcome is off in this build (LED_EFFECTS 0): each light takes its look as it links")
    print("\n".join(timeline(keys)))
    problems = check(keys, skipped, c)
    for p in problems:
        print("  - " + p)
    print("FAIL" if problems else "PASS")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
