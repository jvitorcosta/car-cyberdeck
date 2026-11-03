"""Boot sound rosters for the S3 (src/util/black/sndroster.h): one file per slot holding all its
clips, so each boot picks one at random (util/soundpick).

  python tools/gen_rosters.py [rosters.txt] [--sounds DIR]

rosters.txt (default data/rosters.txt, git-ignored; see data/rosters.example.txt) has [drive]
(the silver Honda's greeting), [opener] (the montage start) and [finale] (the Honda logo), one WAV
per line, relative to DIR (default greeting-sounds/). Each clip is made mono 16 kHz (gen_greeting.py),
trimmed only of real silence (-45 dB), faded out over 20 ms and levelled to today's clips' loudness
(RMS, peaks capped at 95 %), and must stay under 4 s. Writes data/drive.snd,
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
MIN_SECONDS = 0.2            # shorter is a click (an empty or mis-cut WAV), not a greeting
HEAD, ENTRY, NAME, VERSION, MAX_CLIPS = 8, 32, 24, 1, 32
SILENCE = 10 ** (-45 / 20)   # trim only below -45 dB of the peak: keeps a word's natural decay
FADE = RATE // 50            # 20 ms fade-out: no click where the trim cuts the tail
LOUDNESS = 0.12              # RMS of today's clips (ETC, opener, finale): a dense clip at 95 % peak rasped


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


def rms(x):
    return (sum(v * v for v in x) / len(x)) ** 0.5 if x else 0.0


def trim(x):
    """Drop the leading and trailing samples quieter than SILENCE of the peak."""
    peak = max((abs(v) for v in x), default=0.0)
    if peak == 0:
        sys.exit("gen_rosters: a clip is silent")
    loud = [i for i, v in enumerate(x) if abs(v) >= peak * SILENCE]
    return x[loud[0]:loud[-1] + 1]


def fade(x):
    """Ramp the last FADE samples down to 0."""
    n = min(FADE, len(x))
    return x[:len(x) - n] + [v * (n - 1 - k) / max(n - 1, 1) for k, v in enumerate(x[len(x) - n:])]


def level(x):
    """Scale to LOUDNESS RMS, or less when that would push a peak past gen_greeting's PEAK."""
    peak = max((abs(v) for v in x), default=0.0)
    gain = min(LOUDNESS / rms(x), gg.PEAK / peak)
    return [v * gain for v in x]


def load(sounds, paths):
    """[(name, float samples at RATE)] from the WAVs: mono 16 kHz, trimmed, levelled, faded out."""
    out = []
    for p in paths:
        full = os.path.join(sounds, p)
        if not os.path.isfile(full):
            sys.exit("gen_rosters: no such file %s" % full)
        x, rate = gg.read_wav(full)
        out.append((clip_name(p), fade(level(trim(gg.resample(x, rate))))))
    return out


def build(clips):
    """[(name, float samples)] -> the roster file's bytes."""
    if len(clips) > MAX_CLIPS:
        sys.exit("gen_rosters: %d clips, the firmware reads %d" % (len(clips), MAX_CLIPS))
    names = [name[:NAME - 1] for name, _ in clips]
    for name in names:
        if names.count(name) > 1:
            sys.exit("gen_rosters: two clips named %s (the log and SOUND_FORCE couldn't tell them apart)" % name)
    off = HEAD + ENTRY * len(clips)
    table, body = b"", b""
    for name, x in clips:
        if len(x) > RATE * MAX_SECONDS:
            sys.exit("gen_rosters: %s is %.2f s; keep it under %.0f s" % (name, len(x) / RATE, MAX_SECONDS))
        if len(x) < RATE * MIN_SECONDS:
            sys.exit("gen_rosters: %s is %.2f s; a clip needs at least %.1f s" % (name, len(x) / RATE, MIN_SECONDS))
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
