"""Build data/avatar.bin: the deck avatar's portraits from the owner's images
(docs/superpowers/specs/2026-10-14-avatar-design.md).
Run: python tools/gen_avatar.py [folder]      (default: avatar-photos/, which holds avatar.json)
The images and data/avatar.bin are git-ignored: the character art isn't ours to publish.
Needs ffmpeg and ffprobe.

avatar.json:
  {"name": "ZERO TWO",
   "moods": {"smug":  {"file": "a.png", "crop": [size, x, y], "colors": 0, "boost": false},
             "talk":  {"frames": ["rest.png", "open.png"]},
             "happy": {"file": "b.png"}}}
An image that is already 92x92 is used as is (crop, colors and boost are ignored for it).
Others are cropped square, optionally boosted, line-thickened (erosion), area-scaled to 92x92
and, with "colors": N, given their own N-colour palette without dithering.

Format: "AVT1", u8 mood count, u8 0, name (12 bytes, NUL-padded ASCII), count x (u8 mood id,
u8 frame count), then every frame as 92x92 RGB565, high byte first, mood by mood.
Also writes avatar-preview.png (all frames at 4x) into the folder.
"""
import json
import os
import struct
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "avatar.bin")
SIDE = 92
FRAME = SIDE * SIDE * 2
MOODS = {"smug": 0, "talk": 1, "happy": 2}
MAX_FRAMES = 8
NAME_LEN = 12
BOOST = "eq=saturation=1.35:contrast=1.12"   # for washed-out anime frames


class ManifestError(Exception):
    pass


def validate(manifest, folder, size_of):
    """Checks avatar.json. Returns (name, [(mood id, [(path, entry), ...]), ...]) in file order.
    size_of(path) -> (width, height)."""
    name = manifest.get("name", "")
    if not isinstance(name, str) or not name or len(name) > NAME_LEN or not name.isascii():
        raise ManifestError(f"name must be 1-{NAME_LEN} ASCII characters")
    moods = manifest.get("moods") or {}
    if not moods:
        raise ManifestError("no moods")
    out = []
    for mood, entry in moods.items():
        if mood not in MOODS:
            raise ManifestError(f"unknown mood '{mood}' (use {', '.join(MOODS)})")
        if ("file" in entry) == ("frames" in entry):
            raise ManifestError(f"{mood}: give exactly one of 'file' and 'frames'")
        files = [entry["file"]] if "file" in entry else list(entry["frames"])
        if not 1 <= len(files) <= MAX_FRAMES:
            raise ManifestError(f"{mood}: 1-{MAX_FRAMES} frames")
        frames = []
        for f in files:
            path = os.path.join(folder, f)
            if not os.path.isfile(path):
                raise ManifestError(f"{mood}: missing file {f}")
            w, h = size_of(path)
            if (w, h) != (SIDE, SIDE):
                crop = entry.get("crop")
                if not crop or len(crop) != 3:
                    raise ManifestError(f"{mood}: {f} is {w}x{h}, so it needs \"crop\": [size, x, y]")
                c, x, y = crop
                if c <= 0 or x < 0 or y < 0 or x + c > w or y + c > h:
                    raise ManifestError(f"{mood}: crop {crop} is outside {f} ({w}x{h})")
            frames.append((path, entry))
        out.append((MOODS[mood], frames))
    return name, out


def rgb565be(rgb):
    """RGB24 bytes -> RGB565, high byte first (the order pushImage sends)."""
    out = bytearray()
    for i in range(0, len(rgb), 3):
        r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
        out += struct.pack(">H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return bytes(out)


def pack(name, moods):
    """moods: [(mood id, [frame bytes, FRAME each])] -> the avatar.bin bytes."""
    head = b"AVT1" + bytes([len(moods), 0]) + name.encode("ascii").ljust(NAME_LEN, b"\0")
    for mood_id, frames in moods:
        head += bytes([mood_id, len(frames)])
    return head + b"".join(f for _, frames in moods for f in frames)


def size_of(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=width,height",
                        "-of", "csv=p=0", path], check=True, capture_output=True, text=True)
    w, h = r.stdout.strip().splitlines()[0].split(",")[:2]
    return int(w), int(h)


def rgb24(path, entry):
    """One 92x92 frame as RGB24 bytes."""
    if size_of(path) == (SIDE, SIDE):
        graph = "null"
    else:
        c, x, y = entry["crop"]
        chain = [f"crop={c}:{c}:{x}:{y}"]
        if entry.get("boost"):
            chain.append(BOOST)
        chain += ["erosion", f"scale={SIDE}:{SIDE}:flags=area"]   # thicken lines, then shrink
        graph = ",".join(chain)
        colors = int(entry.get("colors", 0))
        if colors:
            graph += (f",split[a][b];[a]palettegen=max_colors={colors}:reserve_transparent=0"
                      f":stats_mode=full[p];[b][p]paletteuse=dither=none")
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-filter_complex", graph,
                        "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                       check=True, capture_output=True)
    if len(r.stdout) != SIDE * SIDE * 3:
        raise ManifestError(f"{path}: ffmpeg returned {len(r.stdout)} bytes")
    return r.stdout


def preview(frames, path):
    """All frames side by side at 4x."""
    strip = bytearray()
    for y in range(SIDE):
        for f in frames:
            strip += f[y * SIDE * 3:(y + 1) * SIDE * 3]
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                    "-s", f"{SIDE * len(frames)}x{SIDE}", "-i", "-",
                    "-vf", "scale=iw*4:ih*4:flags=neighbor", path], input=bytes(strip), check=True)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "avatar-photos")
    try:
        with open(os.path.join(folder, "avatar.json"), encoding="utf-8") as f:
            manifest = json.load(f)
        name, moods = validate(manifest, folder, size_of)
        shown, packed = [], []
        for mood_id, frames in moods:
            rgb = [rgb24(p, e) for p, e in frames]
            shown += rgb
            packed.append((mood_id, [rgb565be(x) for x in rgb]))
    except (OSError, ValueError, ManifestError, subprocess.CalledProcessError) as e:
        sys.exit(f"gen_avatar: {e}")
    blob = pack(name, packed)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    preview(shown, os.path.join(folder, "avatar-preview.png"))
    print(f"gen_avatar: {name}, {len(packed)} moods, {len(shown)} frames, {len(blob)} bytes -> {OUT}")


if __name__ == "__main__":
    main()
