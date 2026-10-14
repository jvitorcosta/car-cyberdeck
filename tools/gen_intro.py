"""Build data/intro.bin: the companion (old deck)'s boot intro video, from your own clip.
Run: python tools/gen_intro.py <video> [--start 12] [--end 99]   (seconds; --end past the end = to the end)
The clip and data/intro.bin are git-ignored. Needs ffmpeg. Silent: the old deck plays no sound.

Format: the montage's "MTG1" (ui/montage): u16 count, u16 fps, count x (u32 offset, u32 size),
then baseline JPEG frames, 320x240 (a centre 4:3 crop of the clip). Without a file the companion
boots straight to its home screen (tools/intro_clip.py writes an empty one for the build).
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
FPS = 10
MAX_BYTES = 1_000_000      # the CYD's app partition: keep the intro under ~1 MB
QUALITY = 3                # ffmpeg -q:v (2 best .. 31 worst); raised automatically to fit


def pack(frames, fps):
    """JPEG frames -> MTG1 bytes."""
    head = b"MTG1" + struct.pack("<HH", len(frames), fps)
    off = 8 + 8 * len(frames)
    index = b""
    for f in frames:
        index += struct.pack("<II", off, len(f))
        off += len(f)
    return head + index + b"".join(frames)


def frames_of(video, start, end, q, tmp):
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
    ap.add_argument("--start", type=float, default=12.0)
    ap.add_argument("--end", type=float, default=99.0)
    a = ap.parse_args()
    if not os.path.isfile(a.video):
        sys.exit(f"gen_intro: no such file {a.video}")
    tmp = tempfile.mkdtemp()
    try:
        q = QUALITY
        while True:
            frames = frames_of(a.video, a.start, a.end, q, tmp)
            blob = pack(frames, FPS)
            if len(blob) <= MAX_BYTES or q >= 20:
                break
            q += 2                                       # too big: lower the quality and retry
    except subprocess.CalledProcessError as e:
        sys.exit(f"gen_intro: ffmpeg failed ({e})")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not frames:
        sys.exit("gen_intro: no frames (check --start/--end)")
    if len(blob) > MAX_BYTES:
        sys.exit(f"gen_intro: {len(blob)} bytes even at -q {q}; use a shorter --start/--end")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    print(f"gen_intro: {len(frames)} frames at {FPS} fps (-q {q}), {len(blob)} bytes -> {OUT}")


if __name__ == "__main__":
    main()
