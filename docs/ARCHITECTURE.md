# Architecture

Two FreeRTOS tasks share one mutex-protected state. The UI loop only draws; the network task
does every download and decode. See [HISTORY.md](HISTORY.md) for why it is built this way.

```
core 0: network task (src/core/nettask.cpp)            core 1: UI loop (src/main.cpp)
  Spotify poll 4 s / player 12 s (429: Retry-After)      snapshot AppState every frame
  per track, one step at a time (util/netplan):           redraw deck on new track generation
    art -> lyrics -> genre -> walker -> prefetch          take art / lyrics / genre / walker
  results tagged with trackGen                            animate: walker 8 fps, CD 6 fps, HP 4 fps
                 \                                        /
                  +---- src/core/shared.cpp (one mutex) -+
                        AppState + per-track mailbox
```

- **UI loop** draws only: battle layout (`ui/screen_now`, `ui/battle`, `ui/icons`), album
  art JPEG decode, walker, CD, lyric.
- **Network task** does every HTTPS request and PNG decode. It never touches the display.
- **Shared state** — one FreeRTOS mutex; `AppState` snapshots plus a mailbox whose results
  carry the track generation they were fetched for, so a quickly skipped song's art or
  lyrics are dropped instead of shown on the next one.
- **Walker** (`images/walksprite`) — PMD SpriteCollab walk cycle (Right-facing row) cropped
  to a shared bounding box and fitted to a 32 px band. Two slots: the UI reads the active
  one while the network task loads the next song's walker into the staged one. Fallback:
  the PokeAPI sprite with a bob/flip fake walk.
- **Pokémon** — random #1–1025 from a bundled Pokédex (`pokemon/dex`, generated).
- **Lyrics** (`lyrics/lrclib`, `util/lrcstream`, `util/lyricbuf`) — LRCLIB `/api/get`, then
  `/api/search`; synced beats plain; retries on server errors; the dialogue box shows
  Pokémon-style status messages (`ui/lyricmsg`) when there is no line to show.
- **Genre badge** (`genre/apple`, `util/genre`, `util/genrebadge`) — Apple artist genre, cached
  per artist, mapped to Gen-3 style badges.
- **Night mode** (`util/daynight`, `net/clock`, `ui/backlight`) — 18:00–06:00 local time
  (SNTP; `TIMEZONE` in `src/config.h`, daylight saving included): "Moonlit battle" palette (`ui/theme` `NIGHT`) and a dimmer backlight.
- **Greeting** (`audio/greeting`) — boot sound: the embedded `data/greeting.pcm` (16-bit mono
  16 kHz, played at 32 kHz: IDF 4.4 DAC mode mis-clocks below ~20 kHz) streamed by I2S0 to
  the built-in DAC on GPIO 26 at 30 % volume, noise-shaped down to the DAC's 8 bits, in a
  one-shot task that frees the driver when done; `tools/greeting_clip.py` falls back to the default chime at build time.
  The S3 plays it through the ES8311, and the boot opening picks the clip: the Honda's drive
  plays `data/greeting.pcm`, McQueen's `data/greeting_alt.pcm` (`tools/gen_greeting.py --alt`),
  the kiss none (`greeting::choose`, `util/intropick::greetingFor`).
- **Boot opening** (`util/intropick`, S3 only) — `setup()` picks one of three openings each boot:
  the kiss video, the Honda's night drive or McQueen's. Random, never last boot's (NVS
  `boot`/`last`), and only from what the build has (the clip, `data/mcqueen.bin`); the Honda is
  always there. `-D INTRO_FORCE=\"kiss\"` (or `honda`, `mcqueen`) forces one for testing. The car
  openings start their drive before WiFi connects; every opening ends in the montage.
- **Intro video** (`ui/intro`, `util/introclock`, `util/termblend`, S3 only) — the kiss opening,
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
  320×24 strip (15 KB, freed before the montage); brake: whole-screen fade; montage: JPEG frames
  from the embedded `data/montage.bin` decoded straight to the display (~10 fps); drive ~15 fps (67 ms/frame). `setup()`
  waits for it; `loop()` and the late-WiFi path wait while active.
- **Status screens** (`util/linkstatus`, `ui/statusscene`, `ui/status_sprite`) — when
  Spotify can't be polled: whose fault it is (no WiFi, no internet, Spotify error, rejected
  certificate) plus a bundled Pokémon per cause on the battle platform with a speech bubble,
  4 fps, pushed in 4-row strips. On the S3 "Nothing playing" shows Sonic instead
  (`util/idlesprite`, `data/idle.bin` from `tools/gen_idle_sprite.py`): Sonic 1's waiting
  animation with the game's timing (12 x stand, then the foot tap, 400 ms a frame), from when
  the screen appears; the scene redraws when his frame or the arrow can change. He waits on a
  sidewalk in front of the Sunset drive (`ui/sunset`, procedural, edge to edge, painted once per
  screen; night palette with stars and a moon) instead of the battle platform. A 429 shows **Spotify busy...** with Spotify's `Retry-After`
  countdown; the network task stays quiet that long (`netplan::PollBackoff`).
- **Music box** (`util/idlewait`, `util/musicbox`, `util/tunes`, `audio/greeting`) — "Nothing
  playing" shows when Spotify has nothing, or once the song has stayed paused 30 s. On the S3 a
  music box then loops `data/tunes.txt` (packed by `tools/tune_clip.py` from `data/tunes/*.txt`,
  git-ignored, else `data/tunes_default.txt`): plucked sine notes on 4 voices at a tenth of the clip volume, synthesised in a
  4 KB clip task; one tune, 3 s rest, another at random. It stops with a 300 ms fade when the
  screen goes, starts only with the howl's TLS margin, stops when the largest block nears
  `TLS_NEED`, and retries at most once a minute. `tools/gen_tune.py` makes tunes from MIDI.
