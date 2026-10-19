# Gauge cluster on the old deck (design)

Date: 2026-10-19. Branch: `feat/obd` (the OBD gauges, not yet merged; Task 7, the on-car check,
is on hold). Builds on `docs/superpowers/specs/2026-10-15-obd-gauges-design.md`: the Bluetooth
ELM327 link, the request schedule, the link health and the show/hold rules stay as designed there.
This document replaces that spec's **Screen** section and adds readings.

## Intent

The owner wants the companion (the old deck, `cyd_companion`, 320 × 240) to feel like the sporty
aftermarket clusters people add to their cars: **watching the engine work** is the point. Health
readings (temperature, battery) sit on the same screen and simply stay quiet unless something is
off. The car is the owner's **Honda City Touring 2022** (Brazil, 1.5 i-VTEC flex, CVT with paddles
and 7 virtual steps), usually filled with additive gasoline (Brazilian pump gasoline, **E30** since
Aug 2025).

The owner's priorities, in their words: "functional at first", and "it can be even simpler given
that the display is not so big".

## Goals

- One gauge screen, layout approved as a mockup (below): a tach arc with the speed big in it, the
  RPM number, engine temperature, a FUEL panel, a BATTERY panel, pedal vs throttle bars, Zero Two
  small in a corner.
- Readable at a glance on the 320 × 240 screen while driving: few numbers, big digits.
- Fallout-terminal green on black: one green in three brightness levels, no other colours.
- Correct readings first. Visual polish comes after the on-car check.

## Non-goals (this version)

- The gear: the lever position (P R N D S) and the virtual gear (1–7). They are not standard OBD
  values; reading them is a **side task** (below). The screen reserves the lever row but leaves it
  empty until that works.
- Polish from the mockups the owner liked but deferred: scanlines, a voltage graph, min/max, run
  time, Zero Two's lines on this screen, fuel trims, timing, intake temperature, load, altitude,
  ethanol shown on screen.
- Day/night palettes: the screen is green in both.
- Anything written to the car: no clearing codes, no mode 04, no Honda-specific requests. Still
  read-only, as the 2026-10-15 spec requires.

## Anti-goals (a failure even if it technically works)

- A screen too dense to read in a glance (the owner rejected the denser mockups for this).
- Numbers that look precise but are wrong: an estimate must be labelled or behave as one (km/l, km
  left), and a value the car doesn't report shows `--`, never a made-up number.
- Gauges that flicker or stall the deck: the request schedule and the partial redraws of the
  2026-10-15 design stay.

## The screen (approved)

320 × 240, plain green on black. Mockup: `.superpowers/brainstorm/571-1792425251/content/simple-v1.html`
(local, git-ignored; open it in a browser to see it at real size).

| Area | Position (approx.) | Content |
|---|---|---|
| Tach | top centre, arc ~200 px wide, y 0–130 | RPM as a thick arc from 0 to the redline (`OBD_REDLINE`, 6500 default); the zone above ~5500 marked dashed/dim. Speed in the middle, the biggest digits on the screen (~48 px). Under it, small: `KM/H · 3850 RPM`. |
| Lever row | under the speed, y ~112 | `P R N D S`, the current one inverted. **Empty in this version** (side task). |
| Zero Two | top left, 44 × 44 | Her portrait, small, static (the existing avatar art, scaled down). |
| Temperature | top right | `TEMP` label, coolant °C in ~18 px digits. |
| FUEL panel | bottom left, ~152 × 56 | Label `FUEL` and tank `%` on the top line; **km/l now** big (~26 px); a line `AVG 11.9 · 320 KM LEFT`. |
| BATTERY panel | bottom right, ~152 × 56 | Label `BATTERY` and `CHARGING` (inverted) when charging; **volts** big (~26 px); a line `AT START 12.3 V`. |
| Pedal / throttle | bottom, two rows | `PEDAL` and `THROTTLE`, each a long bar (~220 px) with the % at the end. |

Colours: bright green `#41FF6A` for values, mid `#1D8A3A` for labels and frames, dark `#0E4A1F`
for empty bar/arc track; black background. (RGB565 equivalents are the builder's.)

Behaviour on screen:

- A value the car hasn't reported (yet, or ever) shows `--`; its bar or arc is empty.
- A stale value (older than the existing `STALE_MS`) is drawn in the mid green instead of bright.
- Warnings without colour: coolant above the existing Hot band, battery below the existing Warn
  band, and RPM in the top zone draw their value **inverted** (black on bright green).
- The screen appears and leaves exactly as today's gauges do (`obdplan::Hold`); Zero Two's home
  comes back when the car stops answering. The message bar the old gauges kept is gone.

## Readings

Existing (keep): RPM `0C`, speed `0D`, coolant `05`, throttle `11`, battery (`ATRV`). Load `04` and
intake `0F` stay in the request set (intake is needed for km/l) but aren't shown.

New:

| Reading | Request | Shown as |
|---|---|---|
| Accelerator pedal | `0149` (pedal D); `014A` (E) if D isn't supported | PEDAL bar, % (0–100 after scaling the pedal's own min/max, see below) |
| Fuel level | `012F` | FUEL `%` |
| Manifold pressure | `010B` | (input to km/l) |
| Airflow (MAF) | `0110` | (input to km/l, preferred when supported) |
| Ethanol % | `0152` | (input to km/l; not shown) |

