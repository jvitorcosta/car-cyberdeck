#!/usr/bin/env python3
"""Builds and maintains the Manaus camera list (data/cameras.csv) and pushes it to the black deck.

  cameras.py fetch                   IMMU page + OpenStreetMap vs the list: what is new / gone (edits nothing)
  cameras.py place [--from immu|osm|marks]   writes build/camera_place.html (drag each pin onto its road,
                                     set the arrow, limit and label, press "save" -> placed.json)
  cameras.py place --apply placed.json       merges placed.json into data/cameras.csv (new mao-NNNN ids)
  cameras.py push                    validates the list like the deck does, then POST /cam/list
  cameras.py import-car [--apply placed.json]  the points marked in the car (GET /cam/marks) -> place page;
                                     with --apply: merge, then clear the deck's marks
  cameras.py check                   fetch; exit 1 when anything is new or gone (for a monthly routine)

Every point is placed by hand once: IMMU gives addresses only, OSM nodes have no direction, and a
geocoded guess never reaches the deck unconfirmed. The deck is cardeck.local (or --host, e.g. the
hotspot's 172.20.10.10); the key is REMOTE_TOKEN from src/config.h (git-ignored, never printed).
`validate` applies the rules of src/util/car/camlist.cpp.
"""
import argparse
import csv
import datetime
import html.parser
import json
import math
import os
import re
import sys
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "data", "cameras.csv")
PLACE_HTML = os.path.join(ROOT, "build", "camera_place.html")
TEMPLATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "camera_place.html")
CONFIG_H = os.path.join(ROOT, "src", "config.h")

IMMU_URL = "https://immu.manaus.am.gov.br/index.php?r=site%2Finforadar"
OVERPASS_URLS = ("https://overpass-api.de/api/interpreter", "https://overpass.kumi.systems/api/interpreter")
NOMINATIM_URL = "https://nominatim.openstreetmap.org/search"
BBOX = "-3.25,-60.15,-2.90,-59.80"  # south,west,north,east
USER_AGENT = "spotify-pokemon-deck-cameras/1.0 (personal car-deck tool; joaovpro17@gmail.com)"

HEADER = "id,kind,lat,lon,heading,limit,label,source,checked"
MAX_ROWS = 96       # camlist::MAX_POINTS
MAX_BYTES = 8192    # camlist::MAX_CSV
MAX_LABEL = 27      # the deck cuts a label here (char label[28])
MAX_ID = 11         # char id[12]
KINDS = ("speed", "bump", "redlight")
MATCH_M = 50.0

# --- validation (same rules as camlist::parse) -------------------------------------------------


def _degrees_ok(s, max_deg):
    return re.fullmatch(r"[+-]?(\d+(\.\d*)?|\.\d+)", s) is not None and abs(float(s)) <= max_deg


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
        if not (head.isdigit() and int(head) <= 359):
            errors.append("line %d: heading is required, 0..359 (a camera without a direction would alert both lanes)" % no)
        if limit != "":
            if kind == "redlight":
                errors.append("line %d: a redlight has no limit" % no)
            elif not (limit.isdigit() and 1 <= int(limit) <= 199):
                errors.append("line %d: limit must be empty or 1..199" % no)
        if len(pid.encode("utf-8")) > MAX_ID:
            errors.append("line %d: id longer than %d bytes" % (no, MAX_ID))
        if len(label.encode("utf-8")) > MAX_LABEL:
            warnings.append("line %d: label is %d bytes, the deck shows the first %d" % (no, len(label.encode("utf-8")), MAX_LABEL))
    if n > MAX_ROWS:
        errors.append("%d rows, the deck takes at most %d" % (n, MAX_ROWS))
    return errors, warnings


# --- the list on disk ----------------------------------------------------------------------------


def read_rows(path=CSV_PATH):
    if not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8", newline="") as fh:
        rows = list(csv.DictReader(fh))
    return [{"id": r["id"], "kind": r["kind"], "lat": float(r["lat"]), "lon": float(r["lon"]),
             "heading": int(r["heading"]), "limit": int(r["limit"]) if r["limit"] else None,
             "label": r["label"], "source": r["source"], "checked": r["checked"]} for r in rows]


