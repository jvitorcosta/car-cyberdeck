# The Cockpit gauge screen (full-height Split Cockpit) on the black deck

Date: 2026-11-20. Status: implemented, merged as #87, #89 and #91; car test owed.

A fifth GAUGES screen, **Cockpit**, next to Suki, Drive, Trip and Ride. It came out of a design review of the
black deck (OBD, screens, bugs) and three rounds of mocks; João picked "Split Cockpit" on round 2's VT323
terminal type and asked for the version without the header row. The mocks, the lab page and the numbers are
archived outside the repo (the film art next to them is not ours to publish).

Picked like the other screens, while parked: `GAUGES SCREEN cockpit` (or `5`) on the serial line, COCKPIT on
the phone page, `GET /gauges?screen=cockpit`.

## Why

Every number on the Ride screen is 7-12 px tall (labels 7 px of cap height, tile values 10, bar values 12). The
panel is 57.6 x 43.2 mm (5.56 px/mm), so that is 1.3-2.2 mm of letter at about 70 cm: a read-while-parked size.
Cockpit gives the driving values 19 px of cap height (the VT323 36 px face), the speed 112 px, and drops what
is not read at a glance (clock, LIVE tag, wordmark, gear estimate, intake air).

## Layout (320 x 240, 8 px margin, no header row)

Left column, x 8..182:
- **Speed**: three seven-segment cells 54 x 112, stroke 13, gap 5, at y 8, over faint ghost 8s; `---` without a value.
- **Status pill** at the top left of the speed, always filled with the alert colour, its type black: `CHK` (check-engine lamp
  on, or a stored code), small enough to stay clear of a lit digit (the first cell's right-hand segments light from
  100 km/h); `NO LINK` (adapter off or searching) or `KEY ON` (ignition on, engine off), which come with `---` on the speed. Nothing otherwise. The
  stored-code count is in the scene but not drawn.
- **KM/H and RPM** labels and the RPM number (VT323 36 px, right-aligned at x 182, baseline 150): normal below 5000, bright
  from 5000, red from 6500.
- **Tach**: 16 capsules growing to the right (6 to 20 px tall), lit by rpm / 7000, the same bands, a white core line on a lit
  capsule, and a 0-7 scale under it (thousands of rpm).
- **Trip and range**: `TRIP` and `RANGE` labels (dim), values at 36 px, `km` after each. The trip stays shown without the link.

Right column, x 190..312, past a one-pixel divider at x 184 (the border colour), five rows 122 x 42 at y 8, 54, 99, 145, 190
(pitch 45.5, the last ends on the margin), top to bottom **COOL, BATT, KM/L, PEDAL, LOAD**. No fill but for an alert, like the yellow deck:
the label top-left (19 px), the value with its unit top-right (36 px: `88\xB0`, `13.9V`, `12.8`, `18%`, `41%`), a 14-tick bar
3 px high along the bottom and a hairline under the row (not the last). The bar's scales: COOL 60..130 C, BATT 11..15 V,
KM/L 0..25, PEDAL and LOAD 0..100 %. The ticks from 64 % of the COOL track (105 C) and from 85 % of PEDAL and LOAD are red.
**KM/L is the trip's average** (`tripKmL`), not the instant reading: the car's own dash already shows that. Like the trip
it stays without the link, and is `--` until there is one.
An alert sets the label and value black on a block of the alert colour (the row's top 33 px) and the lit ticks, on
black under it, in that colour: coolant over 105 C (until under
100 C) and battery under 12 V for 60 s with the engine running (until 12.2 V), from `drivedash::Alerts` as on Drive and
Ride. KM/L has no danger part.

RANGE needs a full tank noted in the fuel book and a learnt km/l; without them it reads `--`.

LOAD is the engine load the car reports (OBD PID 04): the share of the engine's maximum airflow in use, about 20-25 %
idling, 35-55 % cruising, over 85 % under hard acceleration. PEDAL is the throttle / accelerator position.

## Link states

Live: everything. Not live: every value `--` and no bars lit except **BATT**, which stays (the adapter's own reading, as on
Drive), and the trip and range, which stay. Alerts never show without the link.

## Colours

The yellow deck's, exactly (2026-11-20; at first Cockpit had Ride's tinted panel, and cards with a fill until the "yellow-deck look" change the same day): `termpal::rgb(role, theme, true)`, the
theme's night palette at any hour, which is what the yellow deck's `pal()` draws, on `termpal`'s BG, pure black. The theme is
the yellow deck's INSIDE light swatch (`karaoke::themeLight()`), so the two decks show the same colour in the same tone: pink
is termpal's `FF0040` family here, not Ride's pinker `FF3CA0`, and day or night makes no difference. Unlit segments,
capsules and ticks are DIM at 25 %, the divider is DIM at 65 %, the tach's bright band and a
capsule's core line are TEXT lifted toward white. A new colour repaints the whole screen.

## Code

- `util/car/cockpit.{h,cpp}`: geometry, the scene (all text and states), tach and bar maths, the palette and the
  regions with their change keys. PURE, `test/test_cockpit` (13 tests, including a check that the widest strings fit
  their slots with the VT323 advances).
- `black/ui/cockpitview.{h,cpp}`: a 320 x 240 frame sprite in PSRAM (153 KB, made on first use, like the Trip screen),
  the text placed by baseline from the karaoke view's VT323 tables, only the regions whose key changed pushed.
- `util/car/drivedash`: `Screen::Cockpit` (name `cockpit`, number 5, NVS `hud`/`screen`).
- `car/gauges`: `lamp(&codes)`, the check-engine lamp and stored-code count from the OBD link, for the pill.
- Phone page button, `/gauges` and serial `GAUGES SCREEN` accept it.

It needs no art file, so it works on any build and is the screen to show first on a deck with no film art.

## Not done / owed

- Looking at it on a deck: spacing, VT323 baselines and the colours in the car (the mocks were drawn in a browser with
  VT323 itself, the deck uses the repo's glyph tables, so widths are checked on the host but not the pixels).
- Every value is already polled. Two findings from the review are not fixed here and affect what the screen shows: the
  trip km, range and R$ under-count when engine-air readings come more than 2 s apart, and the slow readings (coolant) blank
  after 30 s while arriving every ~20 s.
