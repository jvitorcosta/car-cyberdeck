# Shadow update screen

Date: 2026-10-22. Owner: João. Replaces the S3's firmware-update screen (`src/ui/otascreen.*`).

> **Revision 2026-10-23 (owner, after seeing it on the deck): Green Hill Zone, not Star Light Zone**,
> which was too dark on the screen. `tools/gen_backdrop.py` renders the GHZ background (window
> x 1100..1419, y 16..255: clouds, hills, the waterfall, the lake) into `data/backdrop.bin`; the rest
> of this design stands. Star Light Zone below is the original choice.

## Why

The deck updates over WiFi (`net/ota`, PR #9). Today the screen is a battle box with an EXP bar:
it works, but it looks like a placeholder next to the rest of the deck (the kiss intro, the
montage, Sonic waiting on the sunset). The owner wants the update to feel like part of the game,
"simple but beautiful", and still say at a glance: it's working, how far, don't unplug it.

## What (agreed with the owner, mockup "1. Full scene", `.superpowers/brainstorm/`)

- **Background:** the real **Star Light Zone** backdrop from Sonic the Hedgehog (Mega Drive,
  1991) fills the 320 x 240 screen: starry sky, the band of distant blue city lights, the brick
  buildings with green girders. Night only (it's a night stage).
- **Top strip:** a thin dark strip (about 20 px) with `FIRMWARE UPDATE` on the left and the
  percent on the right in Shadow red.
- **The rail is the progress bar:** across the bottom, over the buildings; dark track, light
  border, the fill red to orange. Under it, `Keep the deck powered`.
- **Shadow** hovers on his jet shoes over the fill's tip, moving right as the percent grows, and
  runs through his running frames all the time (the screen looks alive while the upload stalls).
  Small cyan jet glows under his shoes.
- **At 100%:** a short red flash across the screen ("Chaos Control"), then the reboot into the
  new firmware.
- **If the update fails** (WiFi drops, bad image, the PC stops): Shadow stops, the rail turns
  grey, the strip says `Update failed`, held ~3 s; then the deck goes back to what it showed, as
  today (the running firmware stays).

## Non-goals

- No day version, no sounds, no other characters.
- The CYD decks (companion, ledpanel) keep whatever they have; this is the S3 only.
- No change to how updates are sent or checked (`net/ota`'s SHA-256 + boot guard stay as they are).

## Anti-goals

- Anything that slows or endangers the update: the screen must not hold the OTA task, must not
  allocate big buffers from internal RAM while TLS/OTA need it, and a draw must never stall the
  upload (the UI loop and the OTA task are separate; keep it that way).
- SEGA's art in the repository. The repo is public: art is rebuilt locally and git-ignored, like
  Sonic (`tools/gen_idle_sprite.py`, CREDITS.md).
- A build or CI that breaks without the art: without the files the deck keeps today's battle-box
  screen.

## Constraints and facts from the code

- `main.cpp` draws the update screen while `ota::active()`; today it redraws only when the percent
  changes. Animation needs a draw every pass (~20 ms loop), with only the parts that change
  repainted (Shadow's box, the fill, the percent).
- `ArduinoOTA` calls `onEnd` and then restarts at once (`_rebootOnSuccess` is true). The 100%
  flash needs `net/ota` to tell the UI "done" and wait for it (e.g. `setRebootOnSuccess(false)`,
  set a done state, give the UI ~600 ms, then `esp_restart()` itself).
- A failure today just sets the progress back to -1. The failed state above needs `net/ota` to
  keep a "failed" state for the hold time (or expose it), then clear.
- Media on the S3 live in the `media` flash partition (`core/media`, `tools/pack_media.py`),
  flashed over USB with `pio run -e s3 -t uploadmedia`; OTA updates only the app. So the new art
  arrives with one USB media flash, and the screen shows from the first OTA after that.
- Reading the media partition (memory-mapped flash) while OTA writes the other app slot is fine
  but slower; draw the full background once, then only small regions.
- The S3 has 8 MB PSRAM; a 320 x 240 RGB565 frame is 150 KB.

## The art

- **Star Light Zone background** (spike code: `tools/slz_spike.py`, committed with this design as the starting point): rebuilt from the pinned s1disasm disassembly (same repo and
  commit as Sonic: `sonicretro/s1disasm` @ `15dfa3a6...`): `artnem/8x8 - SLZ.nem` (Nemesis),
  `map16/SLZ.eni` (Enigma), `map256/SLZ.kos` (Kosinski), `levels/slzbg.bin` (layout, 24 x 5
  chunks), `palette/Star Light Zone.bin` (+ `palette/Sonic.bin` for line 0). A spike on
  2026-10-22 decoded all three formats and rendered the layout correctly; the chosen window is
  x 256..575, y 660..899 of the full 6144 x 1280 render (sky, city lights, building tops), at 1x.
  The tool writes one 320 x 240 image file into `data/` (git-ignored), packed into the media
  partition.
- **Shadow:** his running frames from **Sonic Battle** (GBA, 2003), the game with his official
  2D sprites. No disassembly can be fetched automatically (the sprite archive refuses scripted
  downloads), so the owner downloads his sprite sheet once into a git-ignored folder and a tool
  cuts the running frames out of it into a small frame file in `data/`. Frame positions on the
  sheet are found when the sheet is in hand.

## Layout (from the agreed mockup, in screen pixels)

- Top strip: y 0..19, black at ~80% over the backdrop; `FIRMWARE UPDATE` left (x 8), the percent
  right (x 312, right-aligned), font 2; percent in Shadow red (#FF5A2A), the rest white with a
  1 px black shadow.
- Rail: x 16..303, y 200..209 (10 px): 2 px light border (#E8E8F0) around a dark track
  (#101018); the fill inside it, red #C01818 to orange #FF5A2A left to right.
- Shadow: feet ~8 px above the rail's top edge (hovering), his centre over the fill's tip, clamped
  so he stays on screen at 0% and 100%; two small cyan glows (#77FFFF fading out) under his shoes.
- `Keep the deck powered`: x 16, y 216, font 2, white with shadow.
- Failed: the fill and border grey (#808080), the strip's text `Update failed`, Shadow on his
  standing frame.

## Decisions (builder check, 2026-10-22; my calls unless marked as the owner's)

- **The version shown:** ArduinoOTA never sends the incoming version, so the strip says
  `FIRMWARE UPDATE`, not a number (my call; the mockup's `v1.5` was illustrative).
- **Done and failed states in `net/ota` apply with or without the art** (my call): `net/ota` stops
  rebooting from inside ArduinoOTA (`setRebootOnSuccess(false)`), exposes a state Idle /
  Receiving(pct) / Done / Failed, and restarts itself ~600 ms after Done. The old battle-box
  screen (no art) keeps its look and simply shows 100% for those 600 ms.
- **Failed is not "active"** (my call): the Spotify network task resumes at once; the screen
  holds `Update failed` for ~3 s from the UI loop's own clock; a new upload during the hold is
  accepted and takes over the screen.
- **The decoders exist:** the 2026-10-22 spike (`slz_spike.py`, Nemesis + Enigma + Kosinski +
  the S1 layout/chunk/block render, ~200 lines of Python) rendered the background correctly; it
  becomes the base of `tools/gen_slz_backdrop.py`, with host tests for the three decoders on
  small known inputs.
- **Two stages** (my call): stage 1 ships the backdrop, the rail, the states and `net/ota`, with
  Shadow drawn from a placeholder (or not at all) when his frame file is missing; stage 2 adds the
  Sonic Battle frames once the owner has the sheet.

## Left to the builder

- File formats for the two data files (follow `IDL1` / `util/idlesprite` for the frames; the
  background can be raw RGB565 with a small header).
- Exact positions, sizes and colours (the mockup is the guide), Shadow's frame rate, the hover
  bob, how far ahead of the fill he rides.
- Where the pure logic lives (Shadow's x for a percent, the frame for a time, the flash and
  failed timings): a host-tested helper like the repo's other `util/` modules.
- Whether to keep a 150 KB PSRAM copy of the background or read the media partition directly.
- Rebase onto PR #11 (`refactor/split-main`) if it merges first: it moves the update call out of
  `main.cpp`.
