# PlatformIO pre-build script: make sure data/intro.bin (the S3's boot intro video) exists.
#
# data/intro.bin is built from the owner's clip by tools/gen_intro.py and is git-ignored. Without
# one, write an empty montage ("MTG1", 0 frames, 10 fps) so fresh clones and CI build; the
# deck then boots straight to its night drive.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

path = os.path.join(env.subst("$PROJECT_DIR"), "data", "intro.bin")  # noqa: F821
if not os.path.isfile(path):
    with open(path, "wb") as f:
        f.write(b"MTG1" + (0).to_bytes(2, "little") + (10).to_bytes(2, "little"))
    print("intro_clip: no data/intro.bin, building without the intro video")
pcm = os.path.join(env.subst("$PROJECT_DIR"), "data", "intro.pcm")  # noqa: F821
if not os.path.isfile(pcm):
    with open(pcm, "wb") as f:      # one silent sample: the build cannot embed an empty file
        f.write(bytes(2))
    print("intro_clip: no data/intro.pcm, the intro plays silently")
idle = os.path.join(env.subst("$PROJECT_DIR"), "data", "idle.bin")  # noqa: F821
if not os.path.isfile(idle):
    with open(idle, "wb") as f:      # "IDL1", 0 frames: util/idlesprite reads it as none
        f.write(b"IDL1" + bytes(4))
    print("intro_clip: no data/idle.bin, Pikachu stays on Nothing playing")
