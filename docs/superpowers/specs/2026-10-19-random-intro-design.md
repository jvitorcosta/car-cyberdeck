# A shorter boot with a random opening

**Date:** 2026-10-19
**Status:** draft for review
**Build:** `s3` (the new deck). The old deck (`cyd`) keeps today's boot: its flash is 96.6 % full.
**Builds on:** `2026-10-09-boot-scene-design.md` (the night drive), `2026-10-09-boot-scene-v3-design.md` (brake and montage), the intro video (`ui/intro`, `tools/gen_intro.py`) and the BIOS log (`util/bios`).

## Goal

The new deck's boot takes ~39 s today: the whole kiss video (23 s), one greeting over the silver Honda's night drive, the photo montage and the Honda logo card. The owner wants it shorter and varied:
- Each boot plays **one opening, picked at random**, then a **shared tail**: a shorter montage and a "POWERED BY HONDA" end card.
- Power-on to the music screen takes **15 to 25 s**, depending on the opening.
- Everything already built is reused: the video, the night drive, the greetings, the BIOS log, the montage and its sounds.

## Decisions taken while brainstorming

- **Openings:** A. the kiss (video), B. pixel Honda with "bem vindo moreno", C. pixel McQueen with "eu sou a velocidade".
- **Kiss cut:** the walk-in and the kiss (~10 s), without the empty wall or the loop's tail.
- **McQueen:** new pixel art drawn to the Honda sprite's size and style, git-ignored like Sonic (Disney/Pixar's character).
- **Montage:** shorter (~5 s of photos) by quicker cuts, keeping the opener and finale sounds.
- **The boot log everywhere:** the night drive's on-board terminal also runs over the kiss video.
- **Approach:** openings as interchangeable acts in the existing boot scene. Rejected: pre-rendering every opening as a video (loses the live log and sound-reactive lights, ~2 MB of flash each) and skipping parts of today's sequence (can't give McQueen his own car).

## The sequence

| Opening | Then | Estimated total |
|---|---|---|
| A. Kiss | walk-in + kiss video (~10 s), video sound only | ~19 s |
| B. Honda | night drive, "bem vindo moreno" (~7 s with the brake) | ~16 s |
| C. McQueen | night drive as McQueen, "eu sou a velocidade" (~6 s with the brake) | ~15 s |

**The shared tail (~7.5 s):** the montage's photo shots and headlight detail (~5 s), the Honda logo card with "POWERED BY HONDA" (~2.5 s), the fade to black, then the deck.

WiFi and the Spotify login run underneath every opening, as today.

## Components

### 1. The picker: `util/intropick` (pure, host-tested)
- `Opening pick(uint32_t rnd, Opening last, bool haveKiss, bool haveMcQueen)`: one of `KISS`, `HONDA`, `MCQUEEN` at random, never `last`, and never an opening whose files the build lacks.
- The Honda opening is always available, so there is always a pick. With one available opening, it repeats.
- `last` is kept on the deck (NVS, namespace `boot`, key `last`) and written once the pick is made.
- The boot log line: `[boot] opening: kiss` (or `honda`, `mcqueen`).

