"""Joins one trip of the OBD deck's drive log (src/obd/dlogup) into a single CSV.

The deck writes each trip as drive/<trip>/p0001.csv, p0002.csv, ... (a minute of rows each, every
part with the same header) plus linkupN.txt (the raw supported-PID bitmaps and 01 A4). Clone or pull
the private drive-log repo, then:

    python tools/dlog_join.py <clone>/drive/20261119-225001            -> prints the joined CSV
    python tools/dlog_join.py <clone>/drive/20261119-225001 -o trip.csv
    python tools/dlog_join.py <clone>/drive --latest -o trip.csv        the newest trip

Rows keep their order (by part number); a part whose header differs is refused, not mixed in.
"""
import argparse
import pathlib
import re
import sys

PART = re.compile(r"^p(\d{4,})\.csv$")


def parts(trip_dir):
    """The trip's part files in part order."""
    found = []
    for p in pathlib.Path(trip_dir).iterdir():
        m = PART.match(p.name)
        if m:
            found.append((int(m.group(1)), p))
    return [p for _, p in sorted(found)]


def join(trip_dir):
    """The joined CSV text: one header, then every part's rows."""
    header = None
    rows = []
    files = parts(trip_dir)
    if not files:
        raise ValueError(f"no parts (p0001.csv ...) in {trip_dir}")
    for f in files:
        lines = f.read_text(encoding="ascii").splitlines()
        if not lines:
            continue
        if header is None:
            header = lines[0]
        elif lines[0] != header:
            raise ValueError(f"{f.name}: a different header ({lines[0]!r})")
        rows.extend(line for line in lines[1:] if line)
    return "\n".join([header] + rows) + "\n"


def latest(drive_dir):
    trips = sorted(p for p in pathlib.Path(drive_dir).iterdir() if p.is_dir() and parts(p))
    if not trips:
        raise ValueError(f"no trips in {drive_dir}")
    return trips[-1]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", help="a trip folder (drive/<trip>), or drive/ with --latest")
    ap.add_argument("--latest", action="store_true", help="path is drive/: join its newest trip")
    ap.add_argument("-o", "--out", help="write here instead of printing")
    a = ap.parse_args(argv)
    try:
        trip = latest(a.path) if a.latest else pathlib.Path(a.path)
        text = join(trip)
    except (OSError, ValueError) as e:
        print(f"dlog_join: {e}", file=sys.stderr)
        return 1
    if a.out:
        pathlib.Path(a.out).write_text(text, encoding="ascii", newline="\n")
        print(f"{trip.name}: {text.count(chr(10)) - 1} rows -> {a.out}", file=sys.stderr)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
