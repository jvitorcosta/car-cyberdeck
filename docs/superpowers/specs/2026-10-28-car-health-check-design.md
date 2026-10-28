# Car health check and trip summary (yellow deck)

Date: 2026-10-28 · Status: draft, waiting on the owner's approval · Deck: yellow (CYD, env `ledpanel`)

## Intent

The City now carries a lot that Honda never planned for: both decks, two LED strips (inside and
outside), phone chargers, a game-controller charger, other screens, and a courtesy GPS tracker that
stays on the OBD port. The owner wants the car to tell him, in plain words and without digging:

1. **Is the battery coping** with all of that, and is it getting weaker over the weeks?
2. **Is the engine healthy**: direct injection is why he only fills with additive gasoline
   (gasolina aditivada), so slow drifts in how the engine corrects its fuel mix matter.
3. **What did this trip cost**, the way a Toyota Yaris shows a summary at the end of each drive:
   km, km/l and R$.

And he wants to see it **when he gets in and before he gets out**. A screen that only dumps raw
numbers is a failure. If it were only numbers, the only ones he'd care about are distance and range.

Tyre pressure was discussed and **left out**: the City's TPMS is indirect (ABS wheel speeds, no PSI
anywhere), internal sensors cost ~R$300-500 fitted, and external caps get stolen. A wheel-speed
version may come later through CAN sniffing (car-can-lab); it is not part of this design.

## Goals

- **Screens A-D** on the yellow deck (mockups agreed 2026-10-28, see "Screens"):
  - **A** start check, all OK.
  - **B** start check with a problem, shown in red.
  - **C** the dashboard while driving, with real km, km/l now and range.
  - **D** the trip summary while parked, before he switches off.
- **Instant warnings** on the deck from fixed limits (no cloud needed).
- **Every trip logged, a minute summary per row**, and uploaded every 5 minutes while there is
  internet (the phone hotspot). Nothing is lost when the power cuts at ignition off: at most the last
  ~90 s (the minute being built plus up to 30 s of unflushed rows).
- **A daily checkup on the PC** that compares the data with learned baselines and writes a summary
  for the owner. Its verdict comes back to the deck and shows on screens A/B.
- **Trip cost in R$** from the fuel price.

## Non-goals (this version)

