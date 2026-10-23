# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Verification (on device)

- **Yellow deck menu** (built 2026-10-23, `feat/yellow-menu`, only compiled; the deck and watch
  weren't connected). Flash with `pio run -e ledpanel -t upload`, sideload the watch app, then:
  - it boots to the green Menu; LIGHTS opens, BACK returns; every tile, zone, `-` and `+` hits
    where tapped; a controller switched off turns its part of STATUS into `--` within ~10 s;
  - `/say` changes the lights while the Menu shows, and the strip follows;
  - presets: tapping ROSE / FIRE / CALM / PURPURA on the Menu and on Lights applies both lights
    (no hold, nothing saved); Xbox A on one applies it; `/presets` returns 4 lines
    (`1 ROSE pink 40 pink 100` ...) and `/status` returns `inside=… outside=…`; the blocks fit;
  - voice: "rose", "fire", "calm", "purpura" and "preset one".."four" apply and answer
    (`Rose: inside Pink 40 percent, outside Pink 100 percent`); "rose 50" is only ROSE. Siri may hear
    "purpura" as "purple": say "preset four" for that one;
  - Xbox: A applies a preset; **B is now Back** (was: BOTH page);
  - restyle: scanlines, the `> [LIGHTS]` row, the Pip-Boy tabs, the colour boxes with the big chip, the
    10-cell level bar (%, MIX, OFF) read well; the typed RobCo boot takes ~0.5 s and a `/say` during
    it isn't lost; the block cursor blinks on the menu strip;
  - messages: `>ROSE APPLIED`, `>INSIDE: NO LINK`, `>OUTSIDE: LINKED` show ~2.5 s and the strip
    comes back; the strip shows real colours (`IN PINK 40%  OUT PINK 100%`, `OFF`, `--`);
  - night mode: a `-D NIGHT_TEST` build drops the backlight (~90/255) and the screen is **amber**
    (no green left, no bright frame), the 8 tiles stay distinguishable and the ink readable; the
    clock shows `--:--` then `HH:MM` and refreshes each minute without flicker; a normal build
    switches by itself around 18:00 and stays bright green by day;
  - fonts and layout (GLCD header at 8 px, strip positions) read well on the panel;
  - watch: the PRESETS page lists the four names, START applies ROSE and BOTH shows the new
    looks, and a controller switched off shows "Not connected" after reopening the app.

- **Star Light Zone update screen** (built 2026-10-22, `feat/shadow-update`, only compiled): first
  USB `pio run -e s3 -t upload` and `pio run -e s3 -t uploadmedia` (the new `data/slzbg.bin` and
  `data/shadow.bin`), then an update over WiFi (`pio run -e s3_ota -t upload`): the night fills
  the screen, the rail fills red with Shadow skating its tip, a red flash at 100%, the reboot. Cut the PC's upload
  mid-way: grey rail and "Update failed" for ~3 s, then the deck, polling again.

- **Car LEDs** (branch `feat/car-leds`): **tested in the car 2026-10-22, owner: "they worked"**,
  the lights and voice (Siri) on the final build (NimBLE 2, pages, debounced saves). The Garmin app
  works against the deck (tested at home over the hotspot); in the car it takes the same path, so
  it's assumed to work there too, not seen yet. Still open, not reported either way:
  - the LED colours (orange, pink, purple, white and yellow are guesses for RGB LEDs) and whether
    the perceptual 10 % brightness steps look even;
  - INSIDE / OUTSIDE not swapped (the controller addresses were assigned by signal strength);
  - the Xbox controller with the lights connected;
  - memory over a long drive (heap's low point drifted 130 → 123 KB over 25 min at home).

- **Old deck touch works with the pen** (2026-10-21; was "faulty" on 2026-10-14): at rest the
  panel reads a press at raw x=0 (y~4000, z~600), which tripped the stuck-press guard for good.
  `companion/touch` now treats raw x < 100 as released, and `TOUCH_RAW_*` were recalibrated from
  pen taps (x 532..3477, y 590..3513). Taps register under `ledpanel`; the companion's own
  touch menu hasn't been retried yet.

- **Animated status screens**: seen on the S3 (Spotify busy + countdown); the no-WiFi /
  no-internet / certificate screens only from host renders.
- **Half-dead link** (WiFi up, no mobile data): the iPhone hotspot works (2026-10-08), but this
  path has never run on the board. Hotspot needs "Maximize Compatibility" on (2.4 GHz only).

- **Companion screen timing** (runs on the old deck since 2026-10-14): not yet measured. Check
  the lyric line follows the S3 within ~0.3 s, **Looking for the deck...** within ~2 s of
  unplugging the S3 and recovery within ~5 s, and that `[mem]` stays stable over 10 min.

## Investigate

- **Dropped WiFi receive buffers.** The WiFi driver can't get a ~1.5 KB buffer (caps 0x1800)
  and drops the frame; TCP resends it and every request still succeeds, but each drop adds
  latency. Rate varies a lot by network and song: 1 800-4 900 in 5 min on the iPhone hotspot
  (2026-10-08). Findings: it happens during every TLS request, plain polls included, not only
  around track changes; only one TLS session was open at a time then (the old Spotify library
  closed its client after each call; since 2026-10-21 the Spotify connection stays open, so
  re-measure on the S3); even 218 B allocations fail, so the heap is
  momentarily empty while a TLS session (~40 KB) is open on ~65 KB free. The levers left are
  build-time settings fixed in the precompiled Arduino core (mbedTLS 16 KB input buffer, lwIP
  TCP window, WiFi RX buffer count): they need the arduino-as-ESP-IDF-component build.

- **DMA for the S3 intro** (blocked, 2026-10-15): pushing frames by DMA from two buffers ran
  at ~36 ms a frame, but TFT_eSPI's `initDMA()` on this board left every later draw black, with
  or without `deInitDMA()`. Plain pushes take ~50 ms, so some frames of the 20 fps clip are
  skipped. Worth another try only with a way to hand the bus back cleanly.

## Spotify rate limit

- One Spotify app is shared by every deck that runs a Spotify client: running the CYD's old
  standalone build next to the S3 got both blocked for ~2 h on 2026-10-13. That build is retired
  (2026-10-21); the companion (`cyd_companion`) has no Spotify client and doesn't count.

## Lyrics polish (deferred minors, final review 2026-10-08)

- Search can never match a track whose duration is 0 (lyrics wouldn't advance anyway: the
  position is clamped to the duration).
- `test_all_messages_fit_two_lines` checks bytes, not rendered pixel width (worst case: "...").

## Genre badge polish (deferred minors, final review 2026-10-08)

- Test gaps vs the spec: real Apple answers for Pop 14 / Brasileira 1122, every badge's colour,
  and "network errors are not cached" (device code in nettask).

## SD card cache (removed 2026-10-21)

The parked SD cache for walk sheets and sprites was removed; it is in git history up to
`0ba409b`. If repeat songs ever need it, the S3's SDMMC slot (pins in `include/pins.h`) is
the place to add it.

## Accepted as is

- Backlight stays at full brightness (decided 2026-10-07): no auto-dim from the CYD light
  sensor (GPIO 34), even though it was suggested for night glare.

## Ideas

- **Yellow deck menu, parts 3-7** (spec
  `docs/superpowers/specs/2026-10-23-yellow-deck-menu-design.md`): Weather, Car over BLE (once the
  Vgate iCar Pro BLE arrives), Heart, Music, LED effects. Parts 1-2 (menu, favourites, night mode)
  are built.
- **Car lights from the Garmin watch**: done 2026-10-22, see docs/leds-voice.md.
- **Car lights from CarPlay** (researched 2026-10-23, wireless CarPlay in the City). In order:
  (1) Siri from the steering-wheel button running the "Honda" Shortcut. Nothing to build, so try
  it on the next drive. Wireless CarPlay uses 5 GHz and the decks 2.4 GHz, so the hotspot should
  hold; keep Maximize Compatibility on. (2) Tap buttons via a Shortcuts widget on the CarPlay
  dashboard (iOS 26 widgets). This is unproven. Apple's CarPlay guide says a widget can't open a
  non-CarPlay app, and only in-widget buttons work (Music play/pause, Reminders). A Shortcuts
  button that runs a URL in place *might* work. Test with one shortcut ("Honda Red", Get Contents
  of `/say?k=…&text=red`) before making a set, and if it works keep it to 4-5 colours plus Off.
  (3) A real CarPlay app ("driving task" category) needs a Mac, a $99/yr developer account and
  Apple's approval of the CarPlay entitlement, and offers only fixed list/grid templates (no music
  wave).

- **Avatar, next** (home screen built 2026-10-14): more menu items (Weather, Map, OBD); the
  Claude assistant (Siri Shortcut + a `POST /say`); blinking; VROOM on the new deck.
- **PS3 controller for the avatar menu** (side idea, 2026-10-14): over Bluetooth Classic on the
  CYD (the S3 has BLE only), library `esp32-ps3`; D-pad = cursor, X = open, O = back. A cable
  needs a USB host (MAX3421E module + wiring): the CYD's USB port is a device-only CH340.
  **2026-10-22, waiting on a battery:** João's original DS3 is already paired to the yellow deck
  (its master address set over USB to the CYD's Bluetooth address, 68:09:47:F8:25:9E, no
  SixaxisPairTool needed) and the `ps3test` env (Bluedroid) is ready, but its battery is dead. A
  genuine DS3 only powers on from a USB host, not a wall charger, and on the PC it stays wired.
  Next: a new LIP1472 (3.7 V 570 mAh), charge, unplug, press PS. Bluetooth Classic means Bluedroid,
  so it can't share a firmware with the LED panel's NimBLE (Xbox + lights): one or the other.
- **Switch back to the hotspot automatically** (tried 2026-10-14, then removed): scanning on
  the home WiFi paused the S3's network task and blipped the companion, and the move back was
  never seen working (joins were refused, likely "Allow Others to Join" off). Owner: decks stay
  where they joined; home WiFi is dev only, the car has only the hotspot. Tried again in the LED
  panel on 2026-10-22 (a short passive scan every 20 s) and removed again: the iPhone hides its
  hotspot unless its settings screen is open, so the scan rarely saw it; restarting the deck with
  that screen open is what worked.

- **Companion dashboard, the rest of the spec** (cut from v1 on 2026-10-13, "just lyrics"):
  a top strip with the clock and outside weather (`util/weather` is committed, not wired in).
  Howl, Vroom and trip reset are built (the avatar menu and its phone page), and the trip time
  has its own screen. Plan: `docs/superpowers/plans/2026-10-13-companion-screen.md`.
- **Retro map** on the companion, located by the iPhone over HTTPS (spec section "Optional
  extra: retro map"): gets its own plan.
- **OBD2 car info** over a Bluetooth ELM327 dongle, after the companion.

- **Phone remote, more buttons** (Howl built 2026-10-13, see ARCHITECTURE): another sound,
  skip/pause through the deck, a screen flash. After the hotspot check above.
- **Musixmatch as a second lyrics source** (parked 2026-10-07). Official API free plan: API
  key, ~2k calls/day, only a ~30% untimed preview (full/synced lyrics are paid, licensed).
  Unofficial desktop endpoint (`apic-desktop.musixmatch.com`, `macro.subtitles.get` +
  usertoken): full synced lyrics, but bypasses their API auth (against Musixmatch's terms),
  can be blocked any time, token tied to an account; large JSON responses would need a
  second stream parser and another TLS host on a memory-tight board.
- **Bigger status panel when a song has no lyrics** (parked 2026-10-07; current layout kept).
  Instead of the dialogue box, the status panel grows into the bottom of the screen. Two
  mocked-up variants: (A) everything scales up: name/badge/time in the large font, Poke Ball
  and walker at 2x (pixel doubling), 14-16 px HP bar; (B) name row unchanged, a large
  time-remaining readout next to a 2x walker, thicker HP bar. The lyrics status
  (lyricstatus::Status::None / Instrumental) now reports that final state.
- Car power: brownout behaviour on noisy USB during engine start (`[boot] reset reason`
  now says "brownout" if it happens).
- Touch controls (play/pause/skip). The unused groundwork (Spotify play/next/prev/volume
  calls, button widgets) was removed in the 2026-10-08 cleanup; see git history.
- Korean / emoji / Cyrillic lyrics (render blank today; spec non-goal).
