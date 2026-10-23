"""Scores the car mic test clips through speech-to-text services (voice agent, part 1).

    python tools/stt_bench.py micrec/<date>

Reads a folder written by tools/micrec_pull.py, sends every clip to each provider whose key is
set (DEEPGRAM_API_KEY, OPENAI_API_KEY, ELEVENLABS_API_KEY), and checks whether the transcript is
actionable: it holds every keyword of its phrase. Prints and writes <folder>/report.md: per
provider, conditions A-D with n/10 actionable, pass (>= 9) and median latency, then the misses.
"""
import json
import os
import statistics
import sys
import time
import unicodedata
import urllib.error
import urllib.request
import uuid

# P1-P10 (docs/superpowers/plans/2026-10-23-voice-agent-1-mic-test.md, "The test set").
KEYWORDS = [
    ["liga", "luz"],
    ["azul", "fraquinho", "dentro"],
    ["apaga", "fora"],
    ["roxo", "50"],
    ["toca", "djavan"],
    ["pula"],
    ["volta", "anterior"],
    ["toca", "tyler"],
    ["toca", "billie"],
    ["aumenta", "volume"],
]
PASS_OK = 9
PER_CONDITION = 10
NAMES = ["Djavan", "Tyler, the Creator", "Billie Eilish"]   # biased in every provider
NUMBER_WORDS = {"cinquenta": "50"}


def normalise(text):
    text = unicodedata.normalize("NFKD", text.lower())
    text = "".join(c for c in text if not unicodedata.combining(c))
    text = "".join(c if c.isalnum() else " " for c in text)
    return [NUMBER_WORDS.get(w, w) for w in text.split()]


def actionable(transcript, keywords):
    words = normalise(transcript)
    return all(any(w.startswith(k) for w in words) for k in keywords)


def score(results):
    out = {}
    for cond in sorted({r["condition"] for r in results}):
        rows = [r for r in results if r["condition"] == cond]
        ok = sum(actionable(r["transcript"], KEYWORDS[r["phrase"]]) for r in rows)
        out[cond] = {
            "ok": ok,
            "n": len(rows),
            "pass": ok >= PASS_OK and len(rows) == PER_CONDITION,
            "median_ms": statistics.median(r["ms"] for r in rows),
        }
    return out


def _post(url, headers, body):
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        raise RuntimeError("%s answered %d: %s" % (url, e.code, e.read()[:200])) from e


def _multipart(fields, wav):
    b = uuid.uuid4().hex
    parts = []
    for k, v in fields:
        parts.append(('--%s\r\nContent-Disposition: form-data; name="%s"\r\n\r\n%s\r\n' % (b, k, v)).encode())
    parts.append(('--%s\r\nContent-Disposition: form-data; name="file"; filename="clip.wav"\r\n'
                  "Content-Type: audio/wav\r\n\r\n" % b).encode() + wav + b"\r\n")
    parts.append(("--%s--\r\n" % b).encode())
    return "multipart/form-data; boundary=" + b, b"".join(parts)


class Deepgram:
    name = "deepgram"

    def __init__(self, key):
        self.key = key

    def transcribe(self, wav):
        q = "model=%s&language=pt-BR&smart_format=true" % os.environ.get("DEEPGRAM_MODEL", "nova-3")
        q += "".join("&keyterm=" + urllib.request.quote(n) for n in NAMES)
        j = _post("https://api.deepgram.com/v1/listen?" + q,
                  {"Authorization": "Token " + self.key, "Content-Type": "audio/wav"}, wav)
        return j["results"]["channels"][0]["alternatives"][0]["transcript"]


class OpenAI:
    name = "openai"

    def __init__(self, key):
        self.key = key

    def transcribe(self, wav):
        kind, body = _multipart([("model", os.environ.get("OPENAI_STT_MODEL", "gpt-4o-transcribe")),
                                 ("language", "pt"),
                                 ("prompt", "Comandos de carro em português: luzes, Spotify. "
                                            "Artistas: " + ", ".join(NAMES) + ".")], wav)
        j = _post("https://api.openai.com/v1/audio/transcriptions",
                  {"Authorization": "Bearer " + self.key, "Content-Type": kind}, body)
        return j["text"]


class ElevenLabs:
    name = "elevenlabs"

    def __init__(self, key):
        self.key = key

    def transcribe(self, wav):
        kind, body = _multipart([("model_id", os.environ.get("ELEVENLABS_STT_MODEL", "scribe_v1")),
                                 ("language_code", "por")], wav)
        j = _post("https://api.elevenlabs.io/v1/speech-to-text",
                  {"xi-api-key": self.key, "Content-Type": kind}, body)
        return j["text"]


def providers(env):
    out = []
    for cls, var in ((Deepgram, "DEEPGRAM_API_KEY"), (OpenAI, "OPENAI_API_KEY"), (ElevenLabs, "ELEVENLABS_API_KEY")):
        if env.get(var):
            out.append(cls(env[var]))
    return out


def run(folder, provs):
    with open(os.path.join(folder, "manifest.json"), encoding="utf-8") as f:
        clips = json.load(f)
    lines = ["# STT bench: %s" % os.path.basename(os.path.normpath(folder)), ""]
    if not provs:
        lines.append("No provider key set (DEEPGRAM_API_KEY, OPENAI_API_KEY, ELEVENLABS_API_KEY).")
    for p in provs:
        results = []
        for c in clips:
            with open(os.path.join(folder, c["file"]), "rb") as f:
                wav = f.read()
            t0 = time.perf_counter()
            try:
                text = p.transcribe(wav)
            except Exception as e:  # one failed call is a miss, not the end of the run
                text = "[error: %s]" % e
            ms = round((time.perf_counter() - t0) * 1000)
            results.append({"condition": c["condition"], "phrase": c["phrase"], "name": c["name"],
                            "transcript": text, "ms": ms})
        lines += ["## %s" % p.name, "", "| cond | actionable | pass (>= 9) | median ms |", "|---|---|---|---|"]
        for cond, s in score(results).items():
            lines.append("| %s | %d/%d | %s | %g |" % (cond, s["ok"], s["n"], "yes" if s["pass"] else "NO", s["median_ms"]))
        misses = [r for r in results if not actionable(r["transcript"], KEYWORDS[r["phrase"]])]
        if misses:
            lines += ["", "Misses:"]
            lines += ['- %s: "%s"' % (r["name"], r["transcript"]) for r in misses]
        lines.append("")
    text = "\n".join(lines)
    with open(os.path.join(folder, "report.md"), "w", encoding="utf-8") as f:
        f.write(text)
    return text


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    print(run(sys.argv[1], providers(os.environ)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
