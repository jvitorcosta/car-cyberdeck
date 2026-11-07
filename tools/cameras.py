#!/usr/bin/env python3
"""Moves the Manaus radar list onto the black deck, and the car's marks off it.

The list itself lives in https://github.com/jvitorcosta/open-radars-manaus (built, reviewed and
released there as radars.csv); this tool only does the device work:

  cameras.py push [--release VERSION | --file PATH] [--force]
                                     downloads the released list (latest by default) or reads a local
                                     file, validates it like the deck does, then POST /cam/list
                                     (a list with no rows only with --force)
  cameras.py import-car [--out build/car-marks.json] [--clear]
                                     the points marked in the car (GET /cam/marks) -> an items JSON for
                                     the data repo's review map; with --clear: forget the deck's marks

The marks file never leaves this PC: build/ is git-ignored, and it holds where the car has been.
Next step for it, in an open-radars-manaus checkout:
  python tools/radars.py place --from file --file <path>

The deck is cardeck.local (or --host, e.g. the hotspot's 172.20.10.10); the key is REMOTE_TOKEN from
src/config.h (git-ignored, never printed). `validate` applies the rules of src/util/car/camlist.cpp.
"""
import argparse
import http.client
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG_H = os.path.join(ROOT, "src", "config.h")
MARKS_JSON = os.path.join(ROOT, "build", "car-marks.json")

RELEASES_URL = "https://github.com/jvitorcosta/open-radars-manaus/releases"
USER_AGENT = "car-cyberdeck-cameras/1.0 (+https://github.com/jvitorcosta/car-cyberdeck)"

HEADER = "id,kind,lat,lon,heading,limit,label,source,checked"
MAX_ROWS = 96       # camlist::MAX_POINTS
# camlist::MAX_CSV: the "nvs" partition (20 KB) must also hold the old list while the new one is
# written, OTA's flags and the WiFi keys, so the list stays under a third of it
MAX_BYTES = 6144
MAX_LABEL = 27      # the deck cuts a label here (char label[28])
MAX_ID = 11         # char id[12]
KINDS = ("speed", "bump", "redlight")


# --- validation (same rules as camlist::parse) -------------------------------------------------


def _degrees_ok(s, max_deg):
    # re.A: ASCII digits only, like the deck's parser (Unicode digits match \d, and float() takes them)
    return re.fullmatch(r"[+-]?(\d+(\.\d*)?|\.\d+)", s, re.A) is not None and abs(float(s)) <= max_deg


def _uint_ok(s, lo, hi):
    """ASCII digits only (str.isdigit() takes "²" and "٦٠", which the deck refuses), lo..hi."""
    return re.fullmatch(r"\d+", s, re.A) is not None and lo <= int(s) <= hi


def validate(text):
    """Returns (errors, warnings); errors mean the deck would refuse the list."""
    errors, warnings = [], []
    if len(text.encode("utf-8")) > MAX_BYTES:
        errors.append("list is %d bytes, the deck takes at most %d" % (len(text.encode("utf-8")), MAX_BYTES))
    lines = text.lstrip("﻿").split("\n")
    while len(lines) > 1 and lines[-1].strip("\r") == "":
        lines.pop()
    if lines[0].rstrip("\r") != HEADER:
        errors.append("line 1: header must be exactly " + HEADER)
        return errors, warnings
    n = 0
    for no, line in enumerate(lines[1:], start=2):
        f = line.rstrip("\r").split(",")
        if len(f) != 9:
            errors.append("line %d: %d columns, need 9 (a comma in a label is not allowed)" % (no, len(f)))
            continue
        n += 1
        pid, kind, lat, lon, head, limit, label = f[:7]
        if kind not in KINDS:
            errors.append("line %d: kind must be speed, bump or redlight" % no)
        if not _degrees_ok(lat, 90):
            errors.append("line %d: lat out of range" % no)
        if not _degrees_ok(lon, 180):
            errors.append("line %d: lon out of range" % no)
        if not _uint_ok(head, 0, 359):
            errors.append("line %d: heading is required, 0..359 (a camera without a direction would alert both lanes)" % no)
        if limit != "":
            if kind == "redlight":
                errors.append("line %d: a redlight has no limit" % no)
            elif not _uint_ok(limit, 1, 199):
                errors.append("line %d: limit must be empty or 1..199" % no)
        if '"' in line:   # no CSV quoting here: other tools would read a quote as one and re-split the row
            errors.append('line %d: a " is not allowed (like commas, quotes stay out of labels)' % no)
        if len(pid.encode("utf-8")) > MAX_ID:
            errors.append("line %d: id longer than %d bytes" % (no, MAX_ID))
        if len(label.encode("utf-8")) > MAX_LABEL:
            warnings.append("line %d: label is %d bytes, the deck shows the first %d" % (no, len(label.encode("utf-8")), MAX_LABEL))
    if n > MAX_ROWS:
        errors.append("%d rows, the deck takes at most %d" % (n, MAX_ROWS))
    return errors, warnings


