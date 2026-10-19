# PlatformIO pre-build script: data/tunes.txt, the S3 music box's tune set (util/tunes.h format).
#
# Every data/tunes/*.txt (your own, from tools/gen_tune.py song.mid; git-ignored) in name order,
# else the committed public-domain data/tunes_default.txt. Rewritten every build, so adding or
# removing a tune takes effect at once.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import glob
import os

data = os.path.join(env.subst("$PROJECT_DIR"), "data")  # noqa: F821
mine = sorted(glob.glob(os.path.join(data, "tunes", "*.txt")))
srcs = mine or [os.path.join(data, "tunes_default.txt")]
text = "\n".join(open(p, encoding="utf-8").read() for p in srcs).encode("utf-8")
out = os.path.join(data, "tunes.txt")
if not os.path.isfile(out) or open(out, "rb").read() != text:   # unchanged: no relink
    with open(out, "wb") as f:
        f.write(text)
print(f"tune_clip: {len(mine)} tune file(s) in data/tunes/" if mine
      else "tune_clip: no data/tunes/, using the public-domain fallback tunes")
