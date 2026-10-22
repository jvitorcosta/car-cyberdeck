# Emoji in names and lyrics: design

2026-10-22. Owner: João.

## Why

Spotify names are UTF-8, and playlist names are full of emoji ("🔥 Gym 💪", "chill 🌙✨").
The deck draws ASCII + Latin-1 accents (font 2) and Japanese/Chinese (Unifont, `data/cjk16.bin`).
`glyphrun::decode` drops every 4-byte UTF-8 character, so today emoji vanish, and the line reads
"Gym" with odd spaces around it.

## Goals

- Real colour emoji, the same pictures a phone shows, drawn 16 px tall in line with the text.
- Everywhere that draws text through `glyphrun` / `ui::drawText` / `ui::drawRun`: the From: line,
  title, artist, the lyric dialog, and the companion's lyrics (it shares `textdraw`).
- Small: ~600 most-used emoji, ~100 KB of flash, **0 bytes of resident RAM** (read from flash
  while drawing, like the CJK font). The owner's main concern is not spending much memory.
- Variation selectors, skin tones, ZWJ sequences, keycaps and tag sequences never print junk.
- Everything else unchanged: accents, CJK, `fit`'s "..." truncation, `wrapTwo` line breaks.

## Non-goals

- The full ~3,700 set (the same format holds it at ~600 KB if ever wanted).
- ZWJ sequences as their own pictures (👨‍👩‍👧, 🏳️‍🌈): they show their first emoji.
- Skin-tone variants: they show the base (yellow) emoji.
- Fetching emoji over the network at run time (offline in the car; TLS heap pressure).
- Soft (alpha-blended) edges.

## Anti-goals

- Boxes, "?" or stray pixels where a sequence part or an uncovered emoji was.
- Any change in how a name without emoji looks or is truncated.
- Extra RAM held by the deck (no decoded emoji cache).

## Decided

- **Source art: Twemoji** (jdecked/twemoji, graphics CC-BY 4.0), the 72x72 PNGs at a pinned
  version, downscaled to 16x16. Flat art survives 16 px better than Noto's shading. Credit goes in
  the README next to GNU Unifont.
- **Set: ~600 most used**, ranked by Unicode's emoji frequency data, kept as a committed ranked list
  in `tools/` so the build is reproducible and the set is easy to grow.
- **Storage: 16-colour palette per emoji**, index 0 = transparent: 16 x RGB565 palette (32 B) +
  16x16 at 4 bpp (128 B) = 160 B per emoji. Hard (1-bit) edges, which suits the pixel look.
- **Uncovered emoji: dropped cleanly** (as today). Builder's call, the owner can switch it to a
  placeholder later.

## Format: `data/emoji16.bin`

Little-endian. Header `"EMJ1"`, `u16 count`, then `count` x `u32 key` sorted ascending, then
`count` glyphs of 160 B in the same order (palette 16 x u16 RGB565, then 128 B of pixels, row-major,
high nibble = left pixel). Glyph i is at `6 + 4*count + 160*i`.

`key` is the codepoint for a single emoji. A flag (two regional indicators, U+1F1E6..U+1F1FF) gets
`0x110000 + a*26 + b` (a, b = 0..25): outside Unicode, so it never clashes.

## Decoding rules (`glyphrun`, pure, host-tested)

New `Kind::Emoji` item (`cp` = key, `w` = 16). `decode` gains an optional emoji width callback
(defaults to none, so existing callers that pass nothing keep today's behaviour). The decoder reads
3- and 4-byte codepoints and applies, in order:

1. U+FE0E / U+FE0F (variation selectors), U+1F3FB..1F3FF (skin tones), U+20E3 (keycap),
   U+E0020..E007F (tags): dropped.
2. U+200D (ZWJ): dropped together with the next codepoint (and that one's own selectors/tones),
   so a sequence collapses to its first emoji.
3. Two regional indicators in a row: one flag key; a lone one is dropped.
4. A codepoint the emoji font covers -> Emoji item. Else a 3-byte codepoint the CJK font covers ->
   Wide item (as today). Else dropped.

Keycaps (`1️⃣`) therefore show the digit as plain text. Overlaps between the emoji and CJK ranges
(U+3030 〰, U+303D 〽) go to emoji.

`fit` and `wrapTwo` treat Emoji like Wide (16 px item; a line may break next to it).

## Drawing (`ui/textdraw`)

`drawRun`/`drawRunScaled` draw an Emoji item as its 16x16 picture at `CJK_DY`, skipping index-0
pixels, scaled by pixel blocks like CJK. No text shadow under emoji (it would outline the picture).
Pixels go out as horizontal runs of one colour (`fillRect`), so no 512 B buffer and no byte-swap
questions.

## Pieces

- `tools/gen_emoji.py` + `tools/emoji_top.txt` (ranked list) -> `data/emoji16.bin` (committed, like
  `cjk16.bin`). Pure helpers (key encoding, palette packing, file layout) tested in
  `tools/test_gen_emoji.py`. Needs Pillow.
- `src/ui/emojifont.*`: pure reader (validate header, sorted keys, bounds; binary search; returns
  palette + pixels). Host test `test/test_emojifont`.
- `src/ui/emojidata.*`: `ui::emoji()` like `ui::cjk()`, via `MEDIA(...)`.
- `src/util/glyphrun.*`: rules above; `test/test_glyphrun` extended.
- Flash: S3 -> `tools/pack_media.py` FILES (media partition, `uploadmedia`); the yellow deck and the
  companion embed it next to `cjk16.bin` in `platformio.ini`.

## Builder's calls (from the review)

- `jsonflat::strAt` turns each `\uXXXX` into its own 3-byte sequence and does not join surrogate
  pairs, so an emoji sent escaped (`🔥`) would reach the deck as two broken codepoints.
  Spotify normally sends raw UTF-8 (and statejson / lrcstream are already fine), but joining the
  pair in `jsonflat` is small and pure, so it is part of this work, with a host test.
- Flash fit is checked by building: `pio run -e s3` (+ `pack_media.py`'s partition check) and the
  yellow deck / companion builds, which also embed the file.

## Left to the builder

The exact N (aim ~600, stay near 100 KB), the downscale/threshold/quantize settings (judge on the
panel), how the ranked list is produced from the Unicode data, extra flags beyond the frequency list
(🇧🇷 at least), and vertical alignment tweaks.