# --- marks from the car --------------------------------------------------------------------------


def parse_marks(text):
    """GET /cam/marks rows 'time,lat,lon,course,kmh' (time: UTC seconds at the mark, 0 when the deck's
    clock wasn't set)."""
    items = []
    for n, line in enumerate(text.splitlines(), start=1):
        f = line.strip().split(",")
        if len(f) != 5:
            continue
        items.append({"kind": "speed", "address": "", "sentido": "", "heading": int(float(f[3])) % 360,
                      "label": "Mark %d" % n, "limit": None, "source": "car", "lat": float(f[1]), "lon": float(f[2])})
    return items


# --- the deck ------------------------------------------------------------------------------------


def read_token():
    try:
        m = re.search(r'^\s*#define\s+REMOTE_TOKEN\s+"([^"]*)"', open(CONFIG_H, encoding="utf-8").read(), re.M)
    except OSError:
        m = None
    if not m or not m.group(1):
        sys.exit("cameras: no REMOTE_TOKEN in src/config.h")
    return m.group(1)


def deck_call(host, method, path, body=None):
    url = "http://%s%s?k=%s" % (host, path, urllib.parse.quote(read_token(), safe=""))
    headers = {"Content-Type": "text/plain"} if body is not None else {}
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:   # never echo the URL: it carries the key
        return e.code, e.read().decode("utf-8", "replace")
    except (urllib.error.URLError, OSError) as e:
        sys.exit("cameras: cannot reach the deck at %s (%s)" % (host, getattr(e, "reason", e)))


# --- the released list ---------------------------------------------------------------------------


def release_url(version):
    """radars.csv of the latest release of open-radars-manaus, or of the tag `version`."""
    if version is None:
        return RELEASES_URL + "/latest/download/radars.csv"
    return "%s/download/%s/radars.csv" % (RELEASES_URL, urllib.parse.quote(version, safe=""))


def download(url, timeout=30):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read().decode("utf-8")


# --- commands ------------------------------------------------------------------------------------


def cmd_push(a):
    if a.file:
        with open(a.file, encoding="utf-8", newline="") as fh:
            text = fh.read()
    else:
        url = release_url(a.release)
        try:
            text = download(url)
        except UnicodeDecodeError:
            print("cameras: could not download %s: the release file is not UTF-8 text" % url, file=sys.stderr)
            return 2
        except (urllib.error.URLError, http.client.HTTPException, OSError) as e:
            print("cameras: could not download %s (%s): no such release, or no network" % (
                url, getattr(e, "reason", e)), file=sys.stderr)
            return 2
    errs, warns = validate(text)
    for w in warns:
        print("warning: " + w)
    if errs:
        for e in errs:
            print("error: " + e, file=sys.stderr)
        return 1
    rows = sum(1 for line in text.lstrip("﻿").split("\n")[1:] if line.strip("\r"))
    if rows == 0 and not a.force:
        # The deck can't say how many it holds now: an empty file is almost always a slip.
        print("error: the list has no rows; the deck would drop every camera it has (--force pushes it anyway)",
              file=sys.stderr)
        return 1
    status, body = deck_call(a.host, "POST", "/cam/list", text.encode("utf-8"))
    if status == 204:
        print("pushed %d rows (%d bytes) to %s" % (rows, len(text.encode("utf-8")), a.host))
        return 0
    print("deck refused the list: %d %s" % (status, body.strip()), file=sys.stderr)
    return 1


def cmd_import_car(a):
    if a.clear:
        status, _ = deck_call(a.host, "POST", "/cam/marks/clear")
        print("deck marks cleared" if status == 204 else "could not clear the deck's marks (%d)" % status)
        return 0 if status == 204 else 1
    status, body = deck_call(a.host, "GET", "/cam/marks")
    if status != 200:
        print("deck answered %d" % status, file=sys.stderr)
        return 1
    items = parse_marks(body)
    if not items:
        print("no marks on the deck")
        return 0
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(items, fh, ensure_ascii=False, indent=1)
    print("wrote %d marks to %s" % (len(items), a.out))
    print("next, in an open-radars-manaus checkout: python tools/radars.py place --from file --file %s" % a.out)
    print("then, once they are placed there: python tools/cameras.py import-car --clear")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--host", default=os.environ.get("CARDECK_HOST", "cardeck.local"), help="black deck address")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("push")
    g = p.add_mutually_exclusive_group()
    g.add_argument("--release", metavar="VERSION", help="release tag of open-radars-manaus (default: the latest)")
    g.add_argument("--file", metavar="PATH", help="push a local radars.csv instead of downloading one")
    p.add_argument("--force", action="store_true", help="push even a list with no rows")
    p.set_defaults(fn=cmd_push)
    p = sub.add_parser("import-car")
    p.add_argument("--out", default=MARKS_JSON, metavar="PATH", help="items JSON to write (default: build/car-marks.json)")
    p.add_argument("--clear", action="store_true", help="forget the marks on the deck instead of reading them")
    p.set_defaults(fn=cmd_import_car)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
