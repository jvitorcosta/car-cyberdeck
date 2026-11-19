# tools/suki_ride_preview.py
"""The Suki drive screen on the PC, before any flashing: builds tools/suki_ride_preview.cpp with g++,
renders the drive states and every theme from data/sukiride.bin (tools/gen_suki_ride.py) and makes
one PNG sheet (each screen at 2x, labelled). Run:
  python tools/suki_ride_preview.py [--bin data/sukiride.bin] [--out suki-ride-preview.png]
The sheet shows the film's art: keep it private (don't commit or publish it). Needs g++ and Pillow.
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = ["tools/suki_ride_preview.cpp", "src/util/car/sukiride.cpp", "src/util/car/drivedash.cpp",
       "src/util/common/termpal.cpp"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bin", default=os.path.join(ROOT, "data", "sukiride.bin"))
    ap.add_argument("--out", default="suki-ride-preview.png")
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix="sukiride_")
    exe = os.path.join(tmp, "preview.exe" if os.name == "nt" else "preview")
    cc = ["g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Werror"] + [os.path.join(ROOT, s) for s in SRC] + ["-o", exe]
    if os.name == "nt":
        cc += ["-static-libstdc++", "-static-libgcc"]
    subprocess.run(cc, check=True)
    subprocess.run([exe, a.bin, tmp], check=True)
    shots = sorted(glob.glob(os.path.join(tmp, "*.ppm")))
    s = a.scale
    cols = 4
    cw, ch = 320 * s, 240 * s + 22
    sheet = Image.new("RGB", (cols * (cw + 8) + 8, ((len(shots) + cols - 1) // cols) * (ch + 8) + 8), (20, 20, 24))
    d = ImageDraw.Draw(sheet)
    for i, p in enumerate(shots):
        im = Image.open(p).resize((320 * s, 240 * s), Image.NEAREST)
        x, y = 8 + (i % cols) * (cw + 8), 8 + (i // cols) * (ch + 8)
        sheet.paste(im, (x, y + 22))
        d.text((x, y + 4), os.path.splitext(os.path.basename(p))[0], fill=(230, 230, 230))
    sheet.save(a.out)
    print("suki_ride_preview: %d screens -> %s" % (len(shots), a.out))


if __name__ == "__main__":
    sys.exit(main())
