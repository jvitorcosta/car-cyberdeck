# PlatformIO pre-build script: make sure data/avatar.bin (the deck avatar's portraits) exists.
#
# data/avatar.bin is built from the owner's images by tools/gen_avatar.py and is git-ignored (the
# character art isn't ours to publish). Without one, write an empty file ("AVT1", 0 moods) so
# fresh clones and CI build; the deck then runs without the avatar.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

path = os.path.join(env.subst("$PROJECT_DIR"), "data", "avatar.bin")  # noqa: F821
if not os.path.isfile(path):
    with open(path, "wb") as f:
        f.write(b"AVT1" + bytes(14))
    print("avatar_clip: no data/avatar.bin, building without the avatar")
