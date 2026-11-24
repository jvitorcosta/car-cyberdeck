"""Pack the S3's media into media.bin for its "media" flash partition (src/util/black/mediapack.h).

  python tools/pack_media.py [out, default .pio/media.bin] [--allow-stand-ins]

Every file in FILES that data/ has goes in; a missing one, or a pre-build stand-in (8 bytes or
less: one silent sample, an empty sprite), is left out, and the firmware reads it as none.
Refuses (exit 1, nothing written) a file that is a zero-filled stand-in (tools/media_sizes.py
placeholders: all zero after its 8-byte header, in its first 4 KB) or lacks its magic, because
`uploadmedia` would flash it over the real media. CI only checks the pack's size, so it passes
--allow-stand-ins; uploadmedia never does.
Fails when the pack is bigger than the partition in partitions_s3.csv.
"""
import os
import struct
import sys
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA = os.path.join(ROOT, "data")
PARTITIONS = os.path.join(ROOT, "partitions_s3.csv")
# The S3's media: the openings' video and sounds, the boot sound rosters (tools/gen_rosters.py), the montage,
# Sonic, McQueen, the CJK lyrics font, the emoji, the update screen's Green Hill Zone and Shadow, the
# camera alerts' voice and the Suki gauge screen's art.
FILES = ["greeting.pcm", "greeting_alt.pcm", "opener.pcm", "finale.pcm", "intro.bin", "intro.pcm",
         "max.bin", "max.pcm", "howl.pcm", "montage.bin", "idle.bin", "mcqueen.bin",
         "cjk16.bin", "emoji16.bin", "backdrop.bin", "shadow.bin", "drive.snd", "opener.snd", "finale.snd",
         "camvoice.bin", "connect.snd", "suki.bin"]
MAGIC, VERSION = b"CDMEDIA1", 1
HEAD, ENTRY, NAME = 16, 32, 20
STAND_IN = 8
PROBE = 4096     # a real file has a non-zero byte in this much of its start, past the header
MAGICS = {"montage.bin": b"MTG1", "intro.bin": b"MTG1", "max.bin": b"MTG1", "idle.bin": b"IDL1",
          "mcqueen.bin": b"CAR1", "cjk16.bin": b"UFNT", "emoji16.bin": b"EMJ2", "backdrop.bin": b"BKD1",
          "shadow.bin": b"SHR1", "camvoice.bin": b"CAMV", "suki.bin": b"SUK1", "drive.snd": b"SNDR",
          "opener.snd": b"SNDR", "finale.snd": b"SNDR", "connect.snd": b"SNDR"}
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


def stand_in_problem(name, blob):
    """Why `blob` (data/<name>) is not real media ("" when it is): a missing magic, or all zero in
    its first 4 KB past the 8-byte header the stand-ins carry (same idea as media_sizes.ride_art_problem)."""
    magic = MAGICS.get(name)
    if magic and not blob.startswith(magic):
        return "%s lacks its %r header" % (name, magic.decode())
    if not any(blob[STAND_IN:PROBE]):
        return "%s is zero-filled in its first %d B (a stand-in)" % (name, PROBE)
    return ""


def shown(path):
    try:
        return os.path.relpath(path)
    except ValueError:   # another drive on Windows
        return path


def main(argv=None):
    args = list(sys.argv[1:] if argv is None else argv)
    allow = "--allow-stand-ins" in args
    args = [a for a in args if a != "--allow-stand-ins"]
    out = args[0] if args else os.path.join(ROOT, ".pio", "media.bin")
    files, bad = [], []
    for name in FILES:
        path = os.path.join(DATA, name)
        if not os.path.isfile(path) or os.path.getsize(path) <= STAND_IN:
            print("pack_media: no %s, left out" % name)
            continue
        blob = open(path, "rb").read()
        problem = stand_in_problem(name, blob)
        if problem:
            bad.append(problem)
        files.append((name, blob))
    if bad and not allow:
        sys.exit("pack_media: refusing to pack stand-in media (it would wipe the real media if "
                 "uploaded): " + "; ".join(bad) + ". Copy the real files into data/ "
                 "(CI passes --allow-stand-ins for its size check).")
    blob = pack(files)
    _, size = partition()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(blob)
    print("pack_media: %d files, %d B = %.1f %% of the %d B partition -> %s"
          % (len(files), len(blob), 100.0 * len(blob) / size, size, shown(out)))
    if len(blob) > size:
        sys.exit("pack_media: the pack does not fit: shrink the montage or the clips")


if __name__ == "__main__":
    main()