### 2. Opening A: the kiss video
- `tools/gen_intro.py` gets a default cut of the walk-in and the kiss (the exact `--seg` seconds are chosen on the clip and written into the tool's header and the README). `data/intro.bin` and `.pcm` are rebuilt.
- **The terminal over the video.** `ui/intro` paints the on-board terminal into each frame as JPEGDEC decodes it: in the draw callback, the pixels of each block that fall inside the terminal's rectangle are darkened (a fixed ~60 % dark panel) and the terminal's glyphs are drawn over them, before `pushImage`. It never flickers and costs no extra full-screen pass.
- The terminal shows the same lines as in the night drive (the BIOS script, then WI-FI, SPOTIFY). The terminal state (`bios::Term`) and its glyph drawing move out of `bootscene.cpp` into a small shared `ui/bootterm` that both scenes use.
- **If WiFi isn't up when the video ends:** the last frame stays on screen, and the terminal keeps updating (`WI-FI ... RETRYING`), redrawn into that frame. Once online, the boot scene starts directly at the montage.
- **The cost:** the video runs at ~52 ms of work per 50 ms frame and drops ~20 of 462 frames today. The overlay adds a little more work per frame. It's measured on the deck (the `[intro]` log line counts frames shown), and the panel shrinks or the overlay drops to every other frame if it costs more than ~5 % of frames.

### 3. Openings B and C: the night drive with a chosen car
- `bootscene::start` takes the opening: the car sprite to draw (Honda or McQueen) and whether to drive at all (the kiss opening skips straight to the montage).
- `greeting` plays the opening's clip instead of today's coin flip (`util/greetpick` goes): B plays `data/greeting.pcm`, C plays `data/greeting_alt.pcm`, A plays none (the video has its own sound).
- The car's lights, bob, sway, wheels and brake work for both cars. The light positions (headlight, taillight, underglow) are per sprite, stored with it.

### 4. Pixel McQueen
- A side view facing right at the Honda sprite's size (200 × 66 px, `ui/car_sprite`): red body, "95", the lightning bolt, the windshield eyes. Wheels aren't in the sprite: they're drawn in code so they spin, at the sprite's own wheel centres.
- Drawn with the PixelLab tools, then packed by a new `tools/gen_mcqueen_sprite.py` (PNG in, standard library only) into git-ignored `data/mcqueen.bin`: a small header (size, wheel centres, light positions) and RGB565 pixels with a transparent key, embedded in the S3 build (an empty file when absent, like the other clips).
- The Honda stays in the committed `car_sprite.inc`; the scene reads the car through one small interface (pixel, size, wheel and light positions) for either.
- Without the file, opening C isn't available (the picker skips it).
- `CREDITS.md` and the README note that it's a fan sprite of a Disney/Pixar character, kept out of the repo.

### 5. The shorter montage and the end card
- `tools/gen_montage.py`: the three photo shots and the headlight detail get fewer frames (~5 s in total at 10 fps, with the same 0.3 s crossfades). The end card adds "POWERED BY HONDA" under the logo, in the frame's red. The owner's photos are in `montage-photos/`, so `data/montage.bin` is rebuilt here.
- The opener and finale sounds keep their timing rules (`bootanim::finaleStartFrame`, `openerFits`), which already adapt to the montage's length.

### 6. The boot flow (`main.cpp`, S3 branch)
1. Pick the opening; start WiFi and the network task as today.
2. A: `intro::start` (with the terminal), then `intro::wait`, which also waits for WiFi on the last frame. B/C: no video.
3. `bootscene::start(opening)`: B/C drive with their greeting, brake, montage; A goes straight to the montage.
4. `bootscene::waitDone`, then the deck.

## Error handling
- **Missing files:** the picker only offers openings the build has (kiss: `intro.bin`; McQueen: `mcqueen.bin`). The Honda opening always works.
- **No WiFi:** every opening waits for it with the terminal on screen (the car cruises; the kiss holds its last frame), as the car does today.
- **Memory:** the terminal is drawn into a 206 × 60 overlay sprite (~25 KB) that lives only while the video plays (the S3 has heap to spare at that point; it's freed before the boot scene), and composited inside JPEGDEC's draw callback on the blocks it already has. The McQueen sprite is read in place from flash.

## Testing
- **Host tests:** the picker (random, never last, skips missing openings, single option), the greeting for each opening, the montage tool's new frame counts and the end-card text (Python), the McQueen packer (Python), and the terminal compositing on one block (pure: a block in, darkened panel and glyph pixels out).
- **On the deck:** each opening once (a build flag, `-D INTRO_FORCE=kiss|honda|mcqueen`, forces the pick for testing), the frame count with the overlay, the boot time from power-on to the deck for each opening, and a boot with WiFi off (the kiss holds with `RETRYING`, then continues when it connects).

## Out of scope
- The old deck's boot.
- New greetings or new openings beyond these three (the picker makes adding one cheap later).
