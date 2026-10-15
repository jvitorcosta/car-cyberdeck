"""Build data/intro.bin: the new deck (S3)'s boot intro video, from your own clip.
Run: python tools/gen_intro.py <video> [--seg 11.8-99 --seg 0-3]   (seconds, played in the order
given; an end past the clip's end = to the end). Default, for the kiss wallpaper: 11.8 s to the end
(empty wall, her shadow, walk-in, the kiss), then 0-3 s (she turns away, the lipstick print holds).
A looping wallpaper starts mid-loop: its opening is the end of the kiss, then seconds of print alone.
The clip, data/intro.bin and data/intro.pcm are git-ignored. Needs ffmpeg.

Format: the montage's "MTG1" (ui/montage): u16 count, u16 fps, count x (u32 offset, u32 size),
then baseline JPEG frames, 320x240 (a centre 4:3 crop of the clip). Sound: data/intro.pcm,
16-bit mono 48 kHz, each segment cut to its frames' length so the seams stay in sync. Without a file the deck
boots straight to its night drive (tools/intro_clip.py writes an empty one for the build).
"""
import argparse
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "intro.bin")
OUT_PCM = os.path.join(ROOT, "data", "intro.pcm")
FPS = 20
RATE = 48000               # sound sample rate (the S3's ES8311 has a 48 kHz clock row)
MAX_BYTES = 4_000_000      # the S3 has an 8 MB app partition; keep the video under ~4 MB
QUALITY = 2                # ffmpeg -q:v (2 best .. 31 worst); raised automatically to fit
DEFAULT_SEGMENTS = [(11.8, 99.0), (0.0, 3.0)]


def segment(text):
    """'6-99' -> (6.0, 99.0); ValueError unless 0 <= start < end."""
    a, sep, b = text.partition("-")
    if not sep:
        raise ValueError(f"segment {text!r}: want START-END")
    start, end = float(a), float(b)
    if start < 0 or end <= start:
        raise ValueError(f"segment {text!r}: want 0 <= START < END")
    return start, end


def pack(frames, fps):
    """JPEG frames -> MTG1 bytes."""
    head = b"MTG1" + struct.pack("<HH", len(frames), fps)
    off = 8 + 8 * len(frames)
    index = b""
    for f in frames:
        index += struct.pack("<II", off, len(f))
        off += len(f)
    return head + index + b"".join(frames)


def fit(pcm, frames, fps, rate):
    """A segment's 16-bit sound, trimmed or padded with silence to `frames` frames' length."""
    want = 2 * (frames * rate // fps)
    return pcm[:want] + bytes(max(0, want - len(pcm)))


def sound_of(video, segments, counts):
    """The segments' sound, each fitted to its frame count, as one 16-bit mono RATE stream."""
    out = b""
    for (start, end), n in zip(segments, counts):
        pcm = subprocess.run(["ffmpeg", "-v", "error", "-ss", str(start), "-to", str(end), "-i", video,
                              "-vn", "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"],
                             check=True, capture_output=True).stdout
        out += fit(pcm, n, FPS, RATE)
    return out


def segment_frames(video, start, end, q, tmp):
    for f in glob.glob(os.path.join(tmp, "f*.jpg")):
        os.remove(f)
    vf = f"fps={FPS},crop=ih*4/3:ih,scale=320:240:flags=lanczos"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", str(start), "-to", str(end), "-i", video,
                    "-vf", vf, "-an", "-pix_fmt", "yuvj420p", "-q:v", str(q),
                    os.path.join(tmp, "f%04d.jpg")], check=True)
    return [open(f, "rb").read() for f in sorted(glob.glob(os.path.join(tmp, "f*.jpg")))]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("video")
    ap.add_argument("--seg", type=segment, action="append", metavar="START-END")
    a = ap.parse_args()
    segments = a.seg or DEFAULT_SEGMENTS
    if not os.path.isfile(a.video):
        sys.exit(f"gen_intro: no such file {a.video}")
    tmp = tempfile.mkdtemp()
    try:
        q = QUALITY
        while True:
            per = [segment_frames(a.video, s0, s1, q, tmp) for s0, s1 in segments]
            frames = [f for seg in per for f in seg]
            blob = pack(frames, FPS)
            if len(blob) <= MAX_BYTES or q >= 20:
                break
            q += 2                                       # too big: lower the quality and retry
        sound = sound_of(a.video, segments, [len(seg) for seg in per])
    except subprocess.CalledProcessError as e:
        sys.exit(f"gen_intro: ffmpeg failed ({e})")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not frames:
        sys.exit("gen_intro: no frames (check --seg)")
    if len(blob) > MAX_BYTES:
        sys.exit(f"gen_intro: {len(blob)} bytes even at -q {q}; use shorter --seg")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    with open(OUT_PCM, "wb") as f:
        f.write(sound or bytes(2))                       # the build cannot embed an empty file
    print(f"gen_intro: {len(frames)} frames at {FPS} fps (-q {q}), {len(blob)} bytes -> {OUT}")
    print(f"gen_intro: sound {len(sound) // 2} samples at {RATE} Hz, {len(sound)} bytes -> {OUT_PCM}")


if __name__ == "__main__":
    main()
