"""Save what the yellow deck's panel shows as a PNG, over USB:
    python tools/deck_shot.py <port> <out.png>

Sends "SHOT" to the deck's shotTick (src/yellow/ledpanel.cpp) and rebuilds the picture from its rows;
log lines from the deck's tasks land between rows and are skipped. Takes about 30 s, and the deck
doesn't reset (DTR/RTS stay low). Needs pyserial and pillow (tools/requirements.txt).
"""
import re
import sys
import time

ROW = re.compile(r"^R(\d{3}) ([0-9a-f]+)$")
WIDTH, HEIGHT = 320, 240
TIMEOUT_S = 90
NEVER_PORTS = {"COM12"}  # the black deck: this is the yellow deck's tool


def parse_rows(lines):
    """{y: bytes of big-endian RGB565} from the deck's lines; a cut or foreign line is left out."""
    rows = {}
    for line in lines:
        m = ROW.match(line.strip())
        if m and len(m.group(2)) == WIDTH * 4 and int(m.group(1)) < HEIGHT:
            rows[int(m.group(1))] = bytes.fromhex(m.group(2))
    return rows


def missing_rows(rows):
    return [y for y in range(HEIGHT) if y not in rows]


def rgb888(rows):
    """The picture as RGB888 bytes, top row first. Every row must be there."""
    out = bytearray()
    for y in range(HEIGHT):
        row = rows[y]
        for x in range(WIDTH):
            v = row[x * 2] << 8 | row[x * 2 + 1]
            r, g, b = v >> 11, v >> 5 & 63, v & 31
            out += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2))
    return bytes(out)


def read_lines(port):
    import serial

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 1
    s.dtr = False  # opening the port would reset the CYD
    s.rts = False
    s.open()
    try:
        s.write(b"SHOT\n")
        lines, t0 = [], time.time()
        while time.time() - t0 < TIMEOUT_S:
            line = s.readline().decode("ascii", errors="replace")
            if line.startswith("SHOT END"):
                return lines
            lines.append(line)
    finally:
        s.close()
    raise SystemExit("no SHOT END from the deck in %d s: is it running a build with shotTick?" % TIMEOUT_S)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    port, path = sys.argv[1], sys.argv[2]
    if port.upper() in NEVER_PORTS:
        raise SystemExit("%s is the black deck, not this tool's" % port)
    from PIL import Image

    rows = parse_rows(read_lines(port))
    gone = missing_rows(rows)
    if gone:
        raise SystemExit("%d rows cut in transit (first %d): nothing saved, run it again" % (len(gone), gone[0]))
    Image.frombytes("RGB", (WIDTH, HEIGHT), rgb888(rows)).save(path)
    print("saved", path)


if __name__ == "__main__":
    main()
