"""Build data/howl.pcm: the phone remote's HOWL on the new deck (S3), from a real wolf recording.
Run: python tools/gen_howl.py <audio> [--start 13] [--end 19.5]   (seconds)

Made for "Wolf howls.ogg" (Wikimedia Commons, public domain): its first seconds are yips, a
steady howl runs from ~13 s. Any audio ffmpeg reads works. data/howl.pcm is git-ignored; without
one the deck synthesises its howl (util/howl), as the CYD always does.

Format: raw signed 16-bit little-endian mono, 48 kHz, short fades, loudest sample at 95 %.
"""
import argparse
import array
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "howl.pcm")
RATE = 48000
START, END = 13.0, 19.5
PEAK = 0.95
FADE_IN, FADE_OUT = 0.05, 0.8        # seconds: no click at the cut, a natural tail


def normalize(pcm, peak):
    """16-bit samples scaled so the loudest reaches `peak` of full scale; silence stays silent."""
    a = array.array("h", pcm)
    top = max((abs(s) for s in a), default=0)
    if top == 0:
        return pcm
    k = 32767 * peak / top
    return array.array("h", (max(-32768, min(32767, int(s * k))) for s in a)).tobytes()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("audio")
    ap.add_argument("--start", type=float, default=START)
    ap.add_argument("--end", type=float, default=END)
    a = ap.parse_args()
    if not os.path.isfile(a.audio):
        sys.exit(f"gen_howl: no such file {a.audio}")
    if a.end <= a.start:
        sys.exit("gen_howl: --end must be after --start")
    length = a.end - a.start
    fades = f"afade=t=in:d={FADE_IN},afade=t=out:st={max(0.0, length - FADE_OUT)}:d={FADE_OUT}"
    try:
        pcm = subprocess.run(["ffmpeg", "-v", "error", "-ss", str(a.start), "-to", str(a.end), "-i", a.audio,
                              "-vn", "-ac", "1", "-ar", str(RATE), "-af", fades, "-f", "s16le", "-"],
                             check=True, capture_output=True).stdout
    except subprocess.CalledProcessError as e:
        sys.exit(f"gen_howl: ffmpeg failed ({e.stderr.decode(errors='ignore').strip()})")
    if len(pcm) < 2:
        sys.exit("gen_howl: no sound (check --start/--end)")
    pcm = normalize(pcm, PEAK)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(pcm)
    print(f"gen_howl: {len(pcm) // 2} samples ({len(pcm) / 2 / RATE:.1f} s at {RATE} Hz) -> {OUT}")


if __name__ == "__main__":
    main()
