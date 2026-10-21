# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Verification (on device)

- **Old deck touch layer is faulty** (2026-10-14): one constant press (raw x=0, y~3977) under
  both the companion and `hwcheck`, stylus taps never register. The avatar menu is driven from
  the phone; touch stays in code (with the stuck-press guard) for a working board.

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
  around track changes; only one TLS session is ever open (SpotifyArduino closes its client
  after each call, the steps run one at a time); even 218 B allocations fail, so the heap is
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
- Retry-After costs one extra request per 429 (`spclient::fetchRetryAfter`) because
  SpotifyArduino skips the headers; `tools/patch_spotify_lib.py` could capture it instead.

## Lyrics polish (deferred minors, final review 2026-10-08)

- Search can never match a track whose duration is 0 (lyrics wouldn't advance anyway: the
  position is clamped to the duration).
- `test_all_messages_fit_two_lines` checks bytes, not rendered pixel width (worst case: "...").

## Genre badge polish (deferred minors, final review 2026-10-08)

- Test gaps vs the spec: real Apple answers for Pop 14 / Brasileira 1122, every badge's colour,
  and "network errors are not cached" (device code in nettask).

## Parked: SD card cache (not used)

Decided 2026-10-07: the deck runs without a card. The cache code stays; with no card every
call is a no-op and the walker is downloaded each time (~25 KB/song, usually prefetched).

- What a card would add: walk sheets (`/pmd/<dex>.xml|.png`) and fallback sprites
  (`/sprites/<dex>.png`) load in ~0.2 s on repeat songs, with no download. Album art and
  lyrics are never cached.
- Never verified: SD (VSPI) alongside the display (moved to HSPI in `2b75c40`), cache hits,
  and corrupt-file eviction.
- To test: build with `PLATFORMIO_BUILD_FLAGS=-DSD_CACHE` (without it the card is never
  mounted), FAT32 card in, power-cycle, look for `[cache] SD ready`, replay songs and compare
  `[net] walk` times; check `/pmd` and `/sprites` on a PC.

## Accepted as is

- Backlight stays at full brightness (decided 2026-10-07): no auto-dim from the CYD light
  sensor (GPIO 34), even though it was suggested for night glare.

## Ideas

- **Avatar, next** (home screen built 2026-10-14): more menu items (Weather, Map, OBD); the
  Claude assistant (Siri Shortcut + a `POST /say`); blinking; VROOM on the new deck.
- **PS3 controller for the avatar menu** (side idea, 2026-10-14): over Bluetooth Classic on the
  CYD (the S3 has BLE only), library `esp32-ps3`; one-time pairing with SixaxisPairTool;
  D-pad = cursor, X = open, O = back. A cable needs a USB host (MAX3421E module + wiring): the
  CYD's USB port is a device-only CH340.
- **Switch back to the hotspot automatically** (tried 2026-10-14, then removed): scanning on
  the home WiFi paused the S3's network task and blipped the companion, and the move back was
  never seen working (joins were refused, likely "Allow Others to Join" off). Owner: decks stay
  where they joined; home WiFi is dev only, the car has only the hotspot.

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