- Tyre pressure, gear, fuel level from CAN (car-can-lab's job), and HUD.
- Clearing trouble codes or writing anything to the car. The car's buses are **read-only**, always.
- The S3's Pokémon status effects (PARALYZED = weak battery, etc.): a later, separate piece.
- A phone app.

## Anti-goals (would make it a failure even if it works)

- **Unreadable output**: walls of numbers, abbreviations he has to decode, tiny text while driving.
- **Nagging**: false alarms train him to ignore it. Honda's smart charging idles at ~12.3-12.8 V
  while running, so a low running voltage alone is not a fault; limits must respect that and learn
  this car's normal.
- **Distraction**: nothing flashes or beeps while moving, except a critical warning (overheating,
  check-engine light).
- **Breaking what works**: the LED panel, its BLE controllers, the Xbox pad, voice `/say`, the watch
  app and KARAOKE must keep working. If the OBD link starves them, OBD loses.
- **Leaking data**: this repo is public; car data, tokens and personal notes never go into it.

## Constraints and known facts

- **Hardware path.** The **Vgate iCar Pro BLE 4.0** (ordered, not arrived) sits on an **OBD Y splitter**
  (owner has it) next to the courtesy tracker. It is the yellow deck's 4th NimBLE connection, as the
  2026-10-23 menu spec already decided. `feat/obd` has the link pieces to reuse: `util/obdble` (GATT
  pick FFF0/FFE0/18F0, inbox), `util/obd`, `util/obdplan`, `util/fueleco`, `util/carwarn`,
  `util/carstatus`, all host-tested. Copy what's needed onto a fresh branch from `main`; `feat/obd`
  itself stays parked. Two catches:
  - `feat/obd`'s BLE transport was written against NimBLE 1.4.3, while `ledpanel` runs NimBLE 2.5.1,
    so it needs a port.
  - `feat/obd` made Bluetooth Classic its default because BLE was flaky with the *old* adapter. The
    Vgate is a different adapter, but BLE reliability is the first thing to prove.
- **`fueleco`'s fuel-cut logic** also reads pedal/throttle (49/4A/11) and fuel-system status (03).
  Poll them alongside air mass.
- **Car test before merge.** Like all OBD work so far, nothing merges until the owner has tried it
  in the car.
  - Before the Vgate arrives, build against a bench stand-in: an ELM327 replay/emulator over BLE.
  - The car session is the merge gate, with a checklist in `docs/BACKLOG.md`.
- **What the City answers** (Mode 01 PIDs seen 2026-10-19 with the old adapter; three ECUs): 01 03 04 06
  07 0B 0C 0D 0E 10 11 13 15 1C 1F 21 23 2E 30 31 33 34 41-47 49 4A 4C 52 56 67 68 6C.
  - **Present:** MAF (10), MAP (0B), coolant and intake temperatures (67/68, not 05/0F), fuel trims
    (06/07), control module voltage (42), ethanol % (52).
  - **Missing:** fuel level (2F) and fuel rate (5E).
  - Mode 06 (per-cylinder misfire counters) is untested.
- **Clone replies can arrive after >1 s.** `ask()` waits 2 s and resyncs (lesson from `feat/obd`).
  Two readers share the bus (the tracker polls too), so a lost or odd reply is retried, never
  trusted.
- **Power.** The decks power up and down with the ignition. The deck cannot run after the power cut.
- **Network.** The decks reach the internet only through the phone hotspot. The home router isolates
  the decks, so the PC can never pull from them; data goes through the cloud.
- **TLS.** The yellow deck has never done TLS beside NimBLE (its weather uses plain http). The S3
  does TLS every few seconds for Spotify and watches its heap (self-heal, largest block never under
  102 KB in a 30-min capture).
- **The yellow deck already has:**
  - the trip meter (`util/yellow/tripmeter`, NVS record, ">LAST TRIP" for 6 s at boot);
  - voice `/say` (Siri Shortcut "Honda", English);
  - the HI/LO message row;
  - night orange 18:00-06:00;
  - themes that follow the inside light;
  - VT323 at 17/19/24/36 px;
  - red reserved for problems.
- Tank 44 L; the pump gasoline is E30. Ethanol % from PID 52 when it reads sanely, else 30.

## Screens

Mockups from the brainstorm (320x240, VT323, the current theme colour, red only for problems). Text
on the deck stays **English** like the rest of CYBERDECK (owner's convention for the deck).

- **A · Start check, all OK (`>PRE-FLIGHT CHECK`).** Shown at boot once the OBD link answers, ~5 s,
  then the dashboard.
  - BATTERY: volts + OK, one detail line on the running charge vs this car's average. The mockup's
    "crank dip · rested 9h" line is not possible (see "What gets measured").
  - ENGINE: OK, with codes and the fuel-trim state.
  - LAST TRIP: km · km/l · R$.
  - RANGE: ~km.
  - Status line: `>ALL SYSTEMS NOMINAL`.
- **B · Start check with a problem.** Same layout, only the failing item in red, plus up to three
  lines in plain words: what changed and what to do ("Charging lower than usual for 5 trips. Have
  the battery and alternator checked."). Stays until tapped. The status line reads `>1 ISSUE [TAP FOR DETAILS]`.
- **C · Driving (the dashboard).** Today's layout B, with the trip row replaced by three big figures:
  - TRIP km, km/l NOW, RANGE km;
  - a small line under them: time, R$ so far, BATT V and ENG °C.
  - Health stays quiet here; a live warning takes the message row, in red.
- **D · Parked (`>TRIP COMPLETE`).** Appears by itself after the car has stood at 0 km/h for ~5 s on a
  trip of at least 0.5 km, and goes away as soon as it moves or on a tap.
  - Big: km, minutes, km/l, R$.
  - Small: average speed, idle minutes, range.
  - BATTERY / ENGINE OK-or-not.
  - `>SAFE TO SHUT DOWN`.

  So he reads it **before** pressing the button, and the deck never has to outlive the power cut.
  At a red light it simply shows the trip so far, which is harmless.

## What gets measured

All reads are Mode 01/03/07 and `AT RV` (`feat/obd`'s read-only gate allows only Mode 01 and AT setup today, so it must also admit exactly `03` and `07`; Mode 04, which clears codes, stays refused). Rates below are a default for the builder to tune against
the BLE link's real throughput.

| What | Source | Rate | Used for |
|---|---|---|---|
| Speed, RPM | 0D, 0C | 1 s | km, idle time, screen D trigger |
| Air mass | 10 (MAF; MAP 0B + intake 68 as fallback) | 1 s | litres → km/l, R$, range (`fueleco`) |
| Battery | `AT RV` (pin 16) and 42 | 5 s | min/avg/max per minute, charging behaviour |
| Coolant | 67 | 10 s | max per minute, overheating warning |
| Fuel trims | 06, 07 | 10 s | LTFT drift (the injection health signal) |
| Status / codes | 01 (MIL + count), Mode 03, Mode 07 | at start + every 5 min | new stored or pending codes |
| Ethanol | 52 | at start | fuel maths |

**The battery is only measured while running.** The owner confirmed that the decks only come on once
the engine is already running. So this version has **no resting voltage, no crank dip and no
parked-drain figure**. Battery health rests on:
- the running voltage per trip (min/avg/max) and its trend over weeks;
- **the charge right after start** (experimental, my call). Honda's smart charging recharges harder
  just after a start to replace what the crank used. How high and how long the voltage stays up in
  the first minutes is logged per trip. If that recharge keeps getting longer over the weeks, it
  hints at a tired battery. The checkup reports it as a note, never as a problem, until real data
  shows it means something.

Resting and crank figures need an always-on device; see "Later".

## Instant warnings (on the deck, no cloud)

Defaults; the builder keeps them in one table so they're easy to tune.

- **Battery, running:** under 12.0 V for 60 s → red on the message row (12.0 V is the known
  limit for this car's smart charging).
- **Coolant:** over 105 °C → red, with the critical exception to "no flashing while moving".
- **Check-engine light on,** or a new stored code → red on B and on the row, code shown as `P0xxx`.
- **New pending code** → amber-level note on B ("pending, not confirmed yet"), no red.
- **Long-term fuel trim** beyond ±10 % for a whole trip → note on B. Beyond ±20 % → red.

## Trip log and upload

- **A trip** = one deck power-on with the OBD link up and the car moving at some point. It gets an
  id (boot time from SNTP when synced; otherwise a boot counter plus uptime, fixed up later).
- **Each minute** the deck closes one compact row:
  - minute index, km in the minute, average and max speed, average RPM, idle seconds, litres;
  - battery min/avg/max, coolant max, STFT/LTFT averages;
  - flags (MIL, new codes).
  - The builder picks the encoding (JSON lines is fine; must stay small).
- **Persistence:** the open trip's rows go to flash every 30 s (the `min_spiffs` data partition or
  NVS, builder's call, mind wear). At the next boot an unfinished trip is closed as-is.
- **Upload:** every 5 minutes while the hotspot is up, plus at boot for whatever didn't go last time.
  - Destination: a **private** GitHub repository just for this data. The owner agreed to a private
    repo with a key on the deck that can only reach it. Its name defaults to `city-telemetry` (my
    call; easy to change).
  - Each batch is a new file (`trips/YYYY/MM/<trip-id>-<seq>.jsonl`) via the contents API, so no
    read-modify-write.
  - Token: a fine-grained PAT that can only write that one repo, kept in `config.h` like the other
    secrets (never committed; OTA stays push-only).
- **Who uploads, my call:** the **S3**, because it already does TLS and watches its heap. The yellow
  deck hands rows to the S3 over plain http on the hotspot LAN, the same way it already reads the
  S3's `/state` for KARAOKE.
  - The S3 queues them in RAM and uploads between Spotify polls, never two TLS sessions at once.
  - If the S3 is off, the rows wait on the yellow deck.
  - The builder may move the upload onto the yellow deck instead, but only if a heap capture with
    the LEDs, the Xbox pad and OBD all connected shows room for TLS.

## Daily checkup (PC)

- **A deterministic script** in this repo (`tools/car_checkup.py`, pytest-tested on synthetic data,
  no real data in the repo) pulls the private repo and computes per-trip and rolling figures:
  - battery running min/avg and the after-start recharge profile;
  - LTFT trend;
  - coolant warm-up time;
  - km/l;
  - new codes.
  - **Baselines:** the first two weeks of this car's own data (rolling median), with the manual-ish
    defaults above until then.
  - **Output:** a verdict per item (OK / note / problem), each with a one-line plain-language reason
    and the delta vs baseline.
- **A scheduled routine** on the owner's PC runs the script once a day and writes the human summary
  into the owner's notes (outside this repo).
  - **My call:** a Claude scheduled task, like the owner's other daily routines. It runs the script
    and turns its verdicts into the note. The numbers and verdicts come only from the script; Claude
    only words them.
  - Its access to the private repo (pull, commit `checkup.json`) lives in the owner's local git
    credentials, never in this repo.
  - It also commits a small `checkup.json` (≤ 1 KB: verdicts, short reasons, last fill price) back
    to the private repo.
- **Back to the deck:** the S3 fetches `checkup.json` at boot and every 30 min. The yellow deck reads
  it from the S3 and merges it into screens A/B. Instant warnings from the deck always win over a
  stale checkup.

## Fuel price, km/l and range

- **km/l and litres** come from `fueleco` (air mass → fuel for the E30 blend, fuel cut while
  coasting), already written and tested on `feat/obd`. Expect a few % error; a later fill-up
  comparison corrects a scale factor.
- **Fill-up by voice, my call:** at the pump he says "Honda, filled up at 6.49" (Siri, English, the
  existing `/say`).
  - That one phrase marks the tank as full **and** sets the price per litre.
  - The watch app can get the same action later.
- **Price fallback, my call:** the PC routine reads ANP's weekly average for additive gasoline in
  his city when no price was spoken in the last 30 days (to confirm ANP publishes that product per
  city).
- **Range:** the City has no fuel level over OBD, so range is (44 L − litres burnt since the last
  full tank) × recent km/l. It shows `--` until the first fill-up is recorded. A CAN fuel level from
  car-can-lab can replace this later.

## Phases (suggested build order)

1. **On the deck alone:** the OBD-over-BLE link beside the LEDs and the Xbox pad, the readings,
   instant warnings, screens A-D, local trip log. Useful even with no network.
2. **Fuel:** voice fill-up and price, range, R$ everywhere. The fill-up and price go into the local
   trip log too.
3. **Upload:** yellow → S3 → private repo.
4. **Checkup:** the script, the routine, `checkup.json` back to the deck.

Order, my call: fuel comes second because distance and range are what the owner said he cares
about most.

## Left to the builder

- Encodings, flash layout, exact PID scheduling, and retry/backoff.
- How the 4th NimBLE connection shares airtime and heap with the LED controllers and the pad.
  Measure it, and keep `-D` test hooks like the existing `NIGHT_TEST`/`THEME_TEST`.
- Pixel layout within the agreed screens, and the exact wording of the plain-language lines.
- Host tests for every pure piece, as in the rest of `src/util`.

## Later (not this version)

- **An always-on battery logger.** A small low-power board on the OBD Y splitter's 12 V (pin 16)
  sleeps and wakes now and then to log the resting voltage. If it can sample fast enough, it also
  catches the crank dip. It hands its log to the yellow deck over BLE once the car is running. This
  is also what would show whether the courtesy tracker and the Vgate drain the battery while the car
  is parked.
- Tyre-pressure trends from the CAN wheel speeds (car-can-lab).
- The S3's Pokémon status effects.
