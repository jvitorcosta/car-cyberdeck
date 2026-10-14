# Deck avatar (step 1: portrait and canned lines)

**Date:** 2026-10-14
**Status:** draft for review
**Builds:** `s3` only

## Goal

A character, the "avatar", slides into the album-art box now and then. She shows an expression and says a short line in the battle dialogue box, then the deck goes back to normal. The owner's avatar is Zero Two (Darling in the Franxx). Her art comes from images the owner supplies.

This is step 1 of the assistant idea in `docs/BACKLOG.md`:
- **Step 1 (this spec):** the portrait and canned lines, with no AI, no phone and no network.
- **Step 2 (its own spec):** a Siri Shortcut asks Claude, and a new `POST /say` makes her speak.

Step 2 can only drive what step 1 builds, so step 1 has to stand on its own.

## Decisions taken while brainstorming

- **Placement:** a Gen-3-style "trainer slide-in" over the 92×92 album-art box. A pink name tag sits on the dialogue box. Her line replaces the lyric for a few seconds, then the art and the lyrics come back. Chosen over a visual-novel bust beside the dialogue box and over an always-on chibi in the top strip.
- **Art source:** the owner's own images, converted. Hand-drawn drafts (chibi and anime styles) were tried, and converting the owner's pictures gave the most faithful result. The repo is public and the art is copyrighted, so the images and the converted file are git-ignored and never committed. This follows the same pattern as `montage-photos/` and `data/greeting.pcm`.
- **Moods (v1).** Each mood is one or more 92×92 frames:

  | Mood id | Name | Frames | Source used today | Used for |
  |---|---|---|---|---|
  | 0 | `smug` | 1 | the lollipop pixel art (`zero-two.png`), converted by the tool | default |
  | 1 | `talk` | 5 | PixelLab mouth positions made from the sly-smile portrait: rest, open, round, teeth, smile | most lines; she "talks" while the text types |
  | 2 | `happy` | 1 | the big grin (`zero-two-smile.jpeg`), via PixelLab's image-to-pixel-art | greetings, good news |

  - **Talking frames.** PixelLab (`create_vocal_animation`) redraws the whole head for each mouth position. Its head pose differs from the original sly-smile portrait, so the five generated frames form the `talk` mood together, and its rest frame is the still image. The original portrait isn't used.
  - **Lip-sync.** The deck doesn't replay PixelLab's exact timing. While text types out, it alternates mouth frames, which is the usual retro-game talking effect.

- **No avatar file, no feature.** Without `data/avatar.bin`, the deck behaves exactly as today. There is no committed placeholder character, which keeps the repo free of anyone's art and keeps the feature opt-in.
- **S3 only.** The CYD sits at 96.5 % flash. Each mood costs 16.9 KB, and the S3 has room for many.

## The converter: `tools/gen_avatar.py`

**Input:** a git-ignored folder `avatar-photos/` with the images and a manifest, `avatar-photos/avatar.json`:

```json
{
  "name": "ZERO TWO",
  "moods": {
    "smug":  {"file": "zero-two.png",        "crop": [800, 14, 0], "colors": 0,  "boost": false},
    "talk":  {"frames": ["talk-rest.png", "talk-open.png", "talk-round.png", "talk-teeth.png", "talk-smile.png"]},
    "happy": {"file": "happy-92.png"}
  }
}
```

**Fields:**
- A mood has either `file` (one frame) or `frames` (a list, frame 0 = the closed-mouth rest frame, 1–8 frames).
- An image that is already exactly 92×92 is used **as is**: no crop, no filters. This is how portraits made elsewhere get in. During brainstorming, PixelLab's image-to-pixel-art (`image_to_pixelart`, faithful mode, strength 200) gave the cleanest result for the two anime screenshots. Its free plan runs one job at a time and charges per image, so it's a manual step, not part of the build.
- `crop` is `[size, x, y]`: a square crop in source pixels.
- `colors`:
  - `0` keeps the downscaled colours as they are. Use it for sources that are already pixel art.
  - `N` gives the image its own palette of N colours with no dithering. Use it for anime screenshots: it produces flat colour areas instead of a blurry mini-photo.
- `boost` adds a mild saturation and contrast lift (`eq=saturation=1.35:contrast=1.12`) for washed-out frames.

**Pipeline per mood (ffmpeg, as in `gen_montage.py`):**
1. Crop.
2. Optional boost.
3. One `erosion` pass to thicken the dark line art, so it survives the downscale.
4. `scale=92:92:flags=area`.
5. Optional `palettegen` and `paletteuse=dither=none`.
6. Convert to RGB565.

These settings were tuned on the three images during brainstorming.

**Output:** `data/avatar.bin` (git-ignored), plus `avatar-preview.png` next to the manifest. The preview shows the moods at 4× so the owner can check them before flashing.

**Validation:** the tool exits with a clear message for each of these:
- a missing file;
- a crop outside the image;
- an unknown mood name;
- a mood with both or neither of `file` and `frames`, or more than 8 frames;
- no moods at all;
- a name longer than 12 characters.

## `data/avatar.bin` format

