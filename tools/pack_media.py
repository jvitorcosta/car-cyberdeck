"""Pack the S3's media into media.bin for its "media" flash partition (src/util/mediapack.h).

  python tools/pack_media.py [out, default .pio/media.bin]

Every file in FILES that data/ has goes in; a missing one, or a pre-build stand-in (8 bytes or
less: one silent sample, an empty sprite), is left out, and the firmware reads it as none.
Fails when the pack is bigger than the partition in partitions_s3.csv.
"""
import os
import struct
import sys
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA = os.path.join(ROOT, "data")
PARTITIONS = os.path.join(ROOT, "partitions_s3.csv")
# The S3's media: the openings' video and sounds, the montage, Sonic, McQueen, the music box
# tunes and the CJK lyrics font.
FILES = ["greeting.pcm", "greeting_alt.pcm", "opener.pcm", "finale.pcm", "intro.bin", "intro.pcm",
         "howl.pcm", "montage.bin", "idle.bin", "mcqueen.bin", "tunes.txt", "cjk16.bin"]
MAGIC, VERSION = b"CDMEDIA1", 1
HEAD, ENTRY, NAME = 16, 32, 20
STAND_IN = 8
MAX_FILES = 32   # core/media.cpp checks the CRCs of this many


def partition(name="media"):
    """(offset, size) of a partition in partitions_s3.csv."""
    for line in open(PARTITIONS, encoding="utf-8"):
        cols = [c.strip() for c in line.split("#")[0].split(",")]
        if len(cols) >= 5 and cols[0] == name:
            return int(cols[3], 0), int(cols[4], 0)
    sys.exit("pack_media: no %s partition in %s" % (name, PARTITIONS))


def pack(files):
    """files: [(name, bytes)] -> the pack's bytes."""
    if len(files) > MAX_FILES:
        sys.exit("pack_media: %d files, the firmware reads %d" % (len(files), MAX_FILES))
    off = HEAD + ENTRY * len(files)
    table, body = b"", b""
    for name, blob in files:
        if len(name.encode()) >= NAME:
            sys.exit("pack_media: name too long: " + name)
        body += bytes((-(off + len(body))) % 4)   # 4-byte aligned
        table += struct.pack("<%dsIII" % NAME, name.encode(), off + len(body), len(blob),
                             zlib.crc32(blob))
        body += blob
    return MAGIC + struct.pack("<HHI", VERSION, len(files), off + len(body)) + table + body


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, ".pio", "media.bin")
    files = []
    for name in FILES:
        path = os.path.join(DATA, name)
        if not os.path.isfile(path) or os.path.getsize(path) <= STAND_IN:
            print("pack_media: no %s, left out" % name)
            continue
        files.append((name, open(path, "rb").read()))
    blob = pack(files)
    _, size = partition()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(blob)
    print("pack_media: %d files, %d B = %.1f %% of the %d B partition -> %s"
          % (len(files), len(blob), 100.0 * len(blob) / size, size, os.path.relpath(out)))
    if len(blob) > size:
        sys.exit("pack_media: the pack does not fit: shrink the montage or the clips")


if __name__ == "__main__":
    main()
