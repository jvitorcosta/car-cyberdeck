#!/usr/bin/env python3
"""One-time Spotify authorization helper (PC side).

Reads SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET from src/config.h, opens the
Spotify consent page in your browser, catches the loopback redirect on
127.0.0.1:8888, exchanges the code for a refresh token, and writes
SPOTIFY_REFRESH_TOKEN back into src/config.h.

Spotify requires the EXACT redirect URI http://127.0.0.1:8888/callback to be
registered in your app dashboard (Settings -> Redirect URIs).

Run:  python tools/spotify_auth.py    (from the repo root)
Then click "Agree" in the browser that opens.

With --agent the same flow asks for the voice agent's wider scopes (playlists)
and writes SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET / SPOTIFY_REFRESH_TOKEN into
agent/.env instead (created from agent/.env.example if missing). In that mode
src/config.h is only read, never written.
"""
import argparse
import base64
import http.server
import json
import re
import sys
import threading
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONFIG = REPO / "src" / "config.h"
REDIRECT = "http://127.0.0.1:8888/callback"
PORT = 8888
SCOPES = "user-read-playback-state user-modify-playback-state user-read-currently-playing"
AGENT_SCOPES = SCOPES + " playlist-read-private playlist-modify-private playlist-modify-public"
AGENT_ENV = REPO / "agent" / ".env"
AGENT_ENV_EXAMPLE = REPO / "agent" / ".env.example"


def read_define(text, name):
    m = re.search(r'#define\s+' + name + r'\s+"([^"]*)"', text)
    return m.group(1) if m else None


def write_env(path, values, example=None):
    """Set KEY=value lines in a .env file, keeping every other line as is.

    Creates the file from `example` (or empty) when missing; appends keys that
    are not present yet.
    """
    if path.exists():
        lines = path.read_text(encoding="utf-8").splitlines()
    elif example is not None and example.exists():
        lines = example.read_text(encoding="utf-8").splitlines()
    else:
        lines = []
    pending = dict(values)
    for i, line in enumerate(lines):
        key = line.split("=", 1)[0].strip()
        if "=" in line and key in pending:
            lines[i] = f"{key}={pending.pop(key)}"
    lines.extend(f"{k}={v}" for k, v in pending.items())
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="One-time Spotify authorization helper.")
    parser.add_argument("--agent", action="store_true",
                        help="authorize the voice agent (playlist scopes) and write agent/.env "
                             "instead of src/config.h")
    args = parser.parse_args()
    scopes = AGENT_SCOPES if args.agent else SCOPES

    if not CONFIG.exists():
        sys.exit(f"config.h not found at {CONFIG} — create it from include/config.example.h first")
    text = CONFIG.read_text(encoding="utf-8")
    cid = read_define(text, "SPOTIFY_CLIENT_ID")
    secret = read_define(text, "SPOTIFY_CLIENT_SECRET")
    if not cid or cid == "your_client_id" or not secret or secret == "your_client_secret":
        sys.exit("Fill SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET in src/config.h first.")

    auth_url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
        "client_id": cid,
        "response_type": "code",
        "redirect_uri": REDIRECT,
        "scope": scopes,
    })

    holder = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            q = urllib.parse.urlparse(self.path)
            params = urllib.parse.parse_qs(q.query)
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            if "code" in params:
                holder["code"] = params["code"][0]
                self.wfile.write("<h2>CarDeck: authorized! You can close this tab.</h2>".encode())
            else:
                self.wfile.write(("<h2>No code received.</h2><pre>" + q.query + "</pre>").encode())

        def log_message(self, *a):
            pass

    httpd = http.server.HTTPServer(("127.0.0.1", PORT), Handler)
    print(f"Opening browser for Spotify consent...\nIf it doesn't open, paste this URL:\n{auth_url}\n")
    webbrowser.open(auth_url)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    print(f"Waiting for the redirect on {REDIRECT} (click Agree in the browser)...")
    import time
    for _ in range(300):  # up to 5 minutes
        if "code" in holder:
            break
        time.sleep(1)
    httpd.shutdown()
    if "code" not in holder:
        sys.exit("Timed out waiting for authorization.")

    # Exchange the code for tokens.
    body = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": holder["code"],
        "redirect_uri": REDIRECT,
    }).encode()
    basic = base64.b64encode(f"{cid}:{secret}".encode()).decode()
    req = urllib.request.Request(
        "https://accounts.spotify.com/api/token", data=body,
        headers={"Authorization": "Basic " + basic,
                 "Content-Type": "application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req) as resp:
        tok = json.load(resp)
    refresh = tok.get("refresh_token")
    if not refresh:
        sys.exit("No refresh_token in response: " + json.dumps(tok))

    if args.agent:
        write_env(AGENT_ENV, {
            "SPOTIFY_CLIENT_ID": cid,
            "SPOTIFY_CLIENT_SECRET": secret,
            "SPOTIFY_REFRESH_TOKEN": refresh,
        }, AGENT_ENV_EXAMPLE)
        print("\nSUCCESS: Spotify credentials written to agent/.env (src/config.h untouched)")
        return

    new_text = re.sub(r'(#define\s+SPOTIFY_REFRESH_TOKEN\s+)"[^"]*"',
                      r'\1"' + refresh + '"', text)
    CONFIG.write_text(new_text, encoding="utf-8")
    print("\nSUCCESS: refresh token written to src/config.h")
    print("You can now flash the firmware.")


if __name__ == "__main__":
    main()