def rows_text(rows):
    lines = [HEADER]
    for r in sorted(rows, key=lambda r: r["id"]):
        lines.append(",".join([r["id"], r["kind"], "%.6f" % r["lat"], "%.6f" % r["lon"], str(r["heading"]),
                               "" if r["limit"] is None else str(r["limit"]), r["label"], r["source"], r["checked"]]))
    return "\n".join(lines) + "\n"


def apply_placed(path, placed, today=None):
    """Merges placed items into the CSV with the next free mao-NNNN ids. Refuses (ValueError, file
    untouched) when the result would not pass validate()."""
    today = today or datetime.date.today().isoformat()
    rows = read_rows(path)
    used = [int(m.group(1)) for r in rows for m in [re.fullmatch(r"mao-(\d+)", r["id"])] if m]
    nxt = 1 + max(used or [0])
    for it in placed:
        label = re.sub(r"\s+", " ", str(it.get("label", "")).replace(",", " ")).strip()
        rows.append({"id": "mao-%04d" % nxt, "kind": it["kind"], "lat": float(it["lat"]), "lon": float(it["lon"]),
                     "heading": "" if it.get("heading") is None else int(it["heading"]),
                     "limit": it.get("limit") if it["kind"] != "redlight" else None,
                     "label": label, "source": it.get("source", "car"), "checked": today})
        nxt += 1
    text = rows_text(rows)
    errs, _ = validate(text)
    if errs:
        raise ValueError("; ".join(errs))
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)
    return len(placed)


# --- matching ------------------------------------------------------------------------------------

_STOP = {"av", "avenida", "rua", "r", "de", "da", "do", "das", "dos", "e", "em", "frente", "n", "no", "a", "o",
         "prof", "professor", "cel", "coronel"}


