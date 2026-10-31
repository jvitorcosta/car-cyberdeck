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
- **Half-dead link** (WiFi up, no mobile data): the iPhone hotspot works (2026-10-08), but this
  path has never run on the board. The hotspot needs "Maximize Compatibility" on (2.4 GHz only).

## Code health

- **Split `src/yellow/ledpanel.cpp`** (~3 000 lines): the link task, the web server, the lyrics
  and music tasks and the weather are each a candidate file under `src/yellow/`. Wait until the car
  health work (`feat/car-health`, which adds screens to this file) is merged.
- **`feat/obd`** predates the 2026-10-28 reorganisation and ran on the retired companion. Its OBD
  core moves to the yellow deck in `feat/car-health` (`src/util/car/`); retire the branch once that
  merges. (`feat/voice-agent` is already on the new layout.)
- **CI doesn't build the diagnostics**: only `ledpanel` and `s3` are in the firmware matrix, so
  `blescan` / `bletest` stopped linking without anyone noticing. Add `blescan`, `bletest`,
  `xboxtest`, `ps3test` and `hwcheck` / `hwcheck_s3` (and the mic envs once the voice agent merges),
  or a weekly job that builds them.

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

- **Next, when the new deck arrives:** `docs/superpowers/plans/2026-10-31-voice-agent-next.md`:
  make it speak first, then side brain, music and scenes, the car, WhatsApp.
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