Pedal scaling: raw pedal-position PIDs rarely read 0–100 % (e.g. ~15 % released, ~80 % floored).
The PEDAL bar maps the observed range to 0–100, learning min and max as it drives (the builder
decides how, e.g. min/max seen since boot with sane starting values).

### Derived values

- **km/l now:** fuel burnt from the air mass. With MAF: `air g/s` directly. Without MAF
  (speed-density): `air g/s ≈ MAP × displacement × RPM / 120 / (R × T_intake) × VE`, with the City's
  1.5 L (1497 cc) and a fixed volumetric efficiency (the builder's starting value, e.g. 0.85,
  tunable in `config.h`). Fuel g/s = air / AFR, AFR from the ethanol mix (stoichiometric 14.7 for
  gasoline, 9.0 for ethanol, mixed by mass). Ethanol fraction: PID 52 when the car reports it,
  else **E30** (Brazilian pump gasoline), overridable in `config.h`. Fuel density ~0.75 kg/L for
  E30 (builder computes for the mix). km/l = speed / fuel L per h. Below a small speed (e.g. 3
  km/h) show `--` (idle). Smoothed over ~1–2 s so it's readable.
- **AVG:** km/l over the trip since the gauges first went live this power-on (distance from speed,
  fuel from the same estimate). The deck powers off with the ignition, so this is per drive.
