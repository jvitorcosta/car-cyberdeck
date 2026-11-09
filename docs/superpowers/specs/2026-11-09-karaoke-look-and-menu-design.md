# The karaoke look (black deck) and the Menu's top (yellow deck)

Two screen redesigns on branch `feat/black-karaoke`, on top of the karaoke work in
`2026-11-09-black-deck-karaoke-design.md`. Picked from mockups on 2026-11-09 (the brainstorm's
`karaoke-middle.html`, option M1, and `yellow-menu.html`, option 2).

## Why

After the first car look at the black deck's karaoke screen:

- The colours don't read well. The dialogue box's unsung words are low contrast, and the look doesn't
  suit reading at a glance in a car.
- Lines get cut. At 2x the sung line fits about 18 characters a row, two rows at most.
- The walking Pokémon is too big.

The yellow deck's Menu lost its now-playing band with the karaoke move. That leaves the clock band
113 rows tall with only the clock, the date and the trip time, so there's empty space above and below
them.

## Part 1: the black deck's karaoke screen ("M1: night drive, tinted by the light")

### What it looks like

- **No frame or box.** The whole 320x240 screen holds text, over a dark background in both modes.
  The user asked whether black or white is better in the car. The answer is dark: a white screen
  glares and reflects in the windshield.
- **Top:** the song title, the artist under it, and a thin rounded progress bar.
- **Lyrics:**
  - The line just sung, one row, smaller.
  - The sung line, big.
  - The next line, one row, smaller.
- **Colour from the car's inside light,** as on the old yellow karaoke. The INSIDE light's swatch
  picks a hue, and that hue comes at three brightnesses: TEXT, MID and DIM, from termpal's table
  (`src/util/yellow/termpal.cpp`).
  - Day uses the pale tints and night uses the vivid ones: termpal's `rgb(r, theme, night)` for
    each mode.
  - Night is the black deck's own day/night (`daynight::isNight`, 18:00 to 06:00, as
    `theme::setNight` already uses).
