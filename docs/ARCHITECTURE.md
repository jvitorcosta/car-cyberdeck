# Architecture

CarDeck is three programs that meet on the phone's hotspot: the **black deck** (ESP32-S3, env
`s3`, `src/black/`), the **yellow deck** (ESP32 CYD, env `ledpanel`, `src/yellow/`) and the
**Honda City** watch app (`garmin/carlights/`). A fourth, the **OBD deck** (a plain ESP32, env
`obddeck`, `src/obd/`), reads the car for the health check (being built on `feat/car-health`).
Code the decks share lives in `src/common/` and `src/util/`. See [HISTORY.md](HISTORY.md) for why it is built this way.

```
Spotify, LRCLIB, iTunes, PokéAPI (HTTPS)
               |
        +-------------+ GET /music, /karaoke (signed)  +-------------+   BLE    +-----------------+
        | black deck  | <----------------------------- | yellow deck | -------> | 2 LED controllers|
        | ESP32-S3    |                                | ESP32 CYD   | <------- | PS3 controller  |
        | env s3      | -----------------------------> | env ledpanel|   BLE    +-----------------+
        +-------------+   UDP 4210 palettes (signed)   +-------------+
               ^                                         ^         ^
               | phone page, k=                                    | signed, via Garmin Connect
         iPhone (Safari, Home Screen)                         Honda City watch app
```

