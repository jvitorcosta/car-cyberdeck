"""Build the Pokéface watch face's partner sprites (garmin/pokeface/resources/sprites/).
Run: python tools/gen_pokeface.py

Takes the Pokémon Black/White animated sprites of Charmander, Charmeleon and Charizard, and the
Smogon sprite project's Black/White-style Mega Charizard X, from Pokémon Showdown's gen5ani set.
Showdown has no animated one of Mega Y in that style, so Mega Y is its still gen5 sprite (all
frames the same; only the drawn flame moves). They are fetched once into pokeface-sprites/ (git-ignored); the art is Nintendo's / Game
Freak's / Creatures' and the Smogon artists', used on João's own watch, never committed. Spec:
docs/superpowers/specs/2026-10-23-pokeface-watch-face-design.md.

Each GIF gives FRAMES evenly spaced frames, cropped to the box all of them share, and snapped to the Forerunner 255's 64
colours. Charizard and the Megas are scaled down 2x with nearest-neighbour sampling (blocky,
never blurred); Charmander and Charmeleon stay full size, so all of them come out about the same
size. Charizard and the Megas also get a "sleep" frame: frame 0, one colour level darker. The
frames and sprites.xml (their resource ids) land in the git-ignored sprites folder.
"""
import os
import urllib.request

from PIL import Image, ImageSequence

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "pokeface-sprites")
OUT = os.path.join(ROOT, "garmin", "pokeface", "resources", "sprites")
ANI = "https://play.pokemonshowdown.com/sprites/gen5ani/{}.gif"
STILL = "https://play.pokemonshowdown.com/sprites/gen5/{}.png"
FRAMES = 6
PALETTE_LEVELS = (0x00, 0x55, 0xAA, 0xFF)
# (file name, resource id, has a sleep frame, scaled down 2x, source), in Partner's enum order
POKEMON = [("charmander", "Charmander", False, False, ANI.format("charmander")),
           ("charmeleon", "Charmeleon", False, False, ANI.format("charmeleon")),
           ("charizard", "Charizard", True, True, ANI.format("charizard")),
           ("charizard-megax", "MegaX", True, True, ANI.format("charizard-megax")),
           ("charizard-megay", "MegaY", True, True, STILL.format("charizard-megay"))]


def pick_frames(frames, n):
    """n frames spread evenly over the animation; a shorter one repeats from the start."""
    if len(frames) < n:
        return [frames[i % len(frames)] for i in range(n)]
    return [frames[i * len(frames) // n] for i in range(n)]


def crop_union(frames):
    """Every frame cut to the one box that holds all their opaque pixels, so they line up."""
    boxes = [f.getchannel("A").getbbox() for f in frames]
    boxes = [b for b in boxes if b]
    box = (min(b[0] for b in boxes), min(b[1] for b in boxes),
           max(b[2] for b in boxes), max(b[3] for b in boxes))
    return [f.crop(box) for f in frames]


def halve(img):
    return img.resize((img.width // 2, img.height // 2), Image.NEAREST)


def _level(v):
    return min(PALETTE_LEVELS, key=lambda p: abs(p - v))


def snap(img):
    """Each channel to the nearest of the watch's four levels; alpha to on or off."""
    out = img.copy()
    out.putdata([(_level(r), _level(g), _level(b), 255) if a >= 128 else (0, 0, 0, 0)
                 for r, g, b, a in img.get_flattened_data()])
    return out


def darken(img):
    """One palette level down on every channel (the sleep frame)."""
    down = {p: PALETTE_LEVELS[max(0, i - 1)] for i, p in enumerate(PALETTE_LEVELS)}
    out = snap(img)
    out.putdata([(down[r], down[g], down[b], a) for r, g, b, a in out.get_flattened_data()])
    return out


def sprites_xml():
    rows = []
    for name, rid, sleep, _, _ in POKEMON:
        rows += ['    <bitmap id="%s%d" filename="%s_%d.png" dithering="none"/>' % (rid, i, name, i)
                 for i in range(FRAMES)]
        if sleep:
            rows.append('    <bitmap id="%sSleep" filename="%s_sleep.png" dithering="none"/>' % (rid, name))
    return "<drawables>\n" + "\n".join(rows) + "\n</drawables>\n"


def convert(frames, half):
    """An animation's frames -> FRAMES watch-ready frames."""
    out = crop_union(pick_frames(frames, FRAMES))
    return [snap(halve(f) if half else f) for f in out]


def fetch(name, url):
    path = os.path.join(SRC, name + os.path.splitext(url)[1])
    if not os.path.exists(path):
        os.makedirs(SRC, exist_ok=True)
        req = urllib.request.Request(url, headers={"User-Agent": "pokeface-sprite-tool"})
        with urllib.request.urlopen(req, timeout=30) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, _, sleep, half, url in POKEMON:
        with Image.open(fetch(name, url)) as gif:
            frames = [f.convert("RGBA") for f in ImageSequence.Iterator(gif)]
        out = convert(frames, half)
        for i, f in enumerate(out):
            f.save(os.path.join(OUT, "%s_%d.png" % (name, i)))
        if sleep:
            darken(out[0]).save(os.path.join(OUT, name + "_sleep.png"))
        print("%-16s %d frames in the source -> %d x %dx%d" % (name, len(frames), FRAMES, *out[0].size))
    with open(os.path.join(OUT, "sprites.xml"), "w", encoding="utf-8") as f:
        f.write(sprites_xml())


if __name__ == "__main__":
    main()
