"""Build the Pokéface watch face's Pokémon sheets and Emerald fonts (garmin/pokeface/resources/gen/).
Run: python tools/gen_pokeface.py

Everything comes from one pinned commit of rh-hideout/pokeemerald-expansion (the Pokémon
Emerald decompilation, extended to every species in Gen 3 style), fetched once into
pokefont-src/ (git-ignored). The Gen 3 Pokémon and the font are Nintendo's / Game Freak's art;
the later species were drawn by the expansion's artists. Used on João's own watch, never
committed. Spec: docs/superpowers/specs/2026-10-23-pokeface-emerald-design.md.

Pokémon: each anim_front.png (two 64x64 frames, stacked) gets its normal.pal colours, palette
index 0 transparent, snapped to the Forerunner 255's 64 colours and mirrored to face right; the
two frames sit side by side in one 128x64 sheet, mon<i>.png, in SPECIES order (the watch draws
them at 2x).
Font: Emerald's latin_normal sheet (16x16 cells in charmap order, widths from src/fonts.c) gives
the GLYPHS as BMFont fonts: emerald1 (1x), emerald2 (2x) and emerald4 (4x, digits and ':' only,
for the time). gen.xml lists all of them as resources.
"""
import os
import re
import urllib.request

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "pokefont-src")
OUT = os.path.join(ROOT, "garmin", "pokeface", "resources", "gen")
BASE = ("https://raw.githubusercontent.com/rh-hideout/pokeemerald-expansion/"
        "e8914bd0ae80db0af135056f37e3aad5664c1be9/")
PALETTE_LEVELS = (0x00, 0x55, 0xAA, 0xFF)
# Monday to Sunday: day, night, reward (Partner.NAMES and the watch's Mon<i> ids follow this order)
SPECIES = [("blaziken", "BLAZIKEN"), ("sceptile", "SCEPTILE"), ("latias", "LATIAS"),
           ("ho_oh", "HO-OH"), ("lugia", "LUGIA"), ("suicune", "SUICUNE"),
           ("rayquaza", "RAYQUAZA"), ("deoxys", "DEOXYS"), ("latios", "LATIOS"),
           ("cresselia", "CRESSELIA"), ("darkrai", "DARKRAI"), ("jirachi", "JIRACHI"),
           ("arceus", "ARCEUS"), ("giratina", "GIRATINA"), ("dialga", "DIALGA"),
           ("solgaleo", "SOLGALEO"), ("lunala", "LUNALA"), ("necrozma", "NECROZMA"),
           ("xerneas", "XERNEAS"), ("yveltal", "YVELTAL"), ("zygarde", "ZYGARDE")]
GLYPHS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz :./-%°"
TIME_GLYPHS = "0123456789:"
FONT_INK = (56, 56, 56)   # the letters' dark grey in latin_normal.png (the shadow is light grey, the cell blue)


def _level(v):
    return min(PALETTE_LEVELS, key=lambda p: abs(p - v))


def read_jasc(text):
    lines = text.split()
    n = int(lines[2])
    return [tuple(int(v) for v in lines[3 + i * 3:6 + i * 3]) for i in range(n)]


def frames(sheet):
    """A 64x128 stack -> its two frames; a lone 64x64 frame -> itself twice."""
    w = sheet.width
    if sheet.height >= 2 * w:
        return [sheet.crop((0, 0, w, w)), sheet.crop((0, w, w, 2 * w))]
    f = sheet.crop((0, 0, w, w))
    return [f, f.copy()]


def colourise(frame, pal):
    """Palette indices -> RGBA in the watch's colours; index 0 is the see-through background."""
    out = Image.new("RGBA", frame.size)
    out.putdata([(0, 0, 0, 0) if i == 0 else tuple(_level(c) for c in pal[i]) + (255,)
                 for i in frame.get_flattened_data()])
    return out


def sheet(two):
    """Both frames side by side, each mirrored so the Pokémon faces right (its info box)."""
    out = Image.new("RGBA", (128, 64), (0, 0, 0, 0))
    for i, f in enumerate(two):
        out.paste(f.transpose(Image.FLIP_LEFT_RIGHT), (i * 64, 0))
    return out


def read_charmap(text):
    return {m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"^'(.)'\s*=\s*([0-9A-Fa-f]{2})\b", text, re.M)}


def read_widths(c_source):
    m = re.search(r"gFontNormalLatinGlyphWidths\[\]\s*=\s*\{([^}]*)\}", c_source)
    return [int(v) for v in re.findall(r"\d+", m.group(1))]


