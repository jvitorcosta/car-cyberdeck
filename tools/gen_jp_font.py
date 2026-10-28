"""Generate data/jp16.bin: the Japanese subset of the Unifont glyphs, for the yellow deck's lyrics.

Why: the yellow deck (ESP32 CYD) draws Latin-1 VT323 only, and the black deck's whole CJK blob
(data/cjk16.bin, ~686 KB) doesn't fit its app slot. Japanese needs far less: the kana, the CJK
symbols and punctuation, the half/fullwidth forms and the JIS X 0208 level 1 kanji (2,965, the
common ones), plus 々 〆 ヶ. Glyphs outside it show as a box on the deck.
The glyphs come from data/cjk16.bin (GNU Unifont 16.0.04, made by tools/gen_cjk_font.py; GPLv2+
with the GNU font embedding exception, or SIL OFL 1.1: see CREDITS.md), read with that blob's own
format, as tools/gen_gojira_glyphs.py does.
Run: python -I tools/gen_jp_font.py
Format (little-endian), read by src/yellow/ui/jpfont: "JFNT", u16 n, n x u16 codepoint (ascending),
n x u8 width (8 or 16), then n glyphs of 32 bytes each: 16 rows, 2 B/row for width 16, 1 B/row
in the first 16 bytes for width 8 (the rest zero); MSB the leftmost pixel.
"""
import os
import struct
import unicodedata

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "data", "cjk16.bin")
OUT = os.path.join(ROOT, "data", "jp16.bin")
# CJK symbols and punctuation, hiragana, katakana, small kana (katakana phonetic extensions),
# halfwidth and fullwidth forms
BLOCKS = [(0x3000, 0x303F), (0x3040, 0x309F), (0x30A0, 0x30FF), (0x31F0, 0x31FF), (0xFF00, 0xFFEF)]
EXTRA = [0x3005, 0x3006, 0x30F6]  # 々 〆 ヶ (in the blocks too; named as the brief asks)
GLYPH = 32


def jis_level1():
    """The JIS X 0208 level 1 kanji (rows 16-47), as codepoints: every EUC-JP two-byte code with a
    lead byte 0xB0-0xCF that decodes."""
    out = []
    for lead in range(0xB0, 0xD0):
        for trail in range(0xA1, 0xFF):
            try:
                ch = bytes([lead, trail]).decode("euc_jp")
            except UnicodeDecodeError:
                continue
            if len(ch) == 1:
                out.append(ord(ch))
    return out


def wanted():
    """Every codepoint of the subset, ascending: the assigned characters of BLOCKS, the level 1
    kanji and EXTRA."""
    cps = set(jis_level1()) | set(EXTRA)
    for a, b in BLOCKS:
        cps |= {c for c in range(a, b + 1) if unicodedata.category(chr(c)) != "Cn"}
    return sorted(cps)


def read_ufnt(blob):
    """cjk16.bin as {codepoint: (width, bitmap)} (cjkfont::Font's format)."""
    if blob[:4] != b"UFNT":
        raise SystemExit("not a UFNT blob: " + SRC)
    glyphs = {}
    n = struct.unpack_from("<H", blob, 4)[0]
    for i in range(n):
        first, last, width, offset = struct.unpack_from("<IIBI", blob, 6 + 13 * i)
        per = 16 if width == 8 else 32
        for c in range(first, last + 1):
            at = offset + (c - first) * per
            glyphs[c] = (width, bytes(blob[at:at + per]))
    return glyphs


def pack(entries):
    """entries: [(codepoint, width, bitmap)] ascending -> the JFNT blob."""
    cps = [c for c, _, _ in entries]
    if cps != sorted(set(cps)) or any(c > 0xFFFF for c in cps):
        raise ValueError("codepoints must be unique, ascending and in the BMP")
    out = bytearray(b"JFNT" + struct.pack("<H", len(entries)))
    out += b"".join(struct.pack("<H", c) for c in cps)
    out += bytes(w for _, w, _ in entries)
    for _, w, bm in entries:
        if len(bm) != (16 if w == 8 else 32):
            raise ValueError("bitmap size does not match its width")
        out += bm.ljust(GLYPH, b"\0")
    return bytes(out)


def main():
    with open(SRC, "rb") as f:
        src = read_ufnt(f.read())
    missing = [c for c in wanted() if c not in src]
    if missing:
        raise SystemExit("not in %s: %s" % (SRC, " ".join("U+%04X" % c for c in missing)))
    entries = [(c,) + src[c] for c in wanted()]
    blob = pack(entries)
    with open(OUT, "wb") as f:
        f.write(blob)
    kanji = len(jis_level1())
    print("wrote %s: %d glyphs (%d level 1 kanji), %d bytes" % (OUT, len(entries), kanji, len(blob)))


if __name__ == "__main__":
    main()
