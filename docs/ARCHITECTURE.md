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
- **Boot scene** (`ui/bootscene`, `ui/car_sprite`, `ui/montage`, `util/bootanim`,
  `util/loudness`) — own task at boot; cruising: sky drawn once, y 72–239 re-rendered through one
  320×24 strip (15 KB, freed before the montage); brake: whole-screen fade; montage: JPEG frames
  from the embedded `data/montage.bin` decoded straight to the display (~10 fps); drive ~15 fps (67 ms/frame). `setup()`
  waits for it; `loop()` and the late-WiFi path wait while active.
- **Status screens** (`util/linkstatus`, `ui/statusscene`, `ui/status_sprite`) — when
  Spotify can't be polled: whose fault it is (no WiFi, no internet, Spotify error, rejected
  certificate) plus a bundled Pokémon per cause on the battle platform with a speech bubble,
  4 fps, pushed in 4-row strips. A 429 shows **Spotify busy...** with Spotify's `Retry-After`
  countdown; the network task stays quiet that long (`netplan::PollBackoff`).
- **Phone remote** (`net/remote`, `util/remotegate`, `util/howl`) — off unless `REMOTE_TOKEN`
  is set. A one-button page on port 80 (`http://cardeck.local/`, mDNS) served from the UI
  loop; `POST /play?clip=howl` checks the token, one play per 3 s, `409` while a clip plays,
  and the howl is synthesised while it plays (`audio/greeting`).
- **Companion screen** (`companion/{app,link,dashboard}`, env `cyd_companion`) — a second CYD
  build without Spotify, Pokémon or audio code. A network task polls the S3's
  `GET /state?k=<token>` about once a second (`util/pollpace`), parses it with
  `util/{jsonflat,stateparse}` and hands the UI loop a snapshot; the UI loop picks the line to
  show from the song position plus the time since the poll (`util/lyricclock`) and draws it at
  2x (`drawRunScaled`). The S3 side builds `/state` in `util/statejson` and keeps the trip in
  `util/tripclock`. v1 is lyrics only; `util/weather` is in place for the dashboard.
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
- **Pure, host-tested modules** (one suite each under `test/`) — `ui/{theme,icon_map,accents,
  cjkfont,status_sprite,statusscene,typebadge,lyricmsg,pokeball,labels,car_sprite,montage}`,
  `pokemon/dex`, `util/{animdata,walkanim,walkrect,netplan,lyricstatus,interp,text,glyphrun,
  lrcstream,lyricbuf,artmap,ctxcache,dexset,genre,genrebadge,rgb565,daynight,bootanim,loudness,
  bios,linkstatus,howl,remotegate,tripclock,jsonflat,statejson,stateparse,lyricclock,pollpace,
  weather,avatarbin,avatarlines,avatarscene,avatarsing,menunav,press,touchmap,touchguard,
  phonepress,netup,roam,follow,deckaddr}`.

## Layers

- `src/board/cyd/`, `src/board/s3/` — everything board-specific (display bus, audio path,
  backlight); pins in `include/pins.h`. The rest of the code is shared.
- `src/util/`, `src/core/`, `src/images/` never include `src/ui/`; `src/util/` has no Arduino
  dependency, so it builds and tests on a PC.
