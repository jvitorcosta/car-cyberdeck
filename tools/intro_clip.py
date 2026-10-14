# PlatformIO pre-build script: make sure data/intro.bin (the companion's boot video) exists.
#
# data/intro.bin is built from the owner's clip by tools/gen_intro.py and is git-ignored. Without
# one, write an empty montage ("MTG1", 0 frames, 10 fps) so fresh clones and CI build; the
# companion then boots straight to its home screen.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

path = os.path.join(env.subst("$PROJECT_DIR"), "data", "intro.bin")  # noqa: F821
if not os.path.isfile(path):
    with open(path, "wb") as f:
        f.write(b"MTG1" + (0).to_bytes(2, "little") + (10).to_bytes(2, "little"))
    print("intro_clip: no data/intro.bin, building without the intro video")