- **Colour roles.** In the mockup, `tint(c, a, base)` mixes colour c into base at strength a.

  | Role | Day | Night |
  |---|---|---|
  | background | tint(DIM, .10, #0E1218) | tint(DIM, .07, #030406) |
  | song title | TEXT | TEXT |
  | artist | MID | MID |
  | bar played | TEXT | TEXT |
  | bar rest | tint(DIM, .30, background) | tint(DIM, .30, background) |
  | sung words (lit) | #FFFFFF | #EDE6D6 |
  | unsung words of the sung line | tint(DIM, .45, grey) | tint(DIM, .45, grey) |
  | line just sung | tint(DIM, .55, grey) | tint(DIM, .55, grey) |
  | next line | tint(MID, .70, grey) | tint(MID, .70, grey) |

  grey is #8A929C by day and #5A6068 at night. These are the mockup's values; the builder may nudge
  them on the panel. The rule that stays: lit words are clearly the brightest thing on screen, and
  unsung words are clearly dimmer but still readable.
- **The sweep and the walker work as today:**
  - The lit colour sweeps across the sung line in time.
  - The walking Pokémon steps along just above the sweep.
  - The Pokémon hops to a new line.
- **The walker stays above the first row** while later rows are sung, as it does today.
  - Its x is the whole line's progress mapped onto row 1's width (`karaokeWalkerOffset`).
  - **Our call:** the mockup moved it down onto rows 2 and 3, sitting on the text above. On the
    panel that would mean repainting partly swept glyphs under the sprite every frame, and leaving
    a 16 px gap between rows that the 3-row layout can't spare.
- **The walker is about half its current size,** about 32x16 instead of up to 64x32, drawn on the
  background colour. It keeps its own sprite colours.
- **Message screens** keep their wording, in this look. These are the no-lyrics wait and similar
  screens; the lyric messages come from ui/lyricmsg.
- **The camera warning band** still clips the screen as it does now (`setKaraokeClip`).

### Text that fits instead of getting cut

- The sung line wraps to up to **3 rows** at its big size.
- If it needs more than 3 rows, it is drawn one size smaller, again up to 3 rows.
- Only if it still doesn't fit is the end cut with "...".
- The old yellow karaoke did the same: its BIG face, falling back to TITLE.
- The line just sung and the next line stay one row each, cut with "..." when they don't fit.

### The font

- The look calls for a **bold, plain sans-serif face made for legibility**. The mockup used Atkinson
  Hyperlegible Bold: about 33 px for the sung line, 19 to 21 px for the others and 16 to 20 px for
  the header, all at 1.5x on screen. On the panel that's roughly 22 px, 13 to 14 px and 11 to 13 px.
- It replaces today's scaled-up font 2.
- **Builder's call:** where the face comes from and its exact sizes. Options include a bitmap font
  generated into the firmware, a TFT_eSPI smooth font from the media partition, or TFT_eSPI's
  built-in Font 4. The face must handle what glyphrun handles today: Portuguese and other accented
  Latin letters, CJK and emoji. CJK and emoji can keep their current fallbacks next to the new face.

### Where the light colour comes from

- The yellow deck sends its INSIDE light's swatch with the karaoke keep-alive: `GET
  /karaoke?on=1&light=N`. N is the ledlook swatch index, 0 to 7 (red, orange, pink, purple, blue,
  green, white, yellow).
- That swatch is what the light shows: music mode's while music mode drives the lights.
- When the inside light is off or has no link, it sends `light=-1`.
- The ask goes out every 10 s (MUSIC_KEEP_MS). A colour change also sends one at once, so the screen
  follows within a moment rather than 10 s.
- The black deck keeps the last swatch it got.
  - Before any swatch, or with -1, it uses the **white** theme. **Our call:** neutral, not the
    yellow deck's green default.
  - A change of swatch redraws the karaoke screen in full.
- The parameter is part of the signed request, like `on`.

## Part 2: the yellow deck's Menu ("2: bigger buttons, bigger lights")

The screen is 320x240, top to bottom. The terminal font and termpal colours stay; only sizes and
positions change.

- **Header,** rows 0 to 20: unchanged. HONDA CITY TOURING, the PAD battery and the rule.
- **Clock row,** to a rule at about row 52:
  - Left: the clock in TITLE, then the date in SMALL MID to its right.
  - Right, right-aligned: TRIP in SMALL MID, then the trip time in TITLE.
- **Lights band,** to a rule at about row 101. A MID vertical rule splits it at 159. Each half has:
  - A **colour block** about 40x34 on the left, filled with the light's colour.
  - INSIDE or OUTSIDE in SMALL MID to its right.
  - Under that, what the light shows ("ORANGE 60%") in TITLE, or BODY if TITLE can't fit the widest
    value. Builder's call, checked by test_termfont against the widest name with "100%".
  - The states work as today:
    - Off: an outlined DIM block and OFF in DIM.
    - No link: "NO LINK" in ALERT and no block.
    - Music mode: the song's colour, as today.
- **Prompt line,** as today but moved up under the lights band: messages, music mode's line and the
  cursor.
- **The three items,** from about row 128 to the bottom, about 106 rows tall instead of 48.
  - Labels stay in BODY, centred vertically in the taller buttons.
  - The ON/OFF tags stay bottom right.
  - The deckmenu rects change. A tap anywhere in a tall button hits it, and the D-pad works as
    before.
- The lights band stays display-only, never a hit, as it is today.
- The exact rows are the builder's, kept consistent in `termlayout.h` and `deckmenu.h`.
  test_termfont and test_deckmenu check that everything fits and the hit areas match.
- The Lights and Presets screens don't change.
- No new asks and no heap cost. The yellow deck's heap is tight, about 32 to 35 KB with WiFi and asks
  running.

## Not in scope

- The black deck's normal screens, other than sharing the smaller walker code if that's simpler.
- Night backlight changes.
- A now-playing line on the yellow deck: option 3 was turned down because of its heap cost.

## Done looks like

- Host tests pass: `pio test -e native`.
  - New or changed tests cover: the 3-row fit and the one-size-smaller fallback, the swatch-to-
    palette mapping, the `light=` parameter in the request and on the black deck, and the Menu's new
    rects and fits.
- Both firmwares build.
- Looked at on the panels:
  - The black deck in day and night with at least two light colours, including a long line.
  - The yellow Menu.
- Flashing: the yellow deck over USB is approved. The black deck needs the user's OK each time.
