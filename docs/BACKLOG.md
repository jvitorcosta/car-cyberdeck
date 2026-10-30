# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Done (moved out of the lists below)

- **Yellow deck** (the CYD as the car's light panel): the car LEDs over BLE by touch, voice, Xbox
  controller and the watch (#13, tested in the car 2026-10-22); the CYBERDECK dashboard, presets,
  LYRICS, weather, trip, themes and night mode (#18, tested in the car 2026-10-24: "works great");
  Japanese lyrics (#21).
- **Updates over WiFi** on both decks (#9, #19; the yellow deck's first OTA went through on
  2026-10-24 and kept its touch calibration), with their own `OTA_PASSWORD` (#25) and the
  rollback of new firmware that resets in its first minute (#27).
- **The Honda City watch app** (#13, #20): pages, presets, glance, signed requests (#27).
- **Music mode as song colours** (#20, #23): the black deck picks each song's colour, the yellow
  deck shows it; by voice, `/status` and the watch since wave 4.
- **INSIDE / OUTSIDE** controllers confirmed the right way round (2026-10-24); their addresses
  are in `config.h` since wave 4.
- **The repo reorganisation** (#26): one folder per deck, util in groups; the companion screen
  retired.
- **Dropped WiFi receive buffers** (closed 2026-10-23): a 30-min S3 capture had no failed
  allocation; the drops were the retired CYD deck build's.

## Verification (on device)

- **Wave 4, the rest**: music by voice, the watch's Music mode item and "Cyan" were confirmed in
  the car (2026-10-28). Still unchecked: `/status` says `music=`, and the colour lists follow the
  deck's order.
- **`blescan` / `bletest` on NimBLE 2** (#31, only compiled): `blescan` lists the LED controllers
  with their addresses; `bletest` connects to both and cycles the colours. Flash `ledpanel` back
  afterwards; no flash erase should be needed any more.
- **A cut update, by eye** (both decks): stop the PC's upload mid-way. Yellow: `>UPDATE FAILED`
  for ~3 s, then the screen that was showing, with the lights, lyrics and weather carrying on;
  also see the `>UPDATING` page on a normal update. Black: grey rail and "Update failed" for ~3 s,
  then the deck polling again. Windows moves the PC back to the home WiFi on its own: check it is
  still on the hotspot before an upload.
- **Rollback on the device**: upload a build that crashes early (an `abort()` in `setup()`);
  within its first minute the bootloader boots the previous firmware (`[ota] ... rolled back`);
  a build that crashes 3 times after that minute goes back too (`[ota] rolled back`).
- **Green Hill Zone update screen** (built 2026-10-22, only compiled): after `uploadmedia` with
  `data/backdrop.bin` and `data/shadow.bin`, an update over WiFi shows Green Hill, the rail filling
  red with Shadow at its tip, a red flash at 100 %, the reboot.
- **Car LEDs, the rest**: the LED colours (orange, pink, purple, white and yellow are guesses for
  RGB LEDs) and whether the 10 % brightness steps look even; the Xbox controller with the lights
  connected; memory over a long drive (the heap's low point drifted 130 → 123 KB over 25 min at
  home).
- **Animated status screens**: seen on the S3 (Spotify busy + countdown); the no-WiFi /
  no-internet / certificate screens only from host renders.
- **Car-health screens, by eye** (yellow deck, built 2026-10-29, only the serial log seen): look at
  screens A, B and D with `-D HEALTH_TEST=a|b|d` and compare with the mockups (the brainstorm's
  health moments A, B, D): spacing, colours (B's battery lines in ALERT), nothing past 316 px. A
  shows for 5 s after the boot (RESET to see it again); B holds until a tap; D comes 5 s after the
  Menu. Then with the OBD deck: A on a real power-on, D after a real stop. Each screen can be saved
  as a PNG over USB with `python tools/deck_shot.py <port> <out.png>` (never run on the deck yet:
  check it reads the panel back, that the colours match, and that about 30 s without touch is fine).
- **Half-dead link** (WiFi up, no mobile data): the iPhone hotspot works (2026-10-08), but this
  path has never run on the board. The hotspot needs "Maximize Compatibility" on (2.4 GHz only).

## Code health

- **Split `src/yellow/ledpanel.cpp`** (~3 000 lines): the link task, the web server, the lyrics
  and music tasks and the weather are each a candidate file under `src/yellow/`. Wait until the car
  health work (`feat/car-health`, which adds screens to this file) is merged.
- **`feat/obd`** predates the 2026-10-28 reorganisation and ran on the retired companion. Its OBD
  core moves to the OBD deck in `feat/car-health` (`src/util/car/`); retire the branch once that
  merges. (`feat/voice-agent` is already on the new layout.)
- **CI doesn't build the diagnostics**: the firmware matrix has `s3`, `ledpanel`, `obddeck`,
  `hwcheck` and `hwcheck_s3`, so `blescan` / `bletest` stopped linking without anyone noticing. Add
  `blescan`, `bletest`, `xboxtest` and `ps3test` (and the mic envs once the voice agent merges), or
  a weekly job that builds them.

## Investigate

- **DMA for the S3 intro** (blocked, 2026-10-15): pushing frames by DMA from two buffers ran
  at ~36 ms a frame, but TFT_eSPI's `initDMA()` on this board left every later draw black, with
  or without `deInitDMA()`. Plain pushes take ~50 ms, so some frames of the 20 fps clip are
  skipped. Worth another try only with a way to hand the bus back cleanly.

## Spotify rate limit

- One Spotify app is shared by every deck that runs a Spotify client: running the CYD's old
  standalone build next to the S3 got both blocked for ~2 h on 2026-10-13. Only the black deck
  runs one now.

## Lyrics polish (deferred minors, final review 2026-10-08)

- Search can never match a track whose duration is 0 (lyrics wouldn't advance anyway: the
  position is clamped to the duration).
- `test_all_messages_fit_two_lines` checks bytes, not rendered pixel width (worst case: "...").

## Genre badge polish (deferred minors, final review 2026-10-08)

- Test gaps vs the spec: real Apple answers for Pop 14 / Brasileira 1122, every badge's colour,
  and "network errors are not cached" (device code in nettask).

## OBD deck: first flash

The OBD deck (`src/obd`, env `obddeck`) is built and host-tested but has never run on a board
(none was plugged in on 2026-10-29). On its first board, over USB, never the yellow deck's or the
black deck's port: `pio run -e obddeck -t upload --upload-port COMx`, then the monitor.

- **Boot log**: `[boot] obddeck v...`, `[car] LittleFS ... bytes used, mounted in N ms`,
  `[car] boot 1, last trip none, fuel book empty`, `[obd] BLE up, 3 connections allowed`,
  `[wifi] connected ...` (on the hotspot `[wifi] fixed hotspot address 172.20.10.12`),
  `[clock] SNTP started`, `[ota] listening`, `[net] on: http://.../car`. A second boot says
  `boot 2`.
- **First-boot format**: the first boot formats the data partition; note the `mounted in N ms`
  of the first boot against the second (the format must stay well under the 90 s watchdog; it
  runs in setup(), before the loop watchdog starts).
- **BLE with the roles compiled out**: NimBLE runs central + observer only on esp32dev
  (peripheral and broadcaster off in `platformio.ini`): the 4 s scan logs `[obd] seen ...` lines
  for nearby devices (a phone, the black deck's `V-CYBERDECK`), and a connect attempt to a
  scanned candidate gets as far as `connect failed` / `no serial service` without a crash.
- **No adapter**: `[obd] no adapter: next try in 5 s`, then 10, 20, 40, 60 s and 60 s from then on;
  the black deck (`V-CYBERDECK`) is seen but never connected (its name isn't an adapter's), and
  the lights never appear in the log (they're filtered out before the `seen` line).
- **`/car`**: a signed request answers 200 with the JSON (`"v":1`, `"seq"` rising each second,
  `"link":"off"` with no adapter); the same with `?k=<REMOTE_TOKEN>` and no signature answers 403; before SNTP a signed
  one answers 503 "clock not set". Sign from the PC with `util/wire/sigreq`'s scheme (or wait for
  Task 10's yellow-deck client).
- **`/fill`**: signed `GET /fill?p=6.49` answers `ok 6.49`; after a reboot `/car` still shows
  `"price":6.49` (the fuel book in NVS). Then signed `GET /fill?p=20` answers `refused 20` and
  `/car` still shows `"price":6.49`; `GET /fill` with no `p` answers `ok none`.
- **Staying up**: `[car]` every 10 s and `[mem]` every minute for 10 min, no `TG1WDT` / task
  watchdog reset, no `[ota] rolled back`; after a minute online `[ota] this firmware is good`.
- **Memory** (`[mem]` after 10 min with WiFi, NimBLE and the server all up, no adapter):
  - stack left in `net`, `obd-link` and `obd-ask`: each must stay above 1 KB (pass floor); note
    the numbers (each task has 6 KB);
  - the minimum free heap (`min`): note it as the no-adapter baseline for the adapter checks below.
- **Power cut mid-row** (`::truncate` on esp_littlefs, never run on a board): pull the power
  within a few seconds of a `[car]` line after a minute or more (or append a partial line by hand
  from a test build), boot again: `[car] log: cut a partial row left by a power cut`, and the file
  ends at its last whole row (`truncate()` returning 0 is what LittleFS on this core must do).
- **Usage cap** (the 128 KB partition, 32 blocks): boot it 20+ times for a minute or two each
  (each power-on is a file, a whole block): from the 21st file, or past 70 % of the blocks, the
  boot logs `[car] log: dropped the N oldest trip file(s), U of T bytes used` and the oldest
  `/t<seq>.jsonl` are gone; U stays at or under 70 % of T, and rows keep being written.
- **Update over WiFi**: `pio run -e obddeck_ota -t upload` from a PC on the hotspot.

## OBD deck bench build (`obddeck_bench`, never in the car)

A simulated 10 min drive (`util/car/benchdrive`) in place of the adapter, on a real board: start-up,
stops for screen D, an overheat at 4:00, a low battery from 7:20, a stored code and the MIL from
8:50. `/car` says `"sim":1`, and the yellow deck writes SIM on the check, the trip summary and the
band's small line. Its NVS namespace is `carsim` and it keeps no trip log, so a real deck's records
stay; the trip log is therefore only tested by the normal build. Reflash `obddeck` before the car.

## Both decks on the hotspot, 30 min, OBD deck without adapter

Before the car: the yellow deck and the OBD deck on the hotspot together for 30 min, no Vgate.
- Yellow `[mem]`: the minimum free heap stays above 78 KB with the car task polling `/car`; note
  the car task's stack left (`carlink::stackLeft`).
- No `TG1WDT` / task watchdog reset on either deck, no `[ota] rolled back`.
- The lights and the pad answer as fast as without the OBD deck while it is polled (a colour from
  the pad, a preset, a `/say`).
- The `/say` fill-up end to end: `filled up at 6.49` -> `Full tank noted, 6.49 per litre.`;
  `filled up at 649` (the OBD deck answers `refused 649`) -> `Full tank noted. I heard 649, that's
  not a price.`; `full tank` (`ok none`) -> `Full tank noted.`; the OBD deck logs each.
- The trip band on the Menu: `0.0 KM`, dashes for km/L and the range, and the small line
  `0:16  R$ --  --V  --C` style (the time, dashes for the rest).

## Car-health: waiting on the adapter

The OBD link (now on the OBD deck) has never met an adapter: the Vgate hasn't arrived and
`tools/obd_sim.py` can't run its BLE side on the Windows PC. From the yellow-deck attempt (Task 8,
2026-10-28, parked): a 4th BLE connection on the yellow deck is not safe (the precompiled framework
reserves the controller's DRAM for 3; a build with 4 crash-looped), which is why the OBD link moved
to its own board. That attempt measured ~7-8 KB of heap for obdlink.

First, from the PC: `python tools/obd_probe.py` to find it, then `python tools/obd_probe.py <name>`
in the car with the ignition on: its GATT layout, each reply, and ATRV beside PID 42. Check both
voltages against a multimeter: the battery warnings trust ATRV, and another maker's adapter read
28.3 V and 11.8 V on one car.

To check on the OBD deck when the Vgate arrives (or against a sim on a spare ESP32):
- `[obd] gatt ...` lines: record the Vgate's real service (16-bit FFF0 or a 128-bit UUID);
  `obdble::pick` keeps 128-bit services apart by their discovery index.
- `[obd] connected <name>`, then `[obd] supported: ...` and `[obd] codes N ...` /
  `[obd] pending N ...` once the car answers (the codes wait for a live reply).
- Values follow the car within ~2 s (speed, RPM) and the battery at least every ~5 s with the engine
  running (`[car]` line).
- A stored code shows as `[obd] codes 1 P0301` (sim: `code P0301`; within 5 min or at reconnect).
- With the adapter gone: `[obd] <name>: disconnected`, then the backoff 5, 10, 20, 40, 60 s; with a
  working adapter that drops, back at once.
- After a reboot the remembered adapter connects first (`(remembered)`, NVS `obd`/`bleaddr`).
- The Vgate's advertised name: `[obd] seen "<name>" ... dBm` must be one `obd::looksLikeAdapter`
  takes (a name it doesn't take is never tried; only a nameless serial device is); a nearby serial gadget
  that fails the setup shows `skipped for 10 min` and the next round tries another device.
- `[mem]` with the adapter connected vs the no-adapter level; the `obd-ask` stack's live use.
- A trip: `/t<seq>.jsonl` gets a row a minute (30 s flushes); a power cut mid-row is trimmed at the
  next boot (`[car] log: cut a partial row`), and screen A's last trip / range follow.

## Car test (car-health, the owner)

With both decks in the car, the Vgate in the port, the hotspot on:
1. The OBD deck connects to the Vgate within 30 s of the start.
2. Screen A appears with real volts.
3. km matches the car's trip meter within 2 %.
4. km/l is within 10 % of the car's display.
5. Screen D appears at a stop.
6. Voice fill-up at the pump works.
7. The LEDs, the pad and KARAOKE keep working.
8. A one-week log on the OBD deck shows no row gaps beyond link drops.

Before the car, from a PC that can reach the yellow deck (the home router isolates them; the
hotspot doesn't), with no OBD deck on: `/say?text=filled%20up%20at%206.49&k=<REMOTE_TOKEN>` must
answer `I can't reach the car module.` at once (not checked on 2026-10-29: the PC couldn't reach the
deck). With the OBD deck: `filled up at 6.49` -> `Full tank noted, 6.49 per litre.`, `filled up at
649` -> `Full tank noted. I heard 649, that's not a price.`, `full tank` -> `Full tank noted.`, and
the OBD deck's log says `[car] fill-up from ...` each time.

Also by eye on the yellow deck's Menu: the trip band's three figures and the small line under them
(spacing, nothing past 316 px), and a warning in red on the prompt line (a hot engine, or the MIL).

## Car-health polish (deferred minors, reviews of 2026-10-28/29)

Behaviour, to watch in the car test:
- Trip and fuel book litres can disagree while the speed reading is stale.
- `session()` can use an rpm that isn't fresh just after a reconnect.
- A warning replaces the message on the prompt line, and screen A wipes the Menu message.
- Screen A waits for the next Menu if the car turns ready on another screen, so it can show late
  after a long Lights session (a deliberate choice).
- `carhealth::hot()` ignores whether the engine runs (heat soak); the deck only runs with the
  engine on, so low risk.

Small code gaps:
- `/fill` accepts `nan` and `inf` (`strtof`; only the reply is affected); `carwire::getReal` accepts
  them too, plus hex.
- `triplog`: a partial append plus a retry leaves a partial row mid-file; long ids can collide
  after the cut.
- `obd::parse` answers NoData if "NO DATA" appears anywhere, before the multi-ECU status walk;
  `anchored()` refuses a headered first frame (fails safe, the deck runs ATH0).
- `healthview`: `-` as the unknown check; the band truncates minutes where the parked screen rounds;
  sub-zero coolant shows `--C`. The line clear assumes ASCII (an accented reason would leave its top
  pixels behind).
- The yellow deck logs host-not-found as rc=-1, like a timeout.

Tests missing:
- `tripsum`: the trim averages, flag bits 1-2.
- `carhealth`: LTFT exactly 10/20, LTFT with the engine off, `endTrip` with no samples, MIL plus
  stored codes.
- `fuelbook`: "abastecido", a lone "full" / "tanque", a failed decode leaves the book untouched.
- `healthview`: `ParkGate` wrap and NAN trip km; the small-line fit test has no margin.
- `carwire`: `level` over 2 and `flag` over 1 refused. `carfs`: `tripSeq` and the sort.

## Parked: OBD gauge cluster (`feat/obd`, decided 2026-10-23)

The OBD gauge cluster, kept for later on the branch `feat/obd` (pushed to GitHub as a backup on
2026-10-23, no PR): 41 commits on an older main.

- **What it has:** the gauge cluster (RPM arc, speed, coolant, km/l, battery and the crank dip)
  read from an ELM327 over Bluetooth Classic, **seen working in the Honda City on 2026-10-19**;
  drive screens built 2026-10-21 and only compiled. Read-only toward the car by construction: one
  gate, `obd::allowed()`, mode 01 and adapter setup only.
- **Why parked:** it was built on the companion screen, which needs the CYD, and the CYD is now
  the yellow deck. Bluetooth Classic for OBD leaves ~29 KB of heap, so the lights' BLE on the same
  ESP32 is unlikely to fit. Decide on a third board (or a BLE adapter for the yellow deck) before
  returning.
- **Returning as car health** (2026-10-28): `feat/car-health` copies the OBD core into
  `src/util/car/` and runs a BLE adapter on the yellow deck beside the lights. The rest of this
  branch (the companion's screens, `d079b62`, the fix copies) stays behind; retire the branch once
  car health merges. The car checks still to do are in the branch's own BACKLOG.
- **Trip reset**: the S3 still has `POST /trip/reset` (`net/remote`, `util/tripclock`), which only
  the retired companion used. Keep it or remove it when the OBD work returns.

## Parked: voice agent (`feat/voice-agent`, local)

Portuguese voice commands for the black deck through a cloud agent (spec
`docs/superpowers/specs/2026-10-23-voice-agent-design.md` on the branch). Rebased onto the new layout on
2026-10-29: 36 commits, not pushed; the old head is tagged `backup/voice-agent-pre-reorg`.

- **Mic:** the onboard ES8311 mic is too weak inside the shell; waiting on an external I2S mic
  (INMP441), then re-run the 10-clip recording and the speech-to-text bench.
- **Untested since the rebase:** the mic functions now go through main's shared audio port (the
  beep, live gain / ADC volume / ALC changes).
- **Bearer key:** the `micrec_s3` recorder serves its clips with `?k=` and
  `tools/micrec_pull.py` sends it. Sign those requests like the decks' (`util/wire/sigreq`), or
  accept it for a bench-only tool.
- **Old paths:** the spec and the two plans still name pre-reorganisation paths
  (`docs/superpowers/README.md` maps them).
- **Live steps still owed:** the API key, the Spotify `--agent` token, the slow-path smoke test and
  the eval baselines.

## Parked: PS3 controller (2026-10-22, waiting on a battery)

A DualShock 3 over Bluetooth Classic (`ps3test`, Bluedroid): it is already paired to the yellow
deck over USB (its master address set to the CYD's Bluetooth address, no SixaxisPairTool), but its
battery is dead, and a genuine DS3 powers on only from a USB host. Next: a new LIP1472 (3.7 V
570 mAh), charge, unplug, press PS. Bluedroid can't share a firmware with the lights' NimBLE, so
it is one or the other.

## Accepted as is

- **No auto-dim from the light sensor** (decided 2026-10-07): the backlight is full by day and
  about a third at night on both decks (by the clock, 18:00-06:00), never from the CYD's light
  sensor (GPIO 34).
- **Decks stay on the network they joined** (tried 2026-10-14 and 2026-10-22, removed both
  times): scanning for the hotspot from home WiFi paused the S3's network task, and the iPhone
  hides its hotspot unless its settings screen is open, so the scan rarely saw it. Restarting the
  deck with that screen open is what works; in the car only the hotspot exists.

## Ideas

- **Car lights from CarPlay** (researched 2026-10-23, wireless CarPlay in the City). In order:
  (1) Siri from the steering-wheel button running the voice Shortcut. Nothing to build, so try
  it on the next drive. Wireless CarPlay uses 5 GHz and the decks 2.4 GHz, so the hotspot should
  hold; keep Maximize Compatibility on. (2) Tap buttons via a Shortcuts widget on the CarPlay
  dashboard (iOS 26 widgets). This is unproven: Apple's CarPlay guide says a widget can't open a
  non-CarPlay app, and only in-widget buttons work. Test with one shortcut (Get Contents of
  `/say?k=…&text=red`) before making a set, and if it works keep it to 4-5 colours plus Off.
  (3) A real CarPlay app ("driving task" category) needs a Mac, a $99/yr developer account and
  Apple's approval of the CarPlay entitlement, and offers only fixed list/grid templates.
- **Yellow deck, more pages** (spec `docs/superpowers/specs/2026-10-23-yellow-deck-menu-design.md`):
  Car over BLE (once a BLE OBD adapter arrives), Heart, LED effects.
- **When Siri gets "Sorry"**, the Shortcut asks Claude to turn the phrase into a command
  ("something cozy" → "purple 30") and sends that instead.
- **Phone remote, more buttons** (Howl built 2026-10-13): another sound, skip/pause through the
  deck, a screen flash.
- **Musixmatch as a second lyrics source** (parked 2026-10-07). Official API free plan: API
  key, ~2k calls/day, only a ~30% untimed preview (full/synced lyrics are paid, licensed).
  Unofficial desktop endpoint: full synced lyrics, but bypasses their API auth (against
  Musixmatch's terms), can be blocked any time, token tied to an account; large JSON responses
  would need a second stream parser and another TLS host.
- **Bigger status panel when a song has no lyrics** (parked 2026-10-07; current layout kept).
  Instead of the dialogue box, the status panel grows into the bottom of the screen: (A)
  everything scales up, or (B) the name row unchanged, a large time-remaining readout next to a
  2x walker, a thicker HP bar.
- Car power: brownout behaviour on noisy USB during engine start (`[boot] reset reason`
  says "brownout" if it happens).
- Touch controls on the black deck (play/pause/skip). The unused groundwork was removed in the
  2026-10-08 cleanup; see git history.
- Korean / Cyrillic lyrics (blank on the black deck; Korean shows as boxes on the yellow deck).
