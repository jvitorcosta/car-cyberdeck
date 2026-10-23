"""Host tests for tools/micrec_pull.py against a fake deck. Run: python tools/test_micrec_pull.py"""
import json
import os
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import micrec_pull as mp  # noqa: E402

TOKEN = "secret"
MANIFEST = [
    {"slot": 0, "name": "A01", "condition": "A", "phrase": 0, "ms": 1500, "gain": 7},
    {"slot": 1, "name": "A02", "condition": "A", "phrase": 1, "ms": 2000, "gain": 7},
]
WAVS = {0: b"RIFF0-fake-wav", 1: b"RIFF1-fake-wav-longer"}


class FakeDeck(BaseHTTPRequestHandler):
    drops = {}   # slot -> how many more requests for it end mid-body

    def log_message(self, *args):
        pass

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if q.get("k") != [TOKEN]:
            self.send_response(403)
            self.end_headers()
            return
        if u.path == "/manifest.json":
            body, kind = json.dumps(MANIFEST).encode(), "application/json"
        elif u.path == "/clip" and int(q["i"][0]) in WAVS:
            body, kind = WAVS[int(q["i"][0])], "audio/wav"
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        slot = int(q["i"][0]) if u.path == "/clip" else -1
        if FakeDeck.drops.get(slot, 0) > 0:
            FakeDeck.drops[slot] -= 1
            self.wfile.write(body[:4])     # the hotspot drops: fewer bytes than promised
            self.close_connection = True
            return
        self.wfile.write(body)


class PullFromFakeDeck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = HTTPServer(("127.0.0.1", 0), FakeDeck)
        cls.base = "http://127.0.0.1:%d" % cls.server.server_port
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def test_writes_wavs_and_manifest(self):
        with tempfile.TemporaryDirectory() as out:
            rows = mp.pull(self.base, TOKEN, out)
            self.assertEqual([r["file"] for r in rows], ["A01.wav", "A02.wav"])
            for slot, name in ((0, "A01"), (1, "A02")):
                with open(os.path.join(out, name + ".wav"), "rb") as f:
                    self.assertEqual(f.read(), WAVS[slot])
            with open(os.path.join(out, "manifest.json"), encoding="utf-8") as f:
                saved = json.load(f)
            self.assertEqual(saved[0]["file"], "A01.wav")
            self.assertEqual(saved[1]["phrase"], 1)

    def test_a_dropped_clip_is_retried_then_skipped_and_the_manifest_still_written(self):
        FakeDeck.drops = {1: 99}   # slot 1 always cuts off mid-body
        try:
            with tempfile.TemporaryDirectory() as out:
                rows = mp.pull(self.base, TOKEN, out)
                self.assertEqual([r.get("file") for r in rows], ["A01.wav", None])
                self.assertIn("error", rows[1])
                self.assertFalse(os.path.exists(os.path.join(out, "A02.wav")))
                with open(os.path.join(out, "manifest.json"), encoding="utf-8") as f:
                    self.assertEqual([r["name"] for r in json.load(f)], ["A01"])
        finally:
            FakeDeck.drops = {}

    def test_a_clip_that_drops_once_is_retried(self):
        FakeDeck.drops = {0: 1}    # slot 0 cuts off on the first try only
        try:
            with tempfile.TemporaryDirectory() as out:
                rows = mp.pull(self.base, TOKEN, out)
                self.assertEqual([r.get("file") for r in rows], ["A01.wav", "A02.wav"])
        finally:
            FakeDeck.drops = {}

    def test_wrong_token_raises(self):
        with tempfile.TemporaryDirectory() as out:
            with self.assertRaises(RuntimeError) as e:
                mp.pull(self.base, "nope", out)
            self.assertIn("403", str(e.exception))

    def test_host_becomes_a_url(self):
        self.assertEqual(mp.base_url("172.20.10.10"), "http://172.20.10.10")
        self.assertEqual(mp.base_url("http://cardeck.local/"), "http://cardeck.local")


if __name__ == "__main__":
    unittest.main()
