"""Pulls the car mic test clips from the recorder firmware (src/micrec_s3.cpp) to the PC.

    python tools/micrec_pull.py <host> <REMOTE_TOKEN> [--out micrec/<YYYY-MM-DD>]

<host> is the address on the deck's screen (172.20.10.10 on the hotspot) or cardeck.local; the PC
has to be on the same network. Writes <name>.wav per clip plus manifest.json (the deck's, with
"file" added). The clips are the owner's voice: micrec/ is git-ignored.
"""
import argparse
import datetime
import http.client
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request


def base_url(host):
    host = host.rstrip("/")
    return host if host.startswith("http") else "http://" + host


def _get(url):
    try:
        with urllib.request.urlopen(url, timeout=20) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        raise RuntimeError("%s answered %d" % (url.split("?")[0], e.code)) from e


# A hotspot hiccup mid-download: cut body, reset connection, timeout.
DROPS = (http.client.IncompleteRead, http.client.RemoteDisconnected, urllib.error.URLError,
         ConnectionError, TimeoutError)
TRIES = 2


def _clip(url):
    for attempt in range(TRIES):
        try:
            return _get(url)
        except DROPS:
            if attempt == TRIES - 1:
                raise


def pull(base, token, out_dir):
    """Every clip is tried twice; one that still fails is skipped (row["error"]) and left out of
    manifest.json, which is always written, so the bench runs on what arrived."""
    k = urllib.parse.quote(token)
    rows = json.loads(_get("%s/manifest.json?k=%s" % (base, k)))
    os.makedirs(out_dir, exist_ok=True)
    for row in rows:
        try:
            data = _clip("%s/clip?i=%d&k=%s" % (base, row["slot"], k))
        except DROPS as e:
            row["error"] = str(e) or type(e).__name__
            print("%s: not pulled (%s)" % (row["name"], row["error"]), file=sys.stderr)
            continue
        row["file"] = row["name"] + ".wav"
        with open(os.path.join(out_dir, row["file"]), "wb") as f:
            f.write(data)
    with open(os.path.join(out_dir, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump([r for r in rows if "file" in r], f, indent=1)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("host")
    ap.add_argument("token")
    ap.add_argument("--out", default=os.path.join("micrec", datetime.date.today().isoformat()))
    a = ap.parse_args()
    rows = pull(base_url(a.host), a.token, a.out)
    got = sum("file" in r for r in rows)
    print("%d of %d clips -> %s" % (got, len(rows), a.out))
    return 0 if got == len(rows) else 1


if __name__ == "__main__":
    sys.exit(main())
