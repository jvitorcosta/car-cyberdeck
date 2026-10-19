# PlatformIO pre-build script: make sure data/mcqueen.bin (the S3's pixel McQueen) exists.
#
# data/mcqueen.bin is yours (tools/gen_mcqueen_sprite.py mcqueen.png) and git-ignored. Without one,
# a one-sample stand-in is written (the build can't embed an empty file); the deck then never
# picks the McQueen opening (util/carbin refuses it).
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

out = os.path.join(env.subst("$PROJECT_DIR"), "data", "mcqueen.bin")  # noqa: F821
if not os.path.isfile(out):
    with open(out, "wb") as f:
        f.write(bytes(2))
    print("mcqueen_clip: no data/mcqueen.bin, the McQueen opening is off")