- **Phone remote** (`net/remote`, `util/remotegate`, `util/howl`, `util/statejson`) — off unless `REMOTE_TOKEN` is set. A page on port 80 (`http://cardeck.local/`,
  mDNS; `172.20.10.10` on the iPhone hotspot, `util/hotspotip`) served by its own idle-priority
  task, never the UI loop. It reads `GET /state` every 5 s (now playing, the deck's status,
  network, the sound buttons) and has a Home Screen icon (`GET /icon.png`, public). `POST
  /play?clip=howl` checks the token, one play per 3 s, `409` while a clip plays. On the S3 the
  howl is the real recording in `data/howl.pcm` (`tools/gen_howl.py`, 48 kHz) when the build has
  one; otherwise, and always on the CYD, it is synthesised while it plays (`audio/greeting`).
  `/state` is built into a 2 KB buffer (`statejson::MAX_JSON`).
- **Companion screen** (`companion/{app,link,dashboard}`, env `cyd_companion`) — a second CYD
  build without Spotify, Pokémon or audio code. A network task polls the S3's
  `GET /state?k=<token>` about once a second (`util/pollpace`), parses it with
  `util/{jsonflat,stateparse}` and hands the UI loop a snapshot; the UI loop picks the line to
  show from the song position plus the time since the poll (`util/lyricclock`) and draws it at
  2x (`drawRunScaled`). The S3 side builds `/state` in `util/statejson` and keeps the trip in
  `util/tripclock`. It started as lyrics only (v1); the avatar home below added the screens and
  buttons. `util/weather` is in place but not wired in.
- **Avatar home** (`companion/{avatarview,screens,touch,phone,app}`, `util/{avatarbin,
  avatarlines,avatarscene,avatarsing,menunav,press,touchmap,touchguard,phonepress}`) — companion
  only. `data/avatar.bin` (from `tools/gen_avatar.py`; `tools/avatar_clip.py` writes an empty one
  that turns it off) holds 92×92 RGB565 frames per mood, drawn 1× or pixel-doubled from flash
  through a row buffer. Input: the companion's own page (`POST /press?b=..&k=..`, its own
  WebServer task) and the XPT2046 touchscreen on VSPI (`press`: tap on release, 1 s hold;
  `touchguard` ignores a press stuck in one spot). Both feed one `fire(button)`; `menunav` holds
  the screens and buttons. Actions go to the S3 through `companion/link` (POST `/play`,
  `/trip/reset`). Her lines come from `avatarlines` and type out in the message bar
  (`avatarscene`); she sings along by `avatarsing`.
- **OBD gauges** (`companion/{obdlink,gauges}`, `util/{obd,obdplan}`) — companion only: a task
  owns `BluetoothSerial` (master), discovers the ELM327 by name, connects with its PIN, sends
  `ATZ ATE0 ATL0 ATS0 ATSP0`, then asks mode 01 PIDs in `obdplan`'s order (RPM and speed each
  cycle, the slow values every 2 s, unsupported ones every 30 s) and publishes a snapshot under
  a lock. `obd` parses replies tolerantly (echo, SEARCHING..., CAN headers, several ECUs) and
  never sends mode 03/04. The app shows `gauges` while the link is live (5 s hold, `obdplan`),
  her home otherwise; she says when the car comes and goes.
- **Network choice** (`net/wifi`, `util/{roam,follow,deckaddr,netup,hotspotip}`) — config.h
  order (the hotspot first; two tries after a drop from it, `roam`), then a deck stays put. The
  companion reaches the S3 at its fixed hotspot address or by mDNS (`cardeck`), keeps that
  address through blips (`deckaddr`), and after 30 s without the S3 (`follow`) moves to the
  other network if a quick scan finds it. Online means connected with an address; "connected
  without an address" reconnects on the second check in a row (`netup`).
- **Pure, host-tested modules** (suites under `test/`, one per module except `lyricstatus` in
  `netplan`'s, `pollpace` in `lyricclock`'s and `version` in `bios`'s) — `ui/{theme,icon_map,
  accents,cjkfont,status_sprite,statusscene,typebadge,lyricmsg,pokeball,car_sprite,carlook,montage}`,
  `pokemon/dex`, every module in `src/util/`: `{animdata,artmap,avatarbin,avatarlines,
  avatarscene,avatarsing,bios,bootanim,carbin,ctxcache,dacstream,daynight,deckaddr,dexset,follow,genre,
  genrebadge,glyphrun,hotspotip,howl,idlesprite,interp,intropick,introclock,jsonflat,linkstatus,loudness,
  lrcstream,lyricbuf,lyricclock,lyricstatus,menunav,netdiag,netplan,netup,nowplaying,obd,obdplan,phonepress,
  pollpace,press,readguard,remotegate,rgb565,roam,statejson,stateparse,termblend,text,touchguard,
  touchmap,tripclock,version,walkanim,walkrect,weather}`.

## Layers

- `src/board/cyd/`, `src/board/s3/` — everything board-specific (display bus, audio path,
  backlight); pins in `include/pins.h`. The rest of the code is shared.
- `src/util/`, `src/core/`, `src/images/` never include `src/ui/`; `src/util/` has no Arduino
  dependency, so it builds and tests on a PC.