Paths below drop the deck folder: on the black deck `core/nettask.cpp` is
`src/black/core/nettask.cpp`; `net/*` and `ui/backlight` are in `src/common/`; `util/x` is
`src/util/<group>/x` (the groups are under [Layout](#layout)).

## Black deck

Two FreeRTOS tasks share one mutex-protected state. The UI loop only draws; the network task
does every download and decode.

```
core 0: network task (core/nettask.cpp)                core 1: UI loop (main.cpp)
  Spotify /me/player 4 s, kept-open conn (429 wait)     snapshot AppState every frame
  per track, one step at a time (util/netplan):           redraw deck on new track generation
    art -> lyrics -> genre -> walker -> prefetch          take art / lyrics / genre / walker
  results tagged with trackGen                            animate: walker 8 fps, CD 6 fps, HP 4 fps
                 \                                        /
                  +---- core/shared.cpp (one mutex) -----+
                        AppState + per-track mailbox
```

Other tasks: the phone remote's web server (`net/remote`, core 1, idle priority), music mode's
UDP sender (`audio/musicsvc`, started on demand), the update listener (`net/ota`, core 0), and at
boot the boot scene and intro video (core 1) and the one-shot sound task.

Its web server (port 80) answers `GET /` (the phone page) and `GET /icon.png`, both public;
`GET /state`, `GET /music`, `GET /karaoke`, `GET /playlists` and `GET /playlists/play`, which take
`k=<REMOTE_TOKEN>` or a signature (see [Between the devices](#between-the-devices)); and `POST /play?clip=howl`, `POST /trip/reset` and
`GET /music/debug`, which take `k=` only.

- **UI loop** draws only: battle layout (`ui/screen_now`, `ui/battle`, `ui/icons`), album
  art JPEG decode, walker, CD, lyric.
- **Network task** does every HTTPS request and PNG decode. It never touches the display.
- **Shared state** — one FreeRTOS mutex; `AppState` snapshots plus a mailbox whose results
  carry the track generation they were fetched for, so a quickly skipped song's art or
  lyrics are dropped instead of shown on the next one.
- **Spotify** (`spotify/client`, `util/playerjson`, `util/jsonpath`) — one `GET /v1/me/player`
  per poll gives the song, its progress and the device (volume, shuffle, repeat). The
  connection to `api.spotify.com` stays open between polls (one TLS handshake, not one per
  request); a stale one is retried once on a fresh connection, a 401 refreshes the access token
  and retries. A 429's `Retry-After` comes from the same answer. The body lands in a 16 KB
  PSRAM buffer and is read by dotted paths (`item.album.images.1.url`). The playlist name is a
  second request on the same connection, once per context (`util/ctxcache`).
- **Walker** (`images/walksprite`) — PMD SpriteCollab walk cycle (Right-facing row) cropped
  to a shared bounding box and fitted to a 32 px band. Its slots, the 64 KB download scratch,
  the PNG decoder and the album-art bitmap live in PSRAM (`mem::bigAlloc`), leaving internal
  RAM to mbedTLS and the WiFi driver. Two slots: the UI reads the active
  one while the network task loads the next song's walker into the staged one. Fallback:
  the PokeAPI sprite with a bob/flip fake walk.
- **Pokémon** — random #1–1025 from a bundled Pokédex (`pokemon/dex`, generated).
- **Lyrics** (`lyrics/lrclib`, `util/lrcstream`, `util/lrcmatch`, `util/lyricbuf`) — LRCLIB
  `/api/get`, then `/api/search` for the title without its suffix unless get found synced lines
  or an instrumental; entries must match by duration and title; synced beats plain; retries on
  server errors; the dialogue box shows
  Pokémon-style status messages (`ui/lyricmsg`) when there is no line to show.
- **Genre badge** (`genre/apple`, `util/genre`, `util/genrebadge`) — Apple artist genre, cached
  per artist, mapped to Gen-3 style badges.
- **Night mode, black deck** (`util/daynight`, `net/clock`, `ui/backlight`) — 18:00–06:00 local time
  (SNTP; `TIMEZONE` in `src/config.h`, daylight saving included): "Moonlit battle" palette (`ui/theme` `NIGHT`) and a dimmer backlight.
  The yellow deck uses the same schedule for its backlight only.
- **Greeting** (`audio/greeting`) — boot sound: the embedded `data/greeting.pcm` (16-bit mono
  16 kHz) played through the ES8311 (`board/s3/sink_es8311`) in a one-shot task. From cold the
  speaker gets 400 ms to wake before the first sample (with 60 ms the deck's own mic heard a clip's
  first ~0.3 s missing; `-D AUDIO_PROBE` logs that test); after a 16 kHz clip the output stays on,
  sending silence, for 20 s, so the next clip starts at once; `tools/greeting_clip.py` falls back to the default chime at build time.
  The boot opening picks the clip: the Honda's drive
  plays `data/greeting.pcm`, McQueen's `data/greeting_alt.pcm` (`tools/gen_greeting.py --alt`),
  the kiss none (`greeting::choose`, `util/intropick::greetingFor`).
- **Boot sound rosters** (`util/sndroster`, `util/soundpick`, `audio/greeting`) — the Honda's
  drive greeting, the montage start and the Honda logo sound each come from a roster:
  `data/drive.snd`, `opener.snd` and `finale.snd` (git-ignored), built by `tools/gen_rosters.py`
  from the WAVs listed in `data/rosters.txt` (git-ignored; format in `data/rosters.example.txt`):
  up to 32 clips of up to 4 s each, 16-bit mono 16 kHz, trimmed at -45 dB, faded out over 20 ms
  and levelled to 0.12 RMS (peaks capped at 95 %). `setup()` (`pickSounds`) picks one per
  slot at random, never last boot's (NVS `boot`/`drive`, `opener`, `finale`): the logo's first,
  then a montage start among the clips that end before it starts (`bootscene::openerFits`, the
  rule the scene applies); the drive's only for the Honda's opening. A slot without a roster plays
  its single file (`greeting.pcm`, `opener.pcm`, `finale.pcm`). A fourth roster, `connect.snd`
  (NVS `connect`), is the WiFi-connected chime: the boot scene cues it (`greeting::playConnect`)
  once it's online and the greeting is over, and leaves the cruise (or, after a video, starts the
  montage) when it has played, at most 3 s later; it has no single file, so no roster means no
  chime. Serial: `[boot] sounds: drive=… opener=… finale=… connect=…`.
- **Sound timing** (`bootanim::SOUND_LEAD_MS`, `introclock::elapsed`, `greeting::level`) — a clip
  is heard ~0.45 s after it's cued from cold (task, codec bring-up, the speaker's wake), ~10 ms
  while the output is still on from the last clip (`greeting::warm`), so the pictures follow
  the sound, not the cue: an intro video's frames run from its sound's first sample (frame 0 holds
  up to 0.8 s for it), the drive's car lights follow the greeting as heard, the montage start is
  cued that much (warm or cold) before the first photo (in the brake's fade) and the logo sound that much before
  its frame. Serial: `[greeting] <clip>: out N ms after its cue`.
- **Boot opening** (`util/intropick`) — `setup()` picks one of four openings each boot:
  your intro video ("kiss"), the Honda's night drive, McQueen's, or the second video ("max").
  Random, never last boot's (NVS `boot`/`last`), and only from what the build has (the clips,
  `data/mcqueen.bin`); the Honda is always there. `-D INTRO_FORCE=\"kiss\"` (or `honda`,
  `mcqueen`, `max`) forces one for testing. The car
  openings start their drive before WiFi connects; every opening ends in the montage.
- **Intro video** (`ui/intro`, `util/introclock`, `util/termblend`) — the kiss opening,
  on its own task while WiFi connects and the network task logs into Spotify underneath; then
  straight to the montage. The boot terminal types over it: `ui/bootterm` draws into a 206×60
  sprite, and `termblend` paints it into each decoded JPEG block (glyphs over a half-bright panel,
  by shifts: a per-channel blend cost 5 ms a frame), so the video never wipes it. If the deck
  isn't online when the clip ends, the last frame holds with `WI-FI ... RETRYING...` until it is
  (`intro::wait()`). `data/intro.bin` (from `tools/gen_intro.py`; `tools/intro_clip.py` writes an
  empty one that turns it off) holds 320×240 baseline JPEG frames in the montage's container, 20
  fps, with `data/intro.pcm` (48 kHz mono) as its sound. Frames are decoded with JPEGDEC and
  pushed without DMA (~50 ms a frame: `initDMA()` left every later draw black); `introclock`
  picks the frame by time, so late frames are skipped instead of slowing the clip. If the task
  can't start, the deck skips the intro.
- **Boot scene** (`ui/bootscene`, `ui/bootterm`, `ui/carlook`, `ui/car_sprite`, `util/carbin`,
  `ui/montage`, `util/bootanim`, `util/loudness`) — own task at boot. The car is a `carlook::Look`:
  the committed Honda, or McQueen from the git-ignored `data/mcqueen.bin` ("CAR1": size, wheel and
  light anchors, kinds and RGB565, from `tools/gen_mcqueen_sprite.py`), each with its own rims.
  `bootterm` is the on-board terminal (the BIOS log) shared with the intro. Without a drive (the
  kiss) the scene waits for WiFi and starts at the montage. Cruising: sky drawn once, y 72–239 re-rendered through one
  320×24 strip (15 KB, freed before the montage) over a copy of the layers that never move (sky
  band, trees, asphalt; 105 KB in PSRAM), the underglow and lamp light from alpha tables (9 KB);
  brake: the same frame while the LEDC hardware fades the backlight (`backlight::fadeOut`; whole-screen
  pixel fade without it), the first photo relights it; montage: JPEG frames from the embedded
  `data/montage.bin` decoded straight to the display (~10 fps); drive ~19 fps (~52 ms/frame, 24 of
  them the push at 40 MHz; 80 MHz corrupted the picture). `setup()`
  waits for it; `loop()` and the late-WiFi path wait while active.
- **Status screens** (`util/linkstatus`, `ui/statusscene`, `ui/status_sprite`) — when
  Spotify can't be polled: whose fault it is (no WiFi, no internet, Spotify error, rejected
  certificate) plus a bundled Pokémon per cause on the battle platform with a speech bubble,
  4 fps, pushed in 4-row strips. "Waiting for a song..." shows Sonic instead
  (`util/idlesprite`, `data/idle.bin` from `tools/gen_idle_sprite.py`): Sonic 1's waiting
  animation with the game's timing (12 x stand, then the foot tap, 400 ms a frame), from when
  the screen appears; when his frame or the arrow can change only his box is repainted
  (`statusscene::frameRect`; the Pokémon with a bubble or a ball repaint the region). He waits on a
  sidewalk in front of the Sunset drive (`ui/sunset`, procedural, edge to edge, painted once per
  screen; night palette with stars and a moon) instead of the battle platform. A 429 shows **Spotify busy...** with Spotify's `Retry-After`
  countdown; the network task stays quiet that long (`netplan::PollBackoff`). A login Spotify
  refuses (the refresh token revoked or expired) shows **Login failed** / re-run
  `spotify_auth.py`; a refused or failed login is tried again 4 s, 8 s, ... up to every 5 min
  (`netplan::LoginBackoff`), not at every poll.
- **Waiting for a song** (`util/idlewait`) — "Waiting for a song..." shows when Spotify has
  nothing, or once the song has stayed paused 30 s; now and then its second line says how to
  wake a phone that plays on unseen (`PHONE_HINT`). The screen is silent.
- **Update screen** (`ui/otascreen`, `util/otaview`, `util/backdrop`, `util/ridersprite`) — while
  an update comes in (see [Shared](#shared-network-updates-and-staying-up)) the network task
  pauses; a failure resumes it at once. `otaview::Tracker` keeps the update's state (Receiving,
  Done, Failed held 3 s); ArduinoOTA's own reboot is off, so the deck restarts 600 ms after the
  end, once the screen has flashed red. With `data/backdrop.bin` (Green Hill Zone, from
  `tools/gen_backdrop.py`, media partition) the screen is that zone with the red rail filling and
  Shadow skating its tip (`data/shadow.bin` from `tools/gen_shadow_rider.py`; two jet glows
  without it), else the battle box with its EXP bar.
- **Phone remote** (`net/remote`, `util/remotegate`, `util/howl`, `util/statejson`,
  `util/qrfit`) — off unless `REMOTE_TOKEN` is set. A page on port 80 (`http://cardeck.local/`,
  mDNS; `172.20.10.10` on the iPhone hotspot, `util/hotspotip`) served by its own idle-priority
  task, never the UI loop. It reads `GET /state` every 5 s (now playing, the deck's status,
  network, the sound buttons) and has a Home Screen icon (`GET /icon.png`, public). `POST
  /play?clip=howl` checks the token, one play per 3 s, `409` while a clip plays. The
  howl is the real recording in `data/howl.pcm` (`tools/gen_howl.py`, 48 kHz) when the build has
  one; otherwise it is synthesised while it plays (`audio/greeting`).
  `/state` is built into a 2 KB buffer (`statejson::MAX_JSON`) and ends with the firmware's
  version (`"fw"`, util/version), which the status screens also show at the top right. The "Waiting for a
  song..." screen shows the link as a QR code (`esp_qrcode` from the core; `qrfit` builds
  `http://<ip>/?k=<token>` from the current address and picks a version ≤ 3), checked when the
  screen appears and every 5 s. It sits on a Gen-3 battle box at the top left (`ui/qrcard`:
  phone icon and "REMOTE", 3 px modules on a cream panel, 2-module quiet zone, x 4-127,
  y 22-149), painted once with the screen and again only when the address changes; without a
  link the scene is painted back over it.
- **Playlist picker** (`net/remote`, `spotify/client`, `util/playlists`) — the watch's
  `GET /playlists?offset=N` and `GET /playlists/play?id=<id>`, signed. The remote task can't touch
  the Spotify connection, so it leaves the request in a one-slot mailbox and waits (15 s at most);
  the network task runs it between polls (`spclient::serveWatch`, before the next per-track step)
  on the kept-open connection: no TLS handshake of its own, and none at all below `TLS_NEED`
  (`503 low memory`) or while a 429 is being waited out. A page is 8 playlists of
  `/v1/me/playlists` (about 1.5 KB of JSON each, into the 16 KB answer buffer; asked again at 4 when
  long descriptions overflow it), sent as `playlists <total> <next>` and one `<id> <name>` line
  each (names cut to 48 bytes). Play is `PUT /v1/me/player/play` with the playlist as
  `context_uri`. Spotify's refusals come back as `502` with what to do ("Open Spotify on phone" for
  `NO_ACTIVE_DEVICE`, "Needs Spotify Premium", "Re-authorise Spotify" for a missing scope); a
  request the network task didn't reach in time is `504`. The token's scopes are logged once at the
  first login when the playlist ones are missing.
