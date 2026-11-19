# The Suki drive screen (v19, full screen) on the black deck

A fourth GAUGES screen, **Ride**, next to Suki (the film monitor V3b, #71), Drive and Trip. It began as
the research session's approved mock "R1 refined" (layout C, R1 ribbon style) and became João's
full-screen mock v19: no black bands and no stretching, the film's left side (bezel window, silver
ribbons) replaced by a panel in the theme's own colours. Every deck theme, day and night. Picked like
the others while parked: `GAUGES SCREEN ride` on the serial line, RIDE on the phone page, or
`GET /gauges?screen=ride` (also `4`).

## What it shows

- **The girl** on the right, from 19 real cels, the art scaled evenly to the full 240 px (x 0.6575 of
  the 640 x 365 frames, right edge kept). Only her strip is stored: x 160 to 320.
- **The left panel** (x 0 to 178, then a 30 px fade into her): the theme's DIM at 10 %, a touch lighter
  at the top and darker at the bottom. Every element is a card: dark glass, a 0.75 px border in the
  theme's DIM, a short MID tick at its top left. No grey.
- **SUKI**: the film logo's letters, tinted in the theme's TEXT over a DIM shadow. On the right of the
  logo row a tag: `RIDE` (MID), `DEMO` with `GAUGES DEMO` and `NO LINK` without the link (ALERT).
- **Speed card**: the true OBD speed (PID 0D, no dial factor) in three seven-segment cells over a faint
  888, `---` without a value; `SPEED KM/H` above it. On the right `RPM`: the rpm in four smaller
  seven-segment cells (14 x 26 px) over a faint 8888, and under them a tach on the speed's baseline: 12
  segments over 0 to 7000 rpm, rising in height left to right, each in its band's colour (the theme
  under 5000, bright from 5000, the alert colour from 6500), unlit ones faint, and a bright cap on the
  highest of the last ~3 s (13 samples, one every 250 ms). João found the old 7 px number too small
  (2026-11-19).
