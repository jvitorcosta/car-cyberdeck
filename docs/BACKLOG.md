# Backlog

Open items, not scheduled. Newest decisions first within each section.

## Verification (on device)

- **Gauge cluster in the car** (works over Bluetooth Classic, 2026-10-19): compare RPM, speed,
  coolant (from 67), volts and km/l with the City's own cluster (km/l ~14-18 at a steady
  80 km/h); AT START caught when waiting ~5 s at key-on; readable at a glance. The adapter must
  be free: phone Bluetooth off, and after a lost link a replug may be needed (the deck now
  restarts its Bluetooth after a failed connect, `811a022`; reconnect not yet seen working).
- **Idle-screen QR on the hotspot**: seen on the home WiFi only (`http://192.168.100.13/...`);
  check it shows and scans `http://172.20.10.10/?k=...` on the iPhone hotspot, day and night.

- **Old deck touch layer is faulty** (2026-10-14): one constant press (raw x=0, y~3977) under
  both the companion and `hwcheck`, stylus taps never register. The avatar menu is driven from
  the phone; touch stays in code (with the stuck-press guard) for a working board.

- **Phone remote on the CYD**: Howl from Safari on the iPhone hotspot works on the S3
  (2026-10-13: page from 172.20.10.1, howl played in 2.4 s). Not yet tried on the CYD. The
  planned refusal below ~32 KB largest heap block is the 503 guard in `net/remote`; watch
  `[mem]` on the CYD while howling during a track change.
- **Animated status screens**: seen on the S3 (Spotify busy + countdown); not yet on the CYD,
  and the no-WiFi / no-internet / certificate screens only from host renders.
- **Half-dead link** (WiFi up, no mobile data): the iPhone hotspot works (2026-10-08), but this
  path has never run on the board. Hotspot needs "Maximize Compatibility" on (2.4 GHz only).

- **Companion screen timing** (runs on the old deck since 2026-10-14): not yet measured. Check
  the lyric line follows the S3 within ~0.3 s, **Looking for the deck...** within ~2 s of
  unplugging the S3 and recovery within ~5 s, and that `[mem]` stays stable over 10 min.

## Investigate

- **Stuck on "Nothing playing" / TLS -76** (first 2026-10-14, still on 2026-10-19/20): the S3
  logs `ssl_client ... (-76) UNKNOWN ERROR CODE` (MBEDTLS recv failed) after most polls. Usually
  harmless (polls still finish), but once, after ~21 min uptime, every request failed while the
  phone played: stuck on "Nothing playing" until a reboot. Not root-caused; logging (204 vs
  non-track type) was added. Next: reproduce with a long capture and the port closed (see the
  HWCDC serial stall note), correlate with `[mem]`.

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