- **Music mode** (`audio/musicsvc`, `util/songmood`, `util/songpalette`,
  `util/musicwire`) — the network task decides each song's colour (`songmood::choose`: the owner's
  picks, a colour word, the playlist description's #colour tags, else a mood's allowed swatches
  from the playlist's name and description or the genre, the cover's colours choosing among them; at the track change, once the cover is in
  and once the genre is known; one `[colour]` log line per song). `GET /music?on=1` (the
  yellow deck, every 10 s, signed: `util/wire/sigreq`) starts or keeps a small task (core 1, 4 KB stack) that sends the song's
  palette (`songmood::lights`: the swatch's screen colour, only the cap follows the time of day;
  a new song's once its cover is in, or 8 s after the track change without one), on change and
  every 10 s, over UDP to the requester's port 4210 (`util/wire/musicwire` v2: a boot id, a rising
  counter and an 8-byte HMAC tag with `REMOTE_TOKEN`; the yellow deck drops a bad tag or a counter
  that doesn't rise). The yellow deck snaps its main colour to one
  of the lights' 8 swatches (`util/musiclights`), which gives back exactly the chosen one. `on=0` or 30 s without a keep-alive stops it and closes
  the socket. No mic: the howl plays as usual (`board/s3/mic_es8311` stays in the tree, unused,
  for the voice agent). It starts only with
  TLS_NEED + 6 KB in the largest block. `GET /music/debug?k=<token>` (`&on=1` also keeps it alive)
  is the readout (on, the palette last sent, its cap, the socket, the largest free block), and a
  `[music]` serial line tells each palette sent for a new song or a change.
- **Karaoke view** (`core/karaoke`, `ui/karaokeview`, `util/black/karaokepick`,
  `util/black/karaokemode`, `util/black/karaokepal`, `util/black/lyricface`) — `GET
  /karaoke?on=1|0` (with `light=N` on `on=1`, the yellow deck's INSIDE light: `karaokemode::parseLight`,
  0..8 or -1; signed like `/music`, `reqauth`; `400
  on=1 or on=0` otherwise; answers `{"on":1}` or `{"on":0}`) is the yellow deck's switch. `on=1` is a
  keep-alive (sent at once and every 10 s while its KARAOKE switch is on); 30 s without one turns
  karaoke off, and `on=0` stops it at once (`karaokemode`). Logged as `[karaoke] on` / `off` on a
  change and `[karaoke] screen karaoke|no lyrics|normal`. While on, `karaokepick` picks the screen
  per song from the lyric status: searching, retrying or synced lyrics give the full-screen karaoke
  view (searching shows the `lyricmsg` searching message in the line's place); plain lyrics,
  instrumental or none give the "no lyrics" message (`lyricmsg`) for 30 s from when it is known,
  then the normal screen until the next song. Pausing keeps the screen; the OTA screen, the
  offline and waiting status screens and the camera band keep their priority. The view ("night
  drive") is dark and frameless in the light's colour (`karaokepal`: termpal's three tones of that
  swatch, pale by day and vivid at night, white for -1, cyan as blue; a change of light or of day and
  night redraws it whole). It has the song, artist and progress bar at the top, the line just sung
  (one row), the current line (Atkinson Hyperlegible Bold, GFX tables in `src/black/fonts` from
  `tools/gfxfont.py`; up to three rows at 22 px, `glyphrun::wrapRows`, else at 18 px, `...` only past
  that) and the next line (one row). LRCLIB times lines, not words, so the current line lights up
  near white evenly from its start to the next line's start (the last line sweeps 5 s or to the
  song's end). The walking Pokémon, at half size (`blitWalker`'s `half`), walks on the current
  line's top edge at the sweep point (on a longer line it stays on row 1, all rows' progress mapped
  onto it) and hops back (a 300 ms, 14 px arc) at each new line. An instrumental break (a
  timed blank line) keeps the last line fully swept; before the first line, it waits unlit. CJK
  and emoji draw at their 16 px among the letters. Karaoke and music mode are independent.