- **Four equal tiles**: KM/L now (fueleco), IAT (the City's 68), TRIP km (stays shown without the link)
  and GEAR: the estimated gear and seven pips, `EST` beside them. The estimate is rpm / speed matched to
  seven bands (125 rpm per km/h in 1st down to ~20 in 7th, geometric steps); `-` under 10 km/h or
  outside every band. **The bands are guesses until a drive log calibrates them.** Units sit on the
  label's line so the value has the tile's width.
- **Three identical bars**: PEDAL (throttle %), COOL (60 to 130 C scale), BATT (11 to 15 V scale), each
  with its label, the value on the right and a 16-pill track under them. The lit pills take the
  value's zone colour; unlit pills a faint tint of their own zone. Zones: COOL cold under 77, warn from
  95, alert from 105; BATT alert under 12, warn over 14.7; PEDAL warn from 85 %.
- **Alert**: only COOL and BATT, from `drivedash::Alerts` (coolant over 105 C until under 100; volts under
  12.0 V for 60 s with the engine running, until 12.2 V). The bar's border, value and unit turn the
  theme's alert colour and its label sits in an alert box; border and box flash white at 3 Hz. The
  alert colour is white in the red, orange and pink themes (termpal's rule), red elsewhere.
- **Link lost**: every value `--`, the whole screen at 65 %, the girl on the calm cel, the tag `NO LINK`.
- **Ride without its art** (a build from a fresh checkout: CI's stand-in): the screen falls back to
  Suki / the street look, but `/gauges` still reports `ride`.

## The girl

Cel by true speed (the mock's `celIdx`): under 3 km/h, no speed or no link: calm cel 7, still. Under
80: five calm cels ping-pong at 4 a second. 80 to 110: calm cels with the open mouth. Over 110: the
teeth cels shuffled 8 a second with a 1 px sideways shake.

## Colours

The deck's theme is the yellow deck's INSIDE light swatch (`karaoke::themeLight()`, termpal's nine
themes, -1 green) with day or night (`theme::isNightActive()`). The yellow deck sends that swatch with
its link check every 5 s (`GET /link?light=N`) and with the karaoke keep-alive, so the Ride screen
follows its colour within 5 s, KARAOKE on or off. With the light off the screen keeps the last colour;
after a boot it is green until the first link check. The interface uses termpal's TEXT, MID, DIM and ALERT for
that theme; text in the alert box is dark or white, whichever reads better.

Ride wears every theme's **night** palette day and night (the art at 78 %), except pink, whose day
look is the vivid one by day: João's pick on the deck, 2026-11-19. Pink at night is Ride's own
FF3CA0 / E6288C / C81E78, pinker than termpal's red-leaning FF0040. White is termpal's night white
with the art undimmed and lifted 35 % toward white, so it doesn't read grey. termpal itself is
unchanged: the other screens keep its day and night.

The art is stored once, as 8-bit indices into one 256-colour palette (the film's own colours). On a
theme or day/night change the deck recolours the palette, not the pixels (the mock's `recolour()`):
every colour with saturation over 0.12 that isn't skin gets a new hue: the purple family (225 to 290)
near the theme's TEXT hue, every other hue around the theme's MID hue with half its spread. Skin
(hue 8 to 55, lightness over 0.72), black, white and grey stay. The white theme cuts saturation to
6 %. Night dims the art to 78 %.

## On the deck

- `util/car/sukiride` (PURE, host-tested): the file reader, the recolour, the scene, and `compose()`,
  which draws any rectangle of the screen into big-endian RGB565. Region keys say what changed.
- `black/ui/sukirideview`: one 34,560-pixel buffer in PSRAM (the art's region, x 176 to 320, the
  biggest). A whole redraw goes in strips 108 rows tall; after that only regions whose key changed
  are composed and pushed. The art region (~69 KB) is the only big push, 4 to 8 times a second.
- The art file `data/sukiride.bin` (~0.74 MB, format SKR2, `sukiride.h`) is **linked into the app**
  (`board_build.embed_files`), not the media pack: a firmware upload (USB or WiFi) carries it, and no
  `uploadmedia` is needed.
- The art is the film's and the repo is public: the layers and `data/sukiride.bin` are git-ignored. CI
  links `media_sizes.py`'s zero-filled stand-in of the same size, which reads as no art, and the Ride
  screen then falls back to the others.

## Making the art

```bash
python tools/gen_suki_ride.py --assets <folder with suki-layer-backdrop.png and suki-layer-cel-*.png> [--font Orbitron.ttf]
python tools/media_sizes.py record     # only if the size changed: CI's flash check
python tools/suki_ride_preview.py --out suki-ride-preview.png   # every state and theme on the PC
```

The layers come from the research session (cut from the making-of video's frames by its `mk12b.py`).
Fonts: the mock's Orbitron at weight 800 (`Orbitron[wght].ttf`, SIL OFL, google/fonts `ofl/orbitron`,
downloaded with João's OK; only the generated glyphs travel, inside the git-ignored art file), else
Century Gothic Bold from Windows. Labels and units 7 px, the tiles' values 10, the bars' values 12, the
gear 16. The interactive mocks (v18 full screen, v19 left panel) are in the research notes, not here:
they carry the film's art.

## Car tests (João, read-only)

1. Ask the Vgate for PID 01 A4 (transmission actual gear) and read the supported-PID bitmaps (01 00 .. 01 A0).
2. Watch the CAN bus for the transmission frames: are S mode and the paddle shifts visible?
3. Compare the OBD speed with the phone's GPS at 60, 80 and 100 km/h (the dial reads 5 to 8 % high).
4. Log rpm and speed on a drive, to calibrate the seven gear bands.
5. Readability at a glance in daylight and at night, and the overheat pulse in the red, orange and pink themes (white there).