- **KM LEFT:** fuel level % × 44 L (the City's tank) × AVG. `--` until AVG has a few km behind it
  (builder picks the threshold, e.g. 2 km) or without a fuel level.
- **AT START:** the battery's dip while the engine cranks. The deck is powered with the ignition
  (owner, 2026-10-19), so it boots at key-on and the crank often comes before the link is up.
  While the link answers but the engine isn't running yet (RPM absent or 0), `ATRV` is read in a
  tight loop (~10 a second) and the minimum is kept; when RPM appears, that minimum is AT START for
  the rest of the power-on. If the engine was already running at the first reading, the dip was
  missed: AT START shows `--` (never a resting voltage presented as the dip).
- **CHARGING:** battery ≥ 13.2 V with the engine running.

## The request schedule

The existing `obdplan::Plan` alternates RPM and speed with one slow value every `SLOW_MS`. With
the new readings:

- Fast (as often as the adapter answers, in turn): RPM, speed, pedal, throttle, and MAF or MAP
  (whichever the car supports, MAF first).
- Slow (one per `SLOW_MS`): coolant, battery, fuel level, intake, ethanol, load.
- Unsupported PIDs (NO DATA) back off as today (`RETRY_MS`), except RPM.

The exact interleaving is the builder's, provided RPM and speed still update at least ~3 times a
second over a typical clone (each request ~50–100 ms).

## In-car recon: the supported-values scan

On every connect, the link asks the car which mode 01 PIDs it supports (`0100`, `0120`, `0140`,
`0160` while each answer says the next block exists) and logs them over serial as one line, e.g.
`[obd] supported: 04 05 0B 0C 0D 0F 11 2F 49 4A`. The schedule skips PIDs the car says it doesn't
support. This is read-only and is how the first car session tells us what the City really answers.

## Side task: the gear (not in this version)

The lever (P R N D S) and the virtual gear (1–7) are broadcast on the car's CAN bus for the
cluster, not available as standard OBD values. Plan, for later: an in-car recording session with
the ELM327 in listen-only mode (`ATMA` / filtered `ATCRA`), engine on, parked, foot on the brake,
shifting P → R → N → D → S and the paddles, to find the message and byte that carry them. Then a
passive filtered listen during driving feeds the lever row. A ratio-based estimate (RPM ÷ speed
matched to the 7 steps) is the fallback for the virtual gear. Separate design when it starts.

## Read-only guarantee (owner requirement)

The owner's words: "make sure we are only reading from this device, not writing to the car, make
the code safe". Today's code is read-only by convention; this version makes it a rule the code
enforces:

- **One gate.** Every byte sent to the adapter goes through a single function in the link
  (`obdlink`), which first asks a pure check, `obd::allowed(cmd)`. A refused command is never
  sent; it's logged as `[obd] BLOCKED <cmd>` and counts as an Error. There is no other path to
  the Bluetooth serial port (no direct `print`/`write` elsewhere).
- **An allow-list, not a block-list.** `allowed()` accepts exactly:
  - mode `01` requests: `01` followed by one PID byte as two hex digits (`010C`, `0100`, `0149`…),
    nothing more;
  - a fixed list of adapter setup commands that configure only the ELM327 itself: `ATZ`, `ATE0`,
    `ATL0`, `ATS0`, `ATH0`/`ATH1`, `ATSP0`, `ATRV`, `ATI`, `ATDPN`;
  - nothing else, in any case or spacing. Everything outside the list is refused: every other mode
    (`02`–`0A`, including `03`/`04` read/clear codes and `08` on-board tests), manufacturer modes
    (`21`, `22`, `2F`, `3B`…), header and protocol tricks (`ATSH`, `ATCRA`, `ATMA`, `ATCSM0`, `ATBD`,
    `ATPP`…), raw CAN frames, empty strings, embedded `\r` (two commands in one).
- **The gear side task** will need listen-only commands (`ATMA`, `ATCRA`); they join the list only
  in that task's own design, with the adapter's silent monitoring left on (never `ATCSM0`, which
  makes the adapter acknowledge frames on the bus).
- **Tests prove it.** Host tests: every command the code can produce (every `request()`, the
  setup list, the supported-PID scan) passes `allowed()`; a set of write/clear/control commands
  (`04`, `0400`, `03`, `08`, `2F...`, `3B...`, `ATSH7E0`, `ATMA`, `010C\r04`, lowercase and spaced
  variants) is refused.
- The README and CHANGELOG say plainly that the gauges are read-only and how that's enforced.

## Constraints

- The companion's heap is tight: ~29–31 KB free with WiFi + Bluetooth up (2026-10-15 measurement).
  No full-screen buffer; draw with TFT primitives and partial redraws (only what changed), as the
  current `gauges.cpp` does.
- Zero Two's portrait comes from the existing avatar art (`data/avatar.bin`, git-ignored); without
  it the corner is empty.
- Read-only on the car, always.
- New pure logic (pedal scaling, km/l, average, km left, start voltage, supported-PID parsing,
  schedule) is host-tested like the existing `util/obd` and `util/obdplan`.

## Calls made in review (Claude's, not the owner's; flagged for the owner)

- **Stale per reading.** A value is "stale" (mid green) when it's older than a few times its own
  expected interval, not the single `STALE_MS`: fast values ~3 s as today, slow values (coolant,
  battery, fuel) ~3 × their round (~30 s). Otherwise TEMP, FUEL and BATTERY would blink between
  bright and dim on every slow read.
- **Battery warning depends on the engine.** Engine running (RPM > 0): inverted below 13.0 V (the
  alternator isn't charging). Engine off: the existing bands (inverted below 12.2 V).
- **Throttle is range-learned like the pedal**, the same way, so the two bars compare like with
  like (PID 11 idles at ~10–15 % on drive-by-wire Hondas).
- **Fuel cut counts as zero fuel.** While coasting with the pedal released the engine burns no
  fuel, but air still flows. Mode 01 PID `03` (fuel system status, "open loop due to decel") is
  added to the slow set and is read-only; when the car doesn't report it, the fallback is pedal
  released + RPM > 1200 + speed > 20 km/h. During fuel cut km/l now shows a capped `99.9`, and
  AVG/KM LEFT count that distance at zero fuel.

- **Reconnect fast to the last adapter.** The link remembers the last adapter that answered (its
  Bluetooth address, in NVS) and connects to it straight away at boot, falling back to the
  existing discovery only if that fails. That cuts ~10 s of discovery from every drive, so the
  gauges come up sooner and AT START catches the crank more often.

## Left to the builder

- Fonts and exact pixel positions within the approved layout; the arc's drawing method and how
  often it redraws.
- The RGB565 values of the three greens.
- The fast/slow interleaving, within the rate stated above.
- The smoothing for km/l, the AVG/KM LEFT thresholds, the pedal-learning method, the start-voltage
  window.
- Whether `obd::Pid` grows or a second table holds the new PIDs.

## Testing

- Host tests for every new pure piece (parsing `0100`-style bitmaps, the new PIDs' scaling, km/l
  with MAF and with MAP, the ethanol fallback, average and km left, pedal scaling, the start dip).
- Builds: `cyd_companion`, plus `s3` and `cyd` unaffected.
- The on-car check (the existing Task 7, extended): the supported-PID line, every value against the
  car's own cluster (speed, RPM, fuel, temperature), km/l plausibility on a steady cruise, the
  screen's readability at a glance.