## Yellow deck

One file, `src/yellow/ledpanel.cpp`, with its touch layer (`yellow/touch`), fonts
(`yellow/fonts`, `yellow/ui/termfont`, `gojiraname`) and its pure logic in
`src/util/yellow/` and `src/util/lights/`. The user's manual is [yellow-deck.md](yellow-deck.md).

| Task | Core | What it does |
|---|---|---|
| `loop()` | 1 | owns the screen and the lights' state: touch (`util/{touchcal,touchmap,touchguard,tapguard}`), the PS3 controller's input (`Ps3.data`, written by the Bluetooth task), the screens (Menu, Lights: `util/deckmenu`, `yellow/ui/termlayout`), voice jobs from the server, music mode's looks, the theme (`util/termpal`), night (`util/daynight`, `ui/backlight`, `util/termpal`'s night palette), saves to NVS |
| `leds-link` | 0 | connects the two LED controllers (`yellow/elklink` on Bluedroid's GATT client, sharing the stack with the PS3 controller's Bluetooth Classic; service FFF0, characteristic FFF3, written by handle; frames from `util/elkcmd`) in rounds 5 s apart, doubling to 60 s while one is missing. The PS3 controller connects by itself (PS button) |
| `voice` | 1, idle | joins WiFi (`net/wifi`), then the web server: `/say`, `/state`, `/presets`, `/status`; a `/say` goes to `loop()` through a one-slot queue with a sequence number, and the handler waits for its reply |
| `deck` | 1, idle | 6 KB of stack; sends the black deck its keep-alives: `GET /music?on=1` while music mode is on and `GET /karaoke?on=1` while the KARAOKE switch is on, each every 10 s (`on=0` once when switched off). It reads nothing from the black deck |
| `music` | 1, idle | while music mode is on: a UDP socket on port 4210 that takes the black deck's signed palettes (`util/wire/musicwire`) |