def glyph(fontsheet, code, width):
    """Cell `code` of the 16x16 grid, `width` wide: the letter's ink white, the rest clear."""
    x0, y0 = (code % 16) * 16, (code // 16) * 16
    cell = fontsheet.convert("RGBA").crop((x0, y0, x0 + width, y0 + 16))
    cell.putdata([(255, 255, 255, 255) if p[:3] == FONT_INK else (0, 0, 0, 0)
                  for p in cell.get_flattened_data()])
    return cell


def degree(height):
    """Emerald has no °: a small 3x3 ring at the top of a glyph `height` tall."""
    im = Image.new("RGBA", (4, height), (0, 0, 0, 0))
    for x, y in [(0, 1), (1, 0), (2, 1), (1, 2), (0, 0), (2, 0), (0, 2), (2, 2)]:
        if (x, y) != (1, 1):
            im.putpixel((x, y + 1), (255, 255, 255, 255))
    return im


def bmfont(glyphs, scale, name):
    """A BMFont text file and its page: glyphs in a row, scaled `scale` x with no smoothing."""
    scaled = {c: g.resize((g.width * scale, g.height * scale), Image.NEAREST) for c, g in glyphs.items()}
    h = max(g.height for g in scaled.values())
    page = Image.new("RGBA", (sum(g.width + 1 for g in scaled.values()), h), (0, 0, 0, 0))
    rows, x = [], 0
    for c, g in scaled.items():
        page.paste(g, (x, 0))
        rows.append("char id=%d x=%d y=0 width=%d height=%d xoffset=0 yoffset=0 xadvance=%d page=0 chnl=15"
                    % (ord(c), x, g.width, g.height, g.width + scale))
        x += g.width + 1
    text = ("info face=\"%s\" size=%d bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=0 aa=1 "
            "padding=0,0,0,0 spacing=1,1\n" % (name, h)
            + "common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=1 packed=0\n" % (h, h, page.width, h)
            + "page id=0 file=\"%s.png\"\n" % name + "chars count=%d\n" % len(rows) + "\n".join(rows) + "\n")
    return text, page


def gen_xml():
    bitmaps = "\n".join('    <bitmap id="Mon%d" filename="mon%d.png" dithering="none"/>' % (i, i)
                        for i in range(len(SPECIES)))
    fonts = ('    <font id="Emerald1" filename="emerald1.fnt" antialias="false"/>\n'
             '    <font id="Emerald2" filename="emerald2.fnt" antialias="false"/>\n'
             '    <font id="EmeraldTime" filename="emerald4.fnt" antialias="false"/>')
    return ("<resources>\n<drawables>\n" + bitmaps + "\n</drawables>\n<fonts>\n" + fonts
            + "\n</fonts>\n</resources>\n")


def fetch(path):
    """BASE + path, cached in pokefont-src/ (written to .tmp first, so a cut download isn't kept)."""
    local = os.path.join(SRC, path.replace("/", "_"))
    if not os.path.exists(local):
        os.makedirs(SRC, exist_ok=True)
        req = urllib.request.Request(BASE + path, headers={"User-Agent": "pokeface-sprite-tool"})
        with urllib.request.urlopen(req, timeout=30) as r, open(local + ".tmp", "wb") as f:
            f.write(r.read())
        os.replace(local + ".tmp", local)
    return local


def _text(path):
    with open(fetch(path), encoding="utf-8", errors="replace") as f:
        return f.read()


def main():
    os.makedirs(OUT, exist_ok=True)
    for i, (folder, name) in enumerate(SPECIES):
        pal = read_jasc(_text("graphics/pokemon/%s/normal.pal" % folder))
        with Image.open(fetch("graphics/pokemon/%s/anim_front.png" % folder)) as src:
            two = frames(src)
            print("%-10s source %dx%d mode %s" % (name, src.width, src.height, src.mode))
        sheet([colourise(f, pal) for f in two]).save(os.path.join(OUT, "mon%d.png" % i))

    charmap = read_charmap(_text("charmap.txt"))
    widths = read_widths(_text("src/fonts.c"))
    with Image.open(fetch("graphics/fonts/latin_normal.png")) as fs:
        fontsheet = fs.convert("RGBA")
    a = charmap["A"]
    cell = fontsheet.crop(((a % 16) * 16, (a // 16) * 16, (a % 16) * 16 + 16, (a // 16) * 16 + 16))
    print("font cell 'A' colours:", sorted(cell.getcolors(), reverse=True))
    glyphs = {c: glyph(fontsheet, charmap[c], widths[charmap[c]]) for c in GLYPHS if c in charmap}
    glyphs[" "] = Image.new("RGBA", (widths[charmap[" "]] if " " in charmap else 4, 16), (0, 0, 0, 0))
    glyphs["°"] = degree(16)
    missing = [c for c in GLYPHS if c not in glyphs]
    print("glyphs:", len(glyphs), "missing:", missing)
    for scale, name, chars in [(1, "emerald1", GLYPHS), (2, "emerald2", GLYPHS), (4, "emerald4", TIME_GLYPHS)]:
        text, page = bmfont({c: glyphs[c] for c in chars if c in glyphs}, scale, name)
        page.save(os.path.join(OUT, name + ".png"))
        with open(os.path.join(OUT, name + ".fnt"), "w", encoding="utf-8") as f:
            f.write(text)
    with open(os.path.join(OUT, "gen.xml"), "w", encoding="utf-8") as f:
        f.write(gen_xml())


if __name__ == "__main__":
    main()
