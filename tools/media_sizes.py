"""The deck's private media (git-ignored) at their real sizes, so CI's flash check sees the
firmware you actually flash. Only the sizes are committed (data/media_sizes.txt), never the media.

  python tools/media_sizes.py record          after changing your clips/montage: update the sizes
  python tools/media_sizes.py placeholders    CI: same-size stand-ins for the missing files

Stand-ins are silent PCM and empty containers (montage/intro "MTG1" with 0 frames, avatar "AVT1")
padded to size, so the firmware links at its real size and still boots.
"""
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA = os.path.join(ROOT, "data")
SIZES = os.path.join(DATA, "media_sizes.txt")
# Both decks' media: the S3's sounds, montage, intro videos (kiss, Max) and their sound, second greeting, wolf howl,
# Sonic and McQueen (its media partition), the companion's avatar. A zero-filled McQueen stand-in
# fails carbin's checks, so the S3 reads it as no McQueen.
FILES = ["greeting.pcm", "opener.pcm", "finale.pcm", "montage.bin",
         "intro.bin", "intro.pcm", "greeting_alt.pcm", "howl.pcm", "avatar.bin", "idle.bin",
         "mcqueen.bin", "slzbg.bin", "max.bin", "max.pcm"]
MONTAGE_EMPTY = b"MTG1" + (0).to_bytes(2, "little") + (10).to_bytes(2, "little")
# Stand-ins start with an empty header, so the firmware reads "no clip" / "no avatar" and boots.
HEADS = {"montage.bin": MONTAGE_EMPTY, "intro.bin": MONTAGE_EMPTY, "max.bin": MONTAGE_EMPTY,
         "avatar.bin": b"AVT1", "idle.bin": b"IDL1", "slzbg.bin": b"BKD1"}


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
