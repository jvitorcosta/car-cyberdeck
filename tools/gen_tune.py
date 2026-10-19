"""Make a music-box tune for the S3's "Nothing playing" screen from a MIDI file.
Run: python tools/gen_tune.py <song.mid> --name "National Park" [--track N] [--max 90]
                              [--transpose K] [--out data/tunes/<name>.txt]

Keeps the melody: the top note at each moment (drums dropped), or only --track N's notes when
the top line is an accompaniment. Gaps become rests, grace notes merge into the next note, and
the tune is shifted by octaves into the music box's range (unless --transpose). The output is
one tune in util/tunes.h's format; tools/tune_clip.py packs every data/tunes/*.txt into the
build. data/tunes/ is git-ignored: songs you don't own the rights to stay on your PC.
"""
import argparse
import collections
import os
import re
import struct
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
REST = -1
DRUMS = 9                # MIDI channel 10
CHORD_MS = 15            # onsets this close are one chord
GRACE_MS = 30            # shorter notes merge into the next
MAX_NOTE_MS = 10000      # util/tunes MAX_MS
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

Note = collections.namedtuple("Note", "start end pitch track")


def _varlen(data, i):
    n = 0
    while True:
        b = data[i]
        i += 1
        n = (n << 7) | (b & 0x7F)
        if not b & 0x80:
            return n, i


def parse(data):
    """A standard MIDI file -> its notes (ms, drums dropped), sorted by start."""
    if data[:4] != b"MThd":
        raise ValueError("not a MIDI file")
    hlen, _fmt, ntrk, division = struct.unpack(">IHHH", data[4:14])
    if division & 0x8000:
        raise ValueError("SMPTE time division is not supported")
    i = 8 + hlen
    tempos = []          # (tick, us per beat)
    raw = []             # (start tick, end tick, pitch, track)
    for t in range(ntrk):
        if data[i:i + 4] != b"MTrk":
            raise ValueError("bad track chunk")
        (tlen,) = struct.unpack(">I", data[i + 4:i + 8])
        j, end = i + 8, i + 8 + tlen
        i = end
        tick, status, held = 0, 0, {}
        while j < end:
            delta, j = _varlen(data, j)
            tick += delta
            b = data[j]
            if b == 0xFF:                                   # meta
                kind = data[j + 1]
                n, j = _varlen(data, j + 2)
                if kind == 0x51 and n == 3:
                    tempos.append((tick, int.from_bytes(data[j:j + 3], "big")))
                j += n
                continue
            if b in (0xF0, 0xF7):                           # sysex
                n, j = _varlen(data, j + 1)
                j += n
                continue
            if b & 0x80:
                status = b
                j += 1
            kind, ch = status & 0xF0, status & 0x0F
            size = 1 if kind in (0xC0, 0xD0) else 2
            a = data[j]
            v = data[j + 1] if size == 2 else 0
            j += size
            if ch == DRUMS or kind not in (0x80, 0x90):
                continue
            key = (ch, a)
            if kind == 0x90 and v > 0:
                if key in held:                             # re-struck: end the old one
                    raw.append((held.pop(key), tick, a, t))
                held[key] = tick
            elif key in held:
                raw.append((held.pop(key), tick, a, t))
    tempos.sort()
    if not tempos or tempos[0][0] != 0:
        tempos.insert(0, (0, 500000))                       # MIDI's default 120 bpm

    def ms(tick):
        total, last_tick, us = 0.0, 0, tempos[0][1]
        for tt, tus in tempos[1:]:
            if tt >= tick:
                break
            total += (tt - last_tick) * us / division / 1000
            last_tick, us = tt, tus
        return round(total + (tick - last_tick) * us / division / 1000)

    return sorted((Note(ms(s), ms(e), p, t) for s, e, p, t in raw), key=lambda n: (n.start, -n.pitch))


def melody(notes, track=None, max_ms=None):
    """Notes -> [(pitch or REST, ms)]: the top note of each chord, rests for gaps."""
    if track is not None:
        notes = [n for n in notes if n.track == track]
    groups = []
    for n in sorted(notes, key=lambda n: n.start):
        if groups and n.start - groups[-1][0].start <= CHORD_MS:
            groups[-1].append(n)
        else:
            groups.append([n])
    tops = [max(g, key=lambda n: n.pitch)._replace(start=g[0].start) for g in groups]
    # grace notes: give their time to the next note
    kept = []
    for k, n in enumerate(tops):
        nxt = tops[k + 1].start if k + 1 < len(tops) else n.end
        if k + 1 < len(tops) and nxt - n.start < GRACE_MS:
            tops[k + 1] = tops[k + 1]._replace(start=n.start)
            continue
        kept.append(n)
    out = []
    for k, n in enumerate(kept):
        if k + 1 < len(kept):
            gap = kept[k + 1].start - n.start
            sound = min(n.end - n.start, gap)
            if gap - sound >= GRACE_MS:
                out += [(n.pitch, sound), (REST, gap - sound)]
            else:
                out.append((n.pitch, gap))
        else:
            out.append((n.pitch, n.end - n.start))
    if max_ms is not None:
        cut, total = [], 0
        for p, d in out:
            if total + d >= max_ms:
                cut.append((p, max_ms - total))
                break
            cut.append((p, d))
            total += d
        out = cut
    return [(p, min(max(d, 1), MAX_NOTE_MS)) for p, d in out if d > 0]


def fit_octaves(tune, lo=69, hi=81):
    """Shift by whole octaves so the median note lands in [lo, hi) (a music box sits high)."""
    pitches = sorted(p for p, _ in tune if p != REST)
    if not pitches:
        return tune
    med, k = pitches[len(pitches) // 2], 0
    while med + k < lo:
        k += 12
    while med + k >= hi:
        k -= 12
    return [(p if p == REST else p + k, d) for p, d in tune]


def note_name(p):
    return f"{NAMES[p % 12]}{p // 12 - 1}"


def to_text(name, tune, source):
    words = [("R" if p == REST else note_name(p)) + f" {d}" for p, d in tune]
    lines = [" ".join(words[i:i + 8]) for i in range(0, len(words), 8)]
    return f"# from {source} (tools/gen_tune.py)\ntune {name}\n" + "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("midi")
    ap.add_argument("--name", required=True)
    ap.add_argument("--track", type=int, help="only this track's notes (0 = the first)")
    ap.add_argument("--max", type=float, default=90, help="seconds kept (default 90)")
    ap.add_argument("--transpose", type=int, help="semitones, instead of the octave fit")
    ap.add_argument("--out", help="default: data/tunes/<name>.txt")
    a = ap.parse_args()
    with open(a.midi, "rb") as f:
        notes = parse(f.read())
    tune = melody(notes, a.track, int(a.max * 1000))
    if not tune:
        sys.exit("no notes found (try another --track)")
    if a.transpose is None:
        tune = fit_octaves(tune)
    else:
        tune = [(p if p == REST else p + a.transpose, d) for p, d in tune]
    tune = [(p, d) for p, d in tune if p == REST or 12 <= p <= 119]   # octaves 0-8
    slug = re.sub(r"[^a-z0-9]+", "_", a.name.lower()).strip("_")
    out = a.out or os.path.join(ROOT, "data", "tunes", slug + ".txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", newline="\n") as f:
        f.write(to_text(a.name, tune, os.path.basename(a.midi)))
    secs = sum(d for _, d in tune) / 1000
    print(f"{out}: {len(tune)} notes, {secs:.0f} s; tracks in the file: {sorted({n.track for n in notes})}")


if __name__ == "__main__":
    main()
