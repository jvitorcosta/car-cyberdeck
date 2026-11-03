"""Boot sound rosters for the S3 (src/util/black/sndroster.h): one file per slot holding all its
clips, so each boot picks one at random (util/soundpick).

  python tools/gen_rosters.py [rosters.txt] [--sounds DIR]

rosters.txt (default data/rosters.txt, git-ignored; see data/rosters.example.txt) has [drive]
(the silver Honda's greeting), [opener] (the montage start) and [finale] (the Honda logo), one WAV
per line, relative to DIR (default greeting-sounds/). Each clip goes through gen_greeting.py's
steps (mono, 16 kHz, trimmed, normalised) and must stay under 4 s. Writes data/drive.snd,
data/opener.snd and data/finale.snd (git-ignored). Standard library only.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_greeting as gg  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA = os.path.join(ROOT, "data")
SLOTS = ["drive", "opener", "finale"]
RATE = gg.RATE
MAX_SECONDS = 4.0
HEAD, ENTRY, NAME, VERSION, MAX_CLIPS = 8, 24, 16, 1, 32


def parse(text):
    """rosters.txt -> {slot: [paths]}."""
    out, slot = {s: [] for s in SLOTS}, None
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split("#")[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            slot = line[1:-1].strip()
            if slot not in out:
                sys.exit("gen_rosters: line %d: unknown section [%s] (%s)" % (n, slot, ", ".join(SLOTS)))
        elif slot is None:
            sys.exit("gen_rosters: line %d: %s comes before any [section]" % (n, line))
        else:
            out[slot].append(line)
    return out


def clip_name(path):
    """The log name: the file's base name without _16k.wav / .wav, at most NAME - 1 characters."""
    base = os.path.basename(path.replace("\\", "/"))
    for ext in ("_16k.wav", ".wav"):
        if base.endswith(ext):
            base = base[:-len(ext)]
            break
    return base[:NAME - 1]


def load(sounds, paths):
    """[(name, float samples at RATE)] from the WAVs, converted as gen_greeting.py does."""
    out = []
    for p in paths:
        full = os.path.join(sounds, p)
        if not os.path.isfile(full):
            sys.exit("gen_rosters: no such file %s" % full)
        x, rate = gg.read_wav(full)
        out.append((clip_name(p), gg.trim_normalize(gg.resample(x, rate))))
    return out


def build(clips):
    """[(name, float samples)] -> the roster file's bytes."""
    if len(clips) > MAX_CLIPS:
        sys.exit("gen_rosters: %d clips, the firmware reads %d" % (len(clips), MAX_CLIPS))
    off = HEAD + ENTRY * len(clips)
    table, body = b"", b""
    for name, x in clips:
        if len(x) > RATE * MAX_SECONDS:
            sys.exit("gen_rosters: %s is %.2f s; keep it under %.0f s" % (name, len(x) / RATE, MAX_SECONDS))
        pcm = struct.pack("<%dh" % len(x), *(max(-32768, min(32767, int(round(v * 32767)))) for v in x))
        table += struct.pack("<II%ds" % NAME, off + len(body), len(pcm), name.encode()[:NAME - 1])
        body += pcm                                  # 2-byte samples: every clip stays 2-aligned
    return b"SNDR" + struct.pack("<HH", VERSION, len(clips)) + table + body


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("rosters", nargs="?", default=os.path.join(DATA, "rosters.txt"))
    ap.add_argument("--sounds", default=os.path.join(ROOT, "greeting-sounds"))
    a = ap.parse_args()
    if not os.path.isfile(a.rosters):
        sys.exit("gen_rosters: no %s (copy data/rosters.example.txt)" % a.rosters)
    rosters = parse(open(a.rosters, encoding="utf-8").read())
    for slot in SLOTS:
        clips = load(a.sounds, rosters[slot])
        out = os.path.join(DATA, slot + ".snd")
        blob = build(clips)
        with open(out, "wb") as f:
            f.write(blob)
        print("gen_rosters: %s  %d clips  %d bytes" % (os.path.relpath(out, ROOT), len(clips), len(blob)))
        for name, x in clips:
            print("  %-15s %.2f s" % (name, len(x) / RATE))


if __name__ == "__main__":
    main()
