"""The deck's private media (git-ignored) at their real sizes, so CI's flash check sees the
firmware you actually flash. Only the sizes are committed (data/media_sizes.txt), never the media.

  python tools/media_sizes.py record          after changing your clips/montage: update the sizes
  python tools/media_sizes.py placeholders    CI: same-size stand-ins for the missing files

Stand-ins are silent PCM and empty containers (montage/intro "MTG1" with 0 frames)
padded to size, so the firmware links at its real size and still boots.
"""
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA = os.path.join(ROOT, "data")
SIZES = os.path.join(DATA, "media_sizes.txt")
# The S3's media: its sounds, montage, intro videos (kiss, Max) and their sound, second greeting, wolf howl,
# Sonic, McQueen, the boot sound rosters, the camera alerts' voice and the Suki gauge art (its media partition); a zero-filled
# roster reads as none. A zero-filled McQueen stand-in fails carbin's checks, so the S3 reads it as no
# McQueen; a zero-filled camvoice.bin has no "CAMV" header, so the S3 reads it as no voice (beeps only); a
# zero-filled suki.bin has no "SUK1", so the gauges keep the street look. sukiride.bin (the Suki drive
# screen's art, "SKR1") is linked into the S3's app, not its media pack: zero-filled, the Ride screen
# falls back to the others.
FILES = ["greeting.pcm", "opener.pcm", "finale.pcm", "montage.bin",
         "intro.bin", "intro.pcm", "greeting_alt.pcm", "howl.pcm", "idle.bin",
         "mcqueen.bin", "backdrop.bin", "max.bin", "max.pcm", "shadow.bin",
         "drive.snd", "opener.snd", "finale.snd", "camvoice.bin", "connect.snd", "suki.bin",
         "sukiride.bin"]
MONTAGE_EMPTY = b"MTG1" + (0).to_bytes(2, "little") + (10).to_bytes(2, "little")
# Stand-ins start with an empty header, so the firmware reads "no clip" and boots.
HEADS = {"montage.bin": MONTAGE_EMPTY, "intro.bin": MONTAGE_EMPTY, "max.bin": MONTAGE_EMPTY,
         "idle.bin": b"IDL1", "backdrop.bin": b"BKD1", "shadow.bin": b"SHR1"}


RIDE_MAGIC = b"SKR"   # sukiride.bin's header, "SKR<version>" (tools/gen_suki_ride.py)


def ride_art_problem(path):
    """Why the Ride art at `path` would not show on the deck ("" when it is real art). A zero-filled
    stand-in (placeholders(), or a worktree's copy) links fine and boots, but the Ride screen then
    falls back to the Suki film monitor: tools/media_upload.py refuses a firmware upload with it."""
    if not os.path.isfile(path):
        return "%s is missing" % path
    with open(path, "rb") as f:
        head = f.read(4)
    if not head.startswith(RIDE_MAGIC):
        return "%s is a stand-in (no %r header): the Ride screen would show the Suki monitor" % (
            path, RIDE_MAGIC.decode())
    return ""


def record():
    with open(SIZES, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Real sizes of the git-ignored media (tools/media_sizes.py record)\n")
        for name in FILES:
            path = os.path.join(DATA, name)
            if not os.path.isfile(path):
                sys.exit("media_sizes: %s is missing; build it first" % path)
            f.write("%s %d\n" % (name, os.path.getsize(path)))
    print("media_sizes: wrote", os.path.relpath(SIZES))


def placeholders():
    for line in open(SIZES, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        name, size = line.split()
        path = os.path.join(DATA, name)
        if os.path.isfile(path):
            continue
        head = HEADS.get(name, b"")
        with open(path, "wb") as f:
            f.write(head + bytes(int(size) - len(head)))
        print("media_sizes: stand-in %s (%s B)" % (name, size))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "record":
        record()
    elif cmd == "placeholders":
        placeholders()
    else:
        sys.exit(__doc__)