def haversine_m(la1, lo1, la2, lo2):
    p1, p2 = math.radians(la1), math.radians(la2)
    a = math.sin((p2 - p1) / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(math.radians(lo2 - lo1) / 2) ** 2
    return 2 * 6371000.0 * math.asin(math.sqrt(a))


def _plain(s):
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode().lower()


def _words(label):
    return {w for w in re.findall(r"[a-z0-9]+", _plain(label)) if w not in _STOP}


def matches(item, row):
    """Same camera? Within 50 m when both have coordinates, else by the words of the label (numbers
    and a known direction must agree: 'Seripa 7' is not 'Seripa 8')."""
    if item.get("kind") != row.get("kind"):
        return False
    if item.get("lat") is not None and row.get("lat") is not None:
        if haversine_m(item["lat"], item["lon"], row["lat"], row["lon"]) <= MATCH_M:
            return True
        if item.get("source") == "osm":      # an OSM node has no words worth comparing
            return False
    a, b = _words(item.get("label", "")), _words(row.get("label", ""))
    if not a or not b:
        return False
    if {w for w in a if w.isdigit()} != {w for w in b if w.isdigit()}:
        return False
    if item.get("heading") is not None and row.get("heading") is not None and item["heading"] != row["heading"]:
        return False
    shared = len(a & b)
    return shared >= 2 and shared / min(len(a), len(b)) >= 0.6


def diff(items, rows):
    """(new items not in the list, list rows no longer reported). Car-sourced rows are never 'gone'."""
    new = [i for i in items if not any(matches(i, r) for r in rows)]
    gone = [r for r in rows if r["source"] != "car" and not any(matches(i, r) for i in items)]
    return new, gone


# --- sources -------------------------------------------------------------------------------------

_SENTIDO = {"norte": 0, "leste": 90, "sul": 180, "oeste": 270}
_KIND_BY_HEADING = (("radares de velocidade", "speed"), ("lombadas", "bump"), ("radares de avanco", "redlight"))


class _ImmuParser(html.parser.HTMLParser):
    """Collects the <li> rows of the <ol> that follows each heading (found by its text, in a <b>)."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.kind = None
        self.in_b = self.in_li = False
        self.buf = []
        self.items = []

    def handle_starttag(self, tag, attrs):
        if tag == "b":
            self.in_b, self.buf = True, []
        elif tag == "li" and self.kind:
            self.in_li, self.buf = True, []

    def handle_endtag(self, tag):
        if tag == "b" and self.in_b:
            self.in_b = False
            text = _plain(" ".join("".join(self.buf).split()))
            for key, kind in _KIND_BY_HEADING:
                if text.startswith(key):
                    self.kind = kind
        elif tag == "li" and self.in_li:
            self.in_li = False
            text = " ".join("".join(self.buf).split())
            if text:
                self.items.append((self.kind, text))
        elif tag == "ol":
            self.kind = None

    def handle_data(self, data):
        if self.in_b or self.in_li:
            self.buf.append(data)


def _abbrev(s):
    for a, b in (("Avenida ", "Av. "), ("Professor ", "Prof. "), ("Coronel ", "Cel. "), ("Jornalista ", "Jorn. ")):
        s = s.replace(a, b)
    return s


def immu_label(address):
    """'Avenida Professor Nilton Lins, nº 900, Jardim Via Veneto – Flores, sentido Leste' -> 'Av. Prof. Nilton Lins 900 · Flores'."""
    addr = re.sub(r",?\s*sentido\s+.*$", "", address.strip().rstrip("."), flags=re.I)
    parts = [p.strip() for p in re.split(r"\s+[–—-]\s+", addr)]
    street, num = parts[0], ""
    m = re.search(r",\s*n[ºo°]\s*([\d.]+)", street)
    if m:
        num = " " + m.group(1).replace(".", "")
    street = street.split(",")[0]
    district = parts[-1].split(",")[0] if len(parts) > 1 else ""
    label = _abbrev(street) + num + (" · " + district if district else "")
    return re.sub(r"\s+", " ", label.replace(",", " ")).strip()


def sentido_of(address):
    m = re.search(r"sentido\s+(.+?)\.?$", address.strip(), flags=re.I)
    return m.group(1).strip() if m else ""


def heading_of(sentido):
    return _SENTIDO.get(_plain(sentido).strip())


def parse_immu(page):
    """The three lists of the IMMU page as items {kind, address, sentido, heading, label, limit, source}."""
    p = _ImmuParser()
    p.feed(page)
    items = []
    for kind, text in p.items:
        sentido = sentido_of(text)
        items.append({"kind": kind, "address": text, "sentido": sentido, "heading": heading_of(sentido),
                      "label": immu_label(text), "limit": None, "source": "immu", "lat": None, "lon": None})
    seen = {}
    for it in items:   # two cameras on one street and district would share a label: tell them apart
        seen.setdefault(it["label"], []).append(it)
    for group in seen.values():
        if len(group) > 1:
            for it in group:
                it["label"] += " " + (it["sentido"].split()[0] if it["sentido"] else "?")
    return items


# The 7 "Levantamentos Técnicos" on the IMMU page (Av. do Turismo); the limit (60) is from their PDFs.
_SURVEY = (
    ("Av. do Turismo, oposto a empresa Vida Film nº 12568 - Tarumã-açu, sentido oeste", "Av. do Turismo · Vida Film"),
    ("Av. do Turismo/Av. Thales Loureiro - Ponta Negra, sentido complexo turístico Ponta Negra", "Av. do Turismo · Thales Loureiro"),
    ("Av. do Turismo, em frente a 123ª IEADAM - Tarumã, sentido norte", "Av. do Turismo · IEADAM"),
    ("Av. do Turismo, 3727 - Ponta Negra, sentido complexo turístico Ponta Negra", "Av. do Turismo 3727"),
    ("Av. do Turismo, oposto ao Cond Viver Tarumã - Tarumã-Açu, sentido oeste", "Av. do Turismo · Viver Tarumã"),
    ("Av. do Turismo, oposto Cond. Mediterrâneo I - Tarumã, sentido aeroporto Eduardo Gomes", "Av. do Turismo · Mediterrâneo"),
    ("Av. do Turismo, em frente ao Cond Viva Vida Tarumã - Tarumã, sentido leste", "Av. do Turismo · Viva Vida"),
)


def survey_items():
    out = []
    for address, label in _SURVEY:
        s = sentido_of(address)
        out.append({"kind": "speed", "address": address, "sentido": s, "heading": heading_of(s), "label": label,
                    "limit": 60, "source": "immu-survey", "lat": None, "lon": None})
    return out


def parse_overpass(data):
    items = []
    for el in data.get("elements", []):
        if el.get("type") != "node":
            continue
        tags = el.get("tags", {})
        ms = re.match(r"\d+", tags.get("maxspeed", ""))
        d = tags.get("direction", "")
        items.append({"kind": "speed", "address": "", "sentido": "", "heading": int(d) % 360 if d.isdigit() else None,
                      "label": tags.get("name") or "OSM %d" % el["id"], "limit": int(ms.group(0)) if ms else None,
                      "source": "osm", "lat": float(el["lat"]), "lon": float(el["lon"])})
    return items


def parse_marks(text):
    """GET /cam/marks rows 'ms,lat,lon,course,kmh' (ms is millis(), meaningless across boots)."""
    items = []
    for n, line in enumerate(text.splitlines(), start=1):
        f = line.strip().split(",")
        if len(f) != 5:
            continue
        items.append({"kind": "speed", "address": "", "sentido": "", "heading": int(float(f[3])) % 360,
                      "label": "Mark %d" % n, "limit": None, "source": "car", "lat": float(f[1]), "lon": float(f[2])})
    return items


# --- network -------------------------------------------------------------------------------------


def http(url, data=None, headers=None, timeout=30):
    h = {"User-Agent": USER_AGENT}
    h.update(headers or {})
    req = urllib.request.Request(url, data=data, headers=h)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, r.read()


def fetch_immu():
    _, body = http(IMMU_URL)
    return parse_immu(body.decode("utf-8", "replace")) + survey_items()


def fetch_osm():
    q = '[out:json][timeout:25];node["highway"="speed_camera"](%s);out;' % BBOX
    last = None
    for attempt in range(2):    # the public instances answer 502 or stall now and then: retry, try the mirror
        for url in OVERPASS_URLS:
            try:
                _, body = http(url, data=urllib.parse.urlencode({"data": q}).encode(), timeout=70)
                return parse_overpass(json.loads(body))
            except (urllib.error.URLError, OSError, ValueError) as e:
                last = e
        time.sleep(2)
    raise last


_last_geocode = [0.0]


def geocode(item):
    """First guess for an address-only item (Nominatim: at most 1 request/s). None when not found."""
    wait = 1.1 - (time.time() - _last_geocode[0])
    if wait > 0:
        time.sleep(wait)
    _last_geocode[0] = time.time()
    addr = re.split(r",?\s*sentido\s", item["address"], flags=re.I)[0]
    parts = [p.strip() for p in re.split(r"\s+[–—-]\s+", addr)]
    street = parts[0].split(",")[0].split("/")[0]
    district = parts[-1].split(",")[0] if len(parts) > 1 else ""
    q = "%s, %s, Manaus, Amazonas, Brazil" % (street, district) if district else "%s, Manaus, Amazonas, Brazil" % street
    url = NOMINATIM_URL + "?" + urllib.parse.urlencode({"q": q, "format": "json", "limit": 1, "countrycodes": "br"})
    try:
        _, body = http(url)
        res = json.loads(body)
        if res:
            return float(res[0]["lat"]), float(res[0]["lon"])
    except (urllib.error.URLError, ValueError, OSError):
        pass
    return None


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


# --- the placing page ----------------------------------------------------------------------------


def write_place_page(items, out=PLACE_HTML):
    template = open(TEMPLATE, encoding="utf-8").read()
    data = json.dumps(items, ensure_ascii=False).replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(template.replace("__ITEMS__", data))
    return out


# --- commands ------------------------------------------------------------------------------------


def _show(it):
    where = "" if it.get("lat") is None else "  (%.5f, %.5f)" % (it["lat"], it["lon"])
    lim = "" if it.get("limit") is None else " %s km/h" % it["limit"]
    d = it.get("sentido") or ("" if it.get("heading") is None else "%d deg" % it["heading"])
    return "  [%s%s] %s -> %s%s" % (it["kind"], lim, it["label"], d or "?", where)


def collect():
    items, problems = [], []
    for name, fn in (("IMMU", fetch_immu), ("OpenStreetMap", fetch_osm)):
        try:
            items += fn()
        except (urllib.error.URLError, OSError, ValueError) as e:
            problems.append("%s: %s" % (name, getattr(e, "reason", e)))
    return items, problems


def cmd_fetch(_a):
    items, problems = collect()
    for p in problems:
        print("cameras: could not read " + p, file=sys.stderr)
    if problems:
        return 2
    rows = read_rows()
    new, gone = diff(items, rows)
    counts = [sum(1 for i in items if i["kind"] == k) for k in KINDS]
    print("%d points online (%d speed, %d bump, %d redlight); %d in data/cameras.csv" % (len(items), *counts, len(rows)))
    print("new (%d):" % len(new))
    for it in new:
        print(_show(it))
    print("gone (%d):" % len(gone))
    for r in gone:
        print("  %s [%s] %s" % (r["id"], r["kind"], r["label"]))
    unknown = [r for r in rows if r["kind"] != "redlight" and r["limit"] is None]
    if unknown:
        print("limit unknown (%d): %s" % (len(unknown), ", ".join(r["id"] for r in unknown)))
    return 1 if (new or gone) else 0


def cmd_check(a):
    code = cmd_fetch(a)
    print({0: "check: the list is up to date", 1: "check: the list needs attention"}.get(code, "check: failed"))
    return code


def _place(items):
    for it in items:
        if it.get("lat") is None:
            g = geocode(it)
            it["lat"], it["lon"] = g if g else (-3.1019, -60.0250)   # not found: Manaus centre, the owner drags it
            it["guess"] = True
    out = write_place_page(items)
    print("wrote %s (%d points): open it, place each pin, press save, then run with --apply placed.json" % (
        os.path.relpath(out, ROOT), len(items)))


def _load_placed(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def cmd_place(a):
    if a.apply:
        n = apply_placed(CSV_PATH, _load_placed(a.apply))
        print("added %d rows to data/cameras.csv" % n)
        return 0
    if a.src == "marks":
        return cmd_import_car(argparse.Namespace(host=a.host, apply=None))
    try:
        items = fetch_immu() if a.src == "immu" else fetch_osm()
    except (urllib.error.URLError, OSError, ValueError) as e:
        print("cameras: could not read %s: %s" % (a.src, getattr(e, "reason", e)), file=sys.stderr)
        return 2
    new, _ = diff(items, read_rows())
    if not new:
        print("nothing unplaced from %s" % a.src)
        return 0
    _place(new)
    return 0


def cmd_push(a):
    text = open(CSV_PATH, encoding="utf-8", newline="").read()
    errs, warns = validate(text)
    for w in warns:
        print("warning: " + w)
    if errs:
        for e in errs:
            print("error: " + e, file=sys.stderr)
        return 1
    status, body = deck_call(a.host, "POST", "/cam/list", text.encode("utf-8"))
    if status == 204:
        print("pushed %d rows (%d bytes) to %s" % (text.count("\n") - 1, len(text.encode("utf-8")), a.host))
        return 0
    print("deck refused the list: %d %s" % (status, body.strip()), file=sys.stderr)
    return 1


def cmd_import_car(a):
    if a.apply:
        n = apply_placed(CSV_PATH, _load_placed(a.apply))
        print("added %d rows to data/cameras.csv" % n)
        status, _ = deck_call(a.host, "POST", "/cam/marks/clear")
        print("deck marks cleared" if status == 204 else "could not clear the deck's marks (%d)" % status)
        return 0
    status, body = deck_call(a.host, "GET", "/cam/marks")
    if status != 200:
        print("deck answered %d" % status, file=sys.stderr)
        return 1
    items = parse_marks(body)
    if not items:
        print("no marks on the deck")
        return 0
    _place(items)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--host", default=os.environ.get("CARDECK_HOST", "cardeck.local"), help="black deck address")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("fetch").set_defaults(fn=cmd_fetch)
    sub.add_parser("check").set_defaults(fn=cmd_check)
    sub.add_parser("push").set_defaults(fn=cmd_push)
    p = sub.add_parser("place")
    p.add_argument("--from", dest="src", choices=("immu", "osm", "marks"), default="immu")
    p.add_argument("--apply", metavar="placed.json")
    p.set_defaults(fn=cmd_place)
    p = sub.add_parser("import-car")
    p.add_argument("--apply", metavar="placed.json")
    p.set_defaults(fn=cmd_import_car)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