- Running `cyd` (the CYD's own Spotify build) next to the S3 shares one Spotify app, so both
  share its rate limit (both were blocked for ~2 h on 2026-10-13). A second app for the CYD
  would split it. The companion (`cyd_companion`) has no Spotify client and doesn't count.
- Retry-After costs one extra request per 429 (`spclient::fetchRetryAfter`) because
  SpotifyArduino skips the headers; `tools/patch_spotify_lib.py` could capture it instead.

## Lyrics polish (deferred minors, final review 2026-10-08)

- Search can never match a track whose duration is 0 (lyrics wouldn't advance anyway: the
  position is clamped to the duration).
- `test_all_messages_fit_two_lines` checks bytes, not rendered pixel width (worst case: "...").

## Genre badge polish (deferred minors, final review 2026-10-08)

- Test gaps vs the spec: real Apple answers for Pop 14 / Brasileira 1122, every badge's colour,
  and "network errors are not cached" (device code in nettask).

## Gauge cluster polish (deferred minors, final review 2026-10-19)

- `obd::allowed()` accepts a command without its trailing `\r` (never sent; harmless joins):
  require exactly one.
- The RPM line's box (`SUB`, 14 px) is 2 px shorter than font 2: a sliver of the inverted warning
  can linger below it.
- The supported-PID scan runs once per connection: a silent ECU at key-on loses the
  `[obd] supported:` line (rescan on the first Live reply).
- Pedal/throttle learning never narrows: a pedal resting above ~17 % raw never counts as
  released for the fuel-cut fallback (the City reports FuelSys, so it rarely matters).
- No stack high-water log for the 6 KB OBD task after the scan (measured ~3.4 KB free).
- `ATSP0`'s automatic search may probe other protocols first (read-only; `ATSP6` would avoid it
  but isn't allow-listed).

## Boot opening polish (deferred minors, final review 2026-10-19)

- `main.cpp`: the kiss path's `greeting::play()` comment claims a wait that the NONE greeting
  doesn't do.
- The montage's opener can queue ~0.1 s behind the intro sound's tail (the kiss log showed the
  intro sound on time).
- `termblend::composite` forms an out-of-block pointer when a block doesn't overlap the panel
  horizontally (never dereferenced); an early return is also cheaper.
- `carbin` accepts any car height though the scene assumes 66 px (only the tool enforces it).
- Stale comments: bootscene.h "a silver Honda", intro.h "before everything else", bootscene.cpp
  "black until the montage", an overlong line in main.cpp.
- If the intro task can't be created (out of memory), the kiss waits for WiFi on a black screen
  without the terminal.

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

- Phone page on the home WiFi (diagnosed 2026-10-19): the router isolates the 2.4 GHz clients
  ("Cecilia", 192.168.100.1, the ISP's fibre box), so no device can reach the deck there (ping
  fails, no ARP). Not a firmware fault; turn off AP/client isolation in the router to use it at
  home. The car has only the hotspot, where it works.
- Backlight stays at full brightness (decided 2026-10-07): no auto-dim from the CYD light
  sensor (GPIO 34), even though it was suggested for night glare.

## Ideas

- **Fuel level on the gauge cluster** (2026-10-19): the City answers "not supported" for PID 2F on all
  three ECUs, so FUEL % and KM LEFT show `--`. Options: (1) if Car Scanner shows fuel level, it
  uses a Honda-specific read request: allow exactly that one read in `obd::allowed()` (owner's OK
  needed); (2) the cluster's CAN broadcast, found in the gear side task's listen-only session;
  (3) estimate: "filled up" on the phone page, then subtract the MAF-measured fuel burnt, kept in NVS.
  The owner checks Car Scanner first.

- **Gear on the gauge cluster** (side task, 2026-10-19; spec 2026-10-19-gauge-cluster): the City's
  lever (P R N D S) and the CVT's virtual gear (1–7, paddles) aren't standard OBD values; they're
  broadcast on the CAN bus for the car's own instrument cluster. Record a parked session with the ELM327 listening only
  (`ATMA` / filtered `ATCRA`, never `ATCSM0`), shifting P → R → N → D → S and the paddles, find the
  message and byte, then feed the cluster's empty lever row. Those commands join `obd::allowed()`
  only in that task's own design. Fallback for the virtual gear: RPM ÷ speed matched to the 7 steps.

- **Avatar, next** (home screen built 2026-10-14): more menu items (Weather, Map); the
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
- **OBD, next** (the gauge cluster is built, `feat/obd`): a check-engine warning (stored-code
  count from mode 01 PID 01, read-only); reading the codes themselves needs mode 03 in the
  allow-list (owner's OK; clearing codes stays out: the deck never writes to the car); a CAR
  button on the phone page; trip logging; fuel level and gear (above).

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
- Touch controls (play/pause/skip) on the S3, whose touch works (the music box's speaker key,
  `board/s3/touch_ft6336`). The old groundwork (Spotify play/next/prev/volume calls, button
  widgets) was removed in the 2026-10-08 cleanup; see git history.
- Korean / emoji / Cyrillic lyrics (render blank today; spec non-goal).