Header fields are single bytes; frames are big-endian RGB565.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `AVT1` |
| 4 | 1 | mood count `n` (1–8) |
| 5 | 1 | reserved, 0 |
| 6 | 12 | name, NUL-padded ASCII (e.g. `ZERO TWO`) |
| 18 | n × 2 | per mood: id (0 smug, 1 talk, 2 happy; others reserved), frame count (1–8) |
| 18 + 2n | sum of frames × 16 928 | 92×92 RGB565 frames, **high byte first** (the byte order `pushImage` sends as is, like `theme::be`), mood by mood in entry order |

The size today: 7 frames, about 118 KB.

**Build integration:**
- A pre-build script, `tools/avatar_clip.py` (like `montage_clip.py`), writes an empty file (`AVT1`, count 0) when there's no `data/avatar.bin`. Fresh clones and CI then still build.
- Only `env:s3` embeds `data/avatar.bin`. It gets its own `board_build.embed_files` list: the common list plus this file. The CYD builds don't change.

## On the deck

**When she appears** (canned lines; the variety stays small on purpose for v1):
1. **Boot greeting.** On the first full deck draw after boot, with mood `happy`. Example: "Darling~ I'm here."
2. **Track change.** After a new track's deck redraw, at most once every **5 minutes**, with a 1-in-3 chance per eligible change. Lines come from a small template table that can use `{T}` (title), `{A}` (artist) and `{P}` (the walking Pokémon). The table has around 12 lines, each tagged with a mood. Examples:
   - `smug`: "Darling, {A} again?"
   - `happy`: "Ooh, I like this one~"
   - `talk`: "A wild {P}! Bet it can't keep up with us."

She never appears on a status screen (offline or nothing playing). If one comes up during her scene, the scene is cancelled.

**Her scene (about 5.5 s on the UI loop; no new task):**
1. **Slide-in (~240 ms, 6 steps).** The portrait enters the art box from the left. It's drawn through a TFT_eSPI viewport clipped to the box, so the part still outside stays hidden. Rows are copied from the embedded flash data through a 184-byte row buffer (the embedded file may not be 2-byte aligned) and pushed one row at a time. Only the visible columns are pushed, so no viewport is needed.
2. **Name tag and line.** The pink name tag (`name` from the file) goes on the dialogue box's top-left corner. Her line types out in the dialogue box at ~40 characters per second, replacing the lyric. It wraps over the existing two lines, and anything too long ends in "...".
   - **Talking.** If the mood has more than one frame, its frames alternate while the line types. A new frame is pushed every ~100 ms: an open shape after each letter, the rest frame on spaces and punctuation. At the end it settles on the rest frame.
3. **Hold.** The full line stays up for at least 3 s.
4. **Exit.** The album art goes back into the box (`pushArtIfValid`, or the box background if no art has arrived yet). The name tag is erased and the dialogue box goes back to lyrics (`ui::drawNow` already forces a lyric repaint).

While she's on screen:
- the progress bar, the walker, the CD and the top strip keep updating as usual;
- the network task's art arrival (`takeArt`) is consumed, but the push waits until her scene ends.

## Modules

Pure modules, each host-tested:
- **`util/avatarbin`:** parses and validates the blob (magic, count, size, mood lookup, name). Returns "off" for an empty or bad blob.
- **`util/avatarlines`:** picks a line. The inputs are the event (boot or track), the time since her last appearance, a random value, and the title, artist and Pokémon name. The output is "no scene" or a mood plus the filled text, using the same fill and truncation rules as `ui/lyricmsg`.
- **`util/avatarscene`:** the timeline. Given ms since the scene started, the line and the mood's frame count, it returns:
  - the slide offset;
  - the number of characters typed;
  - the frame to show;
  - the phase (`Slide`, `Type`, `Hold`, `Done`).

Device-only modules:
- **`ui/avatar`:** draws a frame at an offset inside the art box (via a viewport), plus the name tag. Mouth changes repush the whole 92×92 frame (17 KB at 40 MHz SPI, about 7 ms).
- **`main.cpp`:**
  - the boot and track hooks;
  - the scene state while she's on screen;
  - the art-push deferral.

## Failure handling

- **No file, empty file or bad file:** feature off, with one `[avatar] off: <reason>` log line at boot.
- **A missing mood:** falls back to the first mood in the file.
- **Track changes during her scene:** the scene ends at once and the normal track redraw runs, so the new song is never hidden behind her.

## Testing

**Host tests:** `avatarbin`, `avatarlines` and `avatarscene`. Each test suite uses made-up names and lines only.

**Converter:** a run on a tiny generated test image checks:
- the header bytes;
- the blob size;
- that each validation error fires.

**On the device (S3):**
- the boot greeting shows;
- a track change shows her at most once in 5 min;
- the art comes back afterwards;
- lyrics resume;
- `[mem]` is unchanged;
- with `data/avatar.bin` deleted, the deck is exactly as before.

## Out of scope (later)

- Blinking.
- Exact PixelLab lip-sync timing.
- More moods and more lines.
- `POST /say` and the Claude/Shortcut assistant (step 2).
- The CYD companion showing her.
- Voice.
