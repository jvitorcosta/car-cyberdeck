# Architecture

Two FreeRTOS tasks share one mutex-protected state. The UI loop only draws; the network task
does every download and decode. See [HISTORY.md](HISTORY.md) for why it is built this way.

```
core 0: network task (src/core/nettask.cpp)            core 1: UI loop (src/main.cpp)
  Spotify /me/player 4 s, kept-open conn (429 wait)     snapshot AppState every frame
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
- **Lyrics** (`lyrics/lrclib`, `util/lrcstream`, `util/lyricbuf`) — LRCLIB `/api/get`, then
  `/api/search`; synced beats plain; retries on server errors; the dialogue box shows
  Pokémon-style status messages (`ui/lyricmsg`) when there is no line to show.
- **Genre badge** (`genre/apple`, `util/genre`, `util/genrebadge`) — Apple artist genre, cached
  per artist, mapped to Gen-3 style badges.
- **Night mode** (`util/daynight`, `net/clock`, `ui/backlight`) — 18:00–06:00 local time
  (SNTP; `TIMEZONE` in `src/config.h`, daylight saving included): "Moonlit battle" palette (`ui/theme` `NIGHT`) and a dimmer backlight.
- **Greeting** (`audio/greeting`) — boot sound: the embedded `data/greeting.pcm` (16-bit mono
  16 kHz) played through the ES8311 (`board/s3/sink_es8311`) in a one-shot task that frees the
  driver when done; `tools/greeting_clip.py` falls back to the default chime at build time.
  The boot opening picks the clip: the Honda's drive
  plays `data/greeting.pcm`, McQueen's `data/greeting_alt.pcm` (`tools/gen_greeting.py --alt`),
  the kiss none (`greeting::choose`, `util/intropick::greetingFor`).
- **Boot opening** (`util/intropick`) — `setup()` picks one of three openings each boot:
  the kiss video, the Honda's night drive or McQueen's. Random, never last boot's (NVS
  `boot`/`last`), and only from what the build has (the clip, `data/mcqueen.bin`); the Honda is
  always there. `-D INTRO_FORCE=\"kiss\"` (or `honda`, `mcqueen`) forces one for testing. The car
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
  320×24 strip (15 KB, freed before the montage); brake: whole-screen fade; montage: JPEG frames
  from the embedded `data/montage.bin` decoded straight to the display (~10 fps); drive ~15 fps (67 ms/frame). `setup()`
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
  countdown; the network task stays quiet that long (`netplan::PollBackoff`).
- **Music box** (`util/idlewait`, `util/musicbox`, `util/tunes`, `audio/greeting`) — "Waiting for
  a song..." shows when Spotify has nothing, or once the song has stayed paused 30 s. A
  music box then loops `data/tunes.txt` (packed by `tools/tune_clip.py` from `data/tunes/*.txt`,
  git-ignored, else `data/tunes_default.txt`): plucked sine notes on 4 voices at 5 % of the clip volume (`TUNE_LEVEL`),
  synthesised in a 4 KB clip task; one tune, 3 s rest, another at random. It stops with a 300 ms fade when the
  screen goes, starts only with the howl's TLS margin, stops when the largest block nears
  `TLS_NEED`, and retries at most once a minute. `tools/gen_tune.py` makes tunes from MIDI.
- **Firmware updates** (`net/ota`, `util/otaview`, `util/backdrop`, `ui/otascreen`, `util/bootguard`)
  — ArduinoOTA on port 3232 (password REMOTE_TOKEN), its own task. `otaview::Tracker` keeps the
  update's state (Receiving, Done, Failed held 3 s); ArduinoOTA's own reboot is off, so the deck
  restarts 600 ms after the end, once the screen has flashed red. While Receiving or Done the
  network task pauses; a failure resumes it at once. With `data/slzbg.bin` (Star Light Zone, from
  `tools/gen_slz_backdrop.py`, media partition) the screen is that night with the red rail
  filling and Shadow skating its tip (`util/ridersprite`, `data/shadow.bin` from
  `tools/gen_shadow_rider.py`: his Sonic Battle run; two jet glows without it),
  else the battle box with its EXP bar.
- **Phone remote** (`net/remote`, `util/remotegate`, `util/howl`, `util/statejson`,
  `util/qrfit`) — off unless `REMOTE_TOKEN` is set. A page on port 80 (`http://cardeck.local/`,
  mDNS; `172.20.10.10` on the iPhone hotspot, `util/hotspotip`) served by its own idle-priority
  task, never the UI loop. It reads `GET /state` every 5 s (now playing, the deck's status,
  network, the sound buttons) and has a Home Screen icon (`GET /icon.png`, public). `POST
  /play?clip=howl` checks the token, one play per 3 s, `409` while a clip plays. The
  howl is the real recording in `data/howl.pcm` (`tools/gen_howl.py`, 48 kHz) when the build has
  one; otherwise it is synthesised while it plays (`audio/greeting`).
  `/state` is built into a 2 KB buffer (`statejson::MAX_JSON`). The "Waiting for a
  song..." screen shows the link as a QR code (`esp_qrcode` from the core; `qrfit` builds
  `http://<ip>/?k=<token>` from the current address and picks a version ≤ 3), checked when the
  screen appears and every 5 s. It sits on a Gen-3 battle box at the top left (`ui/qrcard`:
  phone icon and "REMOTE", 3 px modules on a cream panel, 2-module quiet zone, x 4-127,
  y 22-149), painted once with the screen and again only when the address changes; without a
  link the scene is painted back over it.
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
- **Network choice** (`net/wifi`, `util/{roam,follow,deckaddr,netup,hotspotip}`) — config.h
  order (the hotspot first; two tries after a drop from it, `roam`), then a deck stays put. The
  companion reaches the S3 at its fixed hotspot address or by mDNS (`cardeck`), keeps that
  address through blips (`deckaddr`), and after 30 s without the S3 (`follow`) moves to the
  other network if a quick scan finds it. Online means connected with an address; "connected
  without an address" reconnects on the second check in a row (`netup`).
- **Pure, host-tested modules** (suites under `test/`, one per module except `lyricstatus` in
  `netplan`'s, `pollpace` in `lyricclock`'s and `version` in `bios`'s) — `ui/{theme,icon_map,
  accents,cjkfont,status_sprite,statusscene,qrcard,typebadge,lyricmsg,pokeball,car_sprite,carlook,montage}`,
  `pokemon/dex`, every module in `src/util/`: `{animdata,artmap,avatarbin,avatarlines,
  avatarscene,avatarsing,bios,bootanim,carbin,ctxcache,daynight,deckaddr,dexset,follow,genre,
  genrebadge,glyphrun,hotspotip,howl,idlesprite,interp,intropick,introclock,jsonflat,linkstatus,loudness,
  lrcstream,lyricbuf,lyricclock,lyricstatus,menunav,netdiag,netplan,netup,phonepress,playerjson,jsonpath,
  pollpace,press,qrfit,readguard,remotegate,rgb565,roam,statejson,stateparse,termblend,text,touchguard,
  touchmap,tripclock,version,walkanim,walkrect,weather}`.

## Layers

- `src/board/s3/` (the deck), `src/board/cyd/` (the companion) — everything board-specific
  (display bus, audio path, backlight); pins in `include/pins.h`. The CYD's standalone deck build
  was retired on 2026-10-21 (git history up to `d4d0a67`).
- `src/util/`, `src/core/`, `src/images/` never include `src/ui/`; `src/util/` has no Arduino
  dependency, so it builds and tests on a PC.
