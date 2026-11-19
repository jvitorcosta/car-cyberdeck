# The Suki drive screen (R1.1) on the black deck

A fourth GAUGES screen, **Ride**, next to Suki (the film monitor V3b, #71), Drive and Trip. It is the
research session's approved mock "R1 refined" (layout C, R1 ribbon style), in every deck theme day and
night. Picked like the others while parked: `GAUGES SCREEN ride` on the serial line, RIDE on the phone
page, or `GET /gauges?screen=ride` (also `4`).

## What it shows

- **The art**: the film monitor's backdrop (logo, pink curve, bezel, three silver ribbons) at half size,
  320 x 182, letterboxed 29 px above and below. The girl on the right is animated from 19 real cels.
- **Speed**: the true OBD speed (PID 0D, no dial factor) in three seven-segment cells over a faint 888,
  `KM/H` under it. `---` without a value.
- **RPM trace**: the last 13 samples, one every 250 ms, as the film's pills; red over 6500 rpm.
- **Gear (EST)**: the old clock slot: a mode square (always D: standard OBD2 has no gear or S mode), the
  estimated gear and seven pips. The estimate is rpm / speed matched to seven bands (125 rpm per km/h in
  1st down to ~20 in 7th, geometric steps); `-` under 10 km/h or outside every band. **The bands are
  guesses until a drive log calibrates them.**
- **Chips**: KM/L now (fueleco), IAT (the City's 68), TRIP km (stays shown without the link).
- **Ribbons**: PEDAL (throttle %), COOL (60 to 130 C scale), BATT (11 to 15 V scale), each with a value, a
  16-pill track and a tag. The lit pills take the value's zone colour; unlit pills a faint tint of their
  own zone. Zones: COOL cold under 77, warn from 95, alert from 105; BATT alert under 12, warn over
  14.7; PEDAL warn from 85 %.
- **Alert**: only COOL and BATT, from `drivedash::Alerts` (coolant over 105 C until under 100; volts under
  12.0 V for 60 s with the engine running, until 12.2 V). The inlay border and value turn the theme's
  alert colour, the tag flashes at 3 Hz. The alert colour is white in the red, orange and pink themes
  (termpal's rule), red elsewhere.
- **Link lost**: every value `--`, the whole screen at 65 %, the girl on the calm cel.
- **Ride without its art** (a build from a fresh checkout: CI's stand-in): the screen falls back to
  Suki / the street look, but `/gauges` still reports `ride`.
- **Demo** (`GAUGES DEMO`, the simulated drive): `DEMO` in the alert colour in the top band, so it is
  never mistaken for the car.

## The girl

Cel by true speed (the mock's `celIdx`): under 3 km/h, no speed or no link: calm cel 7, still. Under
80: five calm cels ping-pong at 4 a second. 80 to 110: calm cels with the open mouth. Over 110: the
teeth cels shuffled 8 a second with a 1 px sideways shake.

## Colours

The deck's theme is the inside light's swatch (`karaoke::light()`, termpal's nine themes, -1 green)
with day or night (`theme::isNightActive()`). The interface uses termpal's TEXT, MID, DIM and ALERT
for that theme and time of day; the tag text is dark or white, whichever reads better on MID.

The art is stored once, as 8-bit indices into one 256-colour palette (the film's own colours). On a
theme or day/night change the deck recolours the palette, not the pixels (the mock's `recolour()`):
every colour with saturation over 0.12 that isn't skin gets a new hue: the purple family (225 to 290)
near the theme's TEXT hue, every other hue around the theme's MID hue with half its spread. Skin
(hue 8 to 55, lightness over 0.72), black, white and grey stay. The white theme cuts saturation to
6 %. Night dims the art to 78 %.

## On the deck

- `util/car/sukiride` (PURE, host-tested): the file reader, the recolour, the scene, and `compose()`,
  which draws any rectangle of the screen into big-endian RGB565. Region keys say what changed.
- `black/ui/sukirideview`: one 23,296-pixel buffer in PSRAM (the cel strip, the biggest region). A whole
  redraw goes in four strips; after that only regions whose key changed are composed and pushed. The
  cel strip (128 x 182, ~46 KB) is the only big push, 4 to 8 times a second.
- The art file `data/sukiride.bin` (~0.5 MB, format SKR1, `sukiride.h`) is **linked into the app**
  (`board_build.embed_files`), not the media pack: a firmware upload (USB or WiFi) carries it, and no
  `uploadmedia` is needed. The app went from 1.62 MB to ~2.1 MB of the 3 MB slot.
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
Fonts: the mock's Orbitron when given with `--font`, else Century Gothic Bold from Windows. Text sizes
are larger than half the mock's (labels 8 px, values 12, gear 18), because half-size labels are about
4 px tall and can't be read on a 2.8" panel.

## Car tests (João, read-only)

1. Ask the Vgate for PID 01 A4 (transmission actual gear) and read the supported-PID bitmaps (01 00 .. 01 A0).
2. Watch the CAN bus for the transmission frames: are S mode and the paddle shifts visible?
3. Compare the OBD speed with the phone's GPS at 60, 80 and 100 km/h (the dial reads 5 to 8 % high).
4. Log rpm and speed on a drive, to calibrate the seven gear bands.
5. Readability at a glance in daylight and at night, and the overheat pulse in the red, orange and pink themes (white there).