The deck does lights, the karaoke switch, music mode and the watch's routes only: Bluedroid's dual mode
(Classic for the PS3 controller, LE for the lights) left it ~30 KB of heap with WiFi up (~44-52 KB since the lyrics and the `/state` poll moved to the black deck), so the
weather, speed cameras, car health and updates over WiFi were taken off it (the car's health is the
OBD deck's). Each task's stack is sized from the `[mem]` line's high-water marks.

- **Looks** (`util/ledlook`, `util/bothlook`): each page (BOTH, INSIDE, OUTSIDE) has a colour
  swatch, a level and on/off. The swatch table (`ledlook::SWATCHES`) holds each colour's LED mix
  and screen colour: the grid's eight, then the preset-only cyan. Presets: `util/ledpresets`.
  Voice: `util/ledvoice`. Replies and link words: `util/lightstatus`.
- **Main screen** (`util/dashtext`, `util/tripmeter`, `util/lights/tripclock`): one row of the
  clock and date and the trip's time, both lights (a block of each one's colour, its name and level
  in TITLE), the prompt line and three tall items (`[LIGHTS]`, and the KARAOKE and MUSIC MODE
  switches, each with an ON / OFF tag).
- **Karaoke** (the switch only): an on/off state flipped by a tap, the PS3 controller's cross,
  voice or the watch (`util/ledvoice`), not kept over a reboot. The `deck` task sends
  `GET /karaoke?on=1&light=N` (the INSIDE light's shown swatch, -1 off or unlinked) at once, every
  10 s and on a change of light while it is on (`util/yellow/keepalive`); the black deck draws the
  lyrics in that colour.
- **Music mode** (`util/lights/musiclights`): the palette's main colour snapped to one of the
  grid's eight, the inside light at its level under the palette's cap (40 % floor), the outside at 100 %, a ~3 s fade per new
  song. The pages' looks are never saved over while it runs. On and off from the button, voice
  ("music") or the watch; `/status` says `music=on|off`.
- **Network** (`net/wifi`, `util/{roam,netup,hotspotip,deckaddr}`): the black deck at
  `BLACK_DECK_ADDR` on the hotspot, by mDNS (`cardeck`) elsewhere, kept through blips.
- **Memory**: no PSRAM, so everything that downloads uses fixed buffers
  and plain HTTP (no TLS next to Bluetooth and WiFi).
- **Flash**: `min_spiffs.csv`, two 1.9 MB app slots for the boot guard (updates go over USB only,
  `NO_WIFI_UPDATES`); NVS keeps the touch
  calibration (`touch`), the looks and the trip (`leds`).

## OBD deck

A generic ESP32 with no screen (`src/obd/`, env `obddeck`) that owns everything that reads the car:
the yellow deck can't hold a 4th BLE connection on this framework, so the adapter got its own board.
It answers the yellow deck over the hotspot and never writes to the car.

| Task | Core | What it does |
|---|---|---|
| `loop()` | 1 | once a second: a trip sample from the fresh readings (`util/car/tripsum`), the health `Watch` (`carhealth`), the fuel burnt (`fuelbook`), a closed minute to the trip log (`triplog` on LittleFS, `obd/carfs`), then the `/car` snapshot (`carwire`); every 30 s the log flush and the NVS records; the `[car]` line every 10 s |
| `obd-link` | 0 | finds and connects the ELM327 over BLE (NimBLE 2, `util/car/obdble`): the remembered address first, then a 4 s scan; 5 s apart, doubling to 60 s while none answers |
| `obd-ask` | 0 | while connected: the adapter's setup, the supported-PID scan, the PIDs in `obdplan`'s order, `ATRV` at least every 4 s, 03 / 07 every 5 min (`obdplan`); fuel from the air (`fueleco`), stepped only with fresh values |
| `net` | 1, idle | joins WiFi (`net/wifi`), mDNS `cardeck-obd`, SNTP, the update listener, then the web server: `/car`, `/fill` |
| `ota` | 0 | the update listener (`net/ota`) |

- **`REMOTE_TOKEN` empty**: nothing could sign a request, so the deck starts no `net` task: no
  WiFi, no `/car` and `/fill`, no updates over WiFi (`[net] off (REMOTE_TOKEN empty)`). It still
  reads the car and logs the trips; updates go over USB only.
- **Bench build** (`obddeck_bench`): `obd/benchlink.cpp` stands in for `obdlink` with a simulated
  drive (`util/car/benchdrive`). `/car` carries `"sim":1`; NVS is
  `carsim` and there's no trip log, so a real deck's records stay.
- **Read-only**: every write to the adapter goes through `obd::allowed()` (mode 01, 03, 07 and the
  adapter's setup; never mode 04 or `ATH1`).
- **Which device** (`util/car/obdpick`): the whole scan is weighed. An adapter-like name wins
  (strongest signal), a nameless serial-service device only when no name matched; a scanned
  device that failed is skipped for 10 min (the last 4 kept): one with an adapter-like name only
  after a failed setup (a missed connect is retried next round), a nameless one after either. The lights
  (their MACs from `config.h`, `ELK` / `MELK` names; their controllers serve FFF0 too) never.
- **NVS**: `obd` holds the adapter (`bleaddr`, `bletype`, `blename`); `car` holds the boot sequence
  (`tseq`, which names the trip file `/t<seq>.jsonl`), the last trip with the health memo (`last`,
  so a trip cut by the ignition is judged at the next boot), the fuel book (`fuel`) and the last
  trouble-code counts (`codes`, so a code is "new" against the last read).
- **Flash**: `min_spiffs.csv`; the 128 KB data partition (32 blocks of 4 KB) holds the trip log
  (LittleFS: at most 96 KB of trips, 20 files and 70 % of the blocks, oldest dropped first, at boot
  and when a write fails; a row cut by a power cut is trimmed at boot, an empty file removed).
- **Snapshot**: `checkReady` once a 03 / 07 read and a battery reading are in, or 20 s after the
  link went live; `seq` +1 per snapshot (once a second).

## Between the devices

| From → to | What | Contract |
|---|---|---|
| yellow → black | `GET /karaoke?on=1` / `on=0` (signed; a keep-alive every 10 s while on, 30 s without one turns it off) | `net/remote`, `util/black/karaokemode`; the screen per song is `util/black/karaokepick`, from `util/wire/lyricstatus` |
| yellow → black | `GET /music?on=1` / `on=0` (signed); the reply proves the black deck's boot id for the request's nonce | `audio/musicsvc`, `util/wire/musicwire::readBoot` |
| black → yellow | UDP port 4210: the song's palette (v2: boot id, rising counter, 8-byte HMAC tag) | `util/wire/musicwire`, `util/wire/songpalette`; the colour is `util/black/songmood`'s |
| watch → yellow | `GET /say`, `/state`, `/presets`, `/status` (signed) | replies from `util/lightstatus`, `util/ledpresets` (wire lines), `util/ledvoice` |
| watch → black | `GET /playlists?offset=N` (a page: `playlists <total> <next>`, then `<id> <name>` lines), `GET /playlists/play?id=` (signed); `502` + Spotify's refusal in words | `util/black/playlists`; read by the watch's `Tunes.mc` |
| a link → yellow | `GET /say?k=<REMOTE_TOKEN>&text=...` | bearer key, plain HTTP |
| phone → black | the page `/?k=`, then `/state` and `POST /play` with `k=` | bearer key, plain HTTP |

- **Signing** (`util/wire/sigreq`, `util/wire/hmac256`, `common/net/reqauth`; on the watch
  `Sign.mc`): `t=<unix s>&n=<nonce>&sig=`, the first 16 bytes of HMAC-SHA256 with `REMOTE_TOKEN`
  over `GET|path|sorted params|t|n`. A deck accepts a signature within 2 minutes of its clock,
  once (a ring of the last 32 nonces). Devices never send the key. **The bearer `k=` is still
  accepted** on the same routes, because a plain link and the Home Screen page can't compute an
  HMAC. Details and limits: [SECURITY.md](../SECURITY.md).
- **Addresses**: on the iPhone hotspot the black deck pins `172.20.10.10`, the yellow deck
  `172.20.10.11` and the OBD deck `172.20.10.12` (`util/hotspotip`); elsewhere `cardeck.local`,
  `cardeck-leds.local` and `cardeck-obd.local`.
- **Versions**: both decks report their build (`util/version`, from `tools/version.py`): the
  black deck in `/state`'s `"fw"`, the yellow deck in `/status`'s `fw=`. A protocol change means
  updating every device together, and a major version.

## Shared: network, updates and staying up

- **Network choice** (`net/wifi`, `util/{roam,deckaddr,netup,hotspotip}`) — config.h
  order (the hotspot first: three tries at boot, two after a drop from it, `roam`); on a fallback
  a deck scans for the hotspot once a minute and moves to it when it's in range. The yellow deck
  reaches the S3 at its fixed hotspot address or by mDNS (`cardeck`) and keeps that address
  through blips (`deckaddr`). Online means connected with an address; "connected
  without an address" reconnects on the second check in a row (`netup`).
- **Firmware updates** (`net/ota`, `util/otaview`, `util/bootguard`) — ArduinoOTA on port 3232 on
  the black deck and the OBD deck (the yellow deck takes USB only, `NO_WIFI_UPDATES`, and keeps
  the boot guard), password `OTA_PASSWORD` (off without one), its own task. The upload writes the other
  app slot and switches only once its SHA-256 checks out. The bootloader holds new firmware
  pending (IDF app rollback) until it has run a minute online with its listener up
  (`ota::onOnline`, `[ota] update confirmed`): **keep a deck powered for about a minute after an
  update**, as any reset before that boots the previous firmware. After that, `bootguard` counts
  crash boots in nvs until the app's own sign of working (the black deck: a good Spotify poll; the
  yellow deck: a check while online) and goes back after 3 in a row (`[ota] rolled back`).
- **Watchdogs** (`common/core/watchdog`) — the task watchdog with a 90 s timeout, above the
  longest legitimate wait (an S3 request's DNS, connect, TLS and body; a BLE connect plus a GATT
  discovery). The black deck's network task and the yellow deck's link, voice, deck and music
  tasks are watched; both `loop()`s use the core's loop watchdog. A hang reboots the deck
  within 90 s.
- **Back-offs** — the black deck waits out Spotify's 429 (`netplan::PollBackoff`) and retries a
  refused login 4 s, 8 s, ... up to every 5 min (`netplan::LoginBackoff`); the yellow deck retries
  a missing light 5 s apart, doubling to 60 s.

## Layout

- `src/black/`, `src/yellow/`, `src/obd/` — each deck's own code. Each env compiles only its deck's folder,
  `src/common/` (`net/`, `ui/backlight`, `board/board.h`, `core/embed.h`, `core/watchdog.h`) and its
  util groups (allow-list `build_src_filter` in `platformio.ini`), so a call into the other deck's
  code fails to link. `src/diag/` holds the standalone checks, one env each (`hwcheck`,
  `hwcheck_s3`, `blescan`, `bletest`, `ps3test`).
- `src/black/board/s3/` — everything board-specific on the black deck (display bus, audio path,
  backlight); pins in `include/pins.h`.
- `src/util/` has no Arduino dependency, so it builds and tests on a PC; `src/util/`,
  `src/black/core/` and `src/black/images/` never include a `ui/` folder. Its modules, by who uses
  them:
  - `wire/` (the contracts between the decks): hmac256, lyricstatus, musicwire, sigreq,
    songpalette, statejson
  - `lights/` (the LED domain, both decks): hue, ledlook, musiclights, tripclock
  - `car/` (the car health check: the OBD deck):
    carhealth, carwire, fuelbook, fueleco, healthview, obd, obdble, obdpick, obdplan, triplog, tripsum
  - `common/` (both decks): bootguard, daynight, hotspotip, jsonflat, jsonpath, netdiag, netup,
    otaview, readguard, remotegate, rgb565, roam, text, version
  - `black/`: animdata, artmap, backdrop, bios, bootanim, carbin, ctxcache, dexset, genre (+
    genre_data.inc), genrebadge, glyphrun, howl, idlesprite, idlewait, interp, introclock,
    intropick, karaokemode, karaokepick, linkstatus, sndroster, soundpick, loudness, lrcmatch, lrcstream, lyricbuf, mediapack, netplan, playerjson, qrfit,
    quietboot, ridersprite, songmood (+ songpicks.inc), termblend, walkanim, walkrect
  - `yellow/`: bothlook, dashtext, deckaddr, deckmenu, elkcmd, httphead,
    ledpresets, ledvoice, lightstatus, tapguard, termpal, touchcal, touchguard,
    touchmap, tripmeter
- **Host tests** (`test/`, one Unity suite per module, `tools/run_tests.ps1`): every module in
  `src/util/` (`lyricstatus` is tested in `netplan`'s suite, `version` in `bios`'s), plus the pure
  drawing and data modules `black/ui/{accents,car_sprite,carlook,cjkfont,emojifont,icon_map,
  lyricmsg,montage,pokeball,qrcard,status_sprite,statusscene,sunset,theme,typebadge}`,
  `black/pokemon/dex`, `yellow/ui/{gojiraname,presslook,termfont}` and the CA
  bundle.
- `garmin/carlights/` — the watch app ([garmin/README.md](../garmin/README.md)).

## Test hooks

Build flags that are never set in `platformio.ini`; add one for a desk check, then clear it
(`$env:PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST"`, then `Remove-Item Env:PLATFORMIO_BUILD_FLAGS`).

| Flag | Deck (env) | What it does |
|---|---|---|
| `-D INTRO_FORCE=\"kiss\"` (or `honda`, `mcqueen`, `max`) | black (`s3`) | Forces that boot opening (`main.cpp`, `util/intropick`). |
| `-D SOUND_FORCE=\"drive:3,opener:0,finale:11\"` (any subset) | black (`s3`) | Forces those boot sound roster indices (`main.cpp` `pickSounds`, `util/soundpick`). |
| `-D OTA_DEMO` | black (`s3`) | The update screen on a loop with no upload: 0-99 %, Done, then Failed (`main.cpp`). |
| `-D NIGHT_TEST` | yellow (`ledpanel`) | Night whatever the time, synced clock or not: the night backlight and palette, with a serial tuner to try night colours and the backlight live (`docs/yellow-deck.md` "Night"; `ledpanel.cpp` `tuneLine`). |
| `-D THEME_TEST=<0..7>` | yellow (`ledpanel`) | Forces that swatch's terminal theme whatever the inside light does (`ledpanel.cpp`). |

