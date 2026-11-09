# Changelog

All notable changes to CarDeck: the black deck, the yellow deck and the watch app. The format follows [Keep a Changelog](https://keepachangelog.com),
and versions follow [semantic versioning](https://semver.org) (README "Versioning").

## [Unreleased]

### Both decks
- **Changed:** the decks wait longer for the iPhone hotspot at boot (three 8 s tries, time to
  unlock the phone) before falling back to another network, and a deck on the fallback moves to
  the hotspot once a minute's scan finds it. Before, a hotspot that wasn't advertising yet lost to
  the home WiFi and the deck stayed there.

### Watch app
- **Added:** a PLAYLISTS page (the fifth): START lists your Spotify playlists (a page at a time,
  More for the next, up to 60) and START on one plays it on the phone's Spotify; the page then
  shows "Playing" and its name, or what went wrong ("Open Spotify on phone", "Needs Spotify
  Premium"). It asks the black deck at `172.20.10.10`, so the black deck has to be on the hotspot.

### Yellow deck
- **Changed:** KARAOKE on the main screen is an on/off switch like MUSIC MODE (an `ON` / `OFF` tag;
  a tap or the PS3 controller's cross on the focus toggles it; `>KARAOKE OFF`, and when it goes on
  `>KARAOKE: CONNECTING...` until the black deck answers, then `>KARAOKE ON`, or `>BLACK DECK NOT
  ANSWERING` after two misses in a row while it keeps asking).
  The lyrics are drawn by the black deck now. While it is on the deck sends the black deck `GET
  /karaoke?on=1` at once and every 10 s. Voice and the watch say "karaoke" / "karaoke off" (also
  "karaokê", "caraoquê"). It is not kept over a reboot. Each ask carries the inside light's colour
  (`light=N`, the one its own theme takes, -1 when it is off) for the karaoke view's colour, and a change of
  colour asks at once.
- **Changed:** the main screen's top is one row now (the clock and date left, the trip's time
  right); the lights each get a block of their colour with INSIDE / OUTSIDE and the colour and
  level in large type; the three items are more than twice as tall (106 px), easier to hit while
  driving.
- **Removed:** the KARAOKE screen (the lyrics page), the poll of the black deck's `/state` (the
  heaviest WiFi and heap load the deck had), the main screen's now-playing band and the Japanese
  font (`data/jp16.bin`, `tools/gen_jp_font.py`). The `karaoke` task is the `deck` task (6 KB of stack): it only
  sends the music and karaoke keep-alives, and the `[mem]` line says `deck`. At the desk the free
  heap went from ~31-33 KB (low-water 25.7 KB, largest block 11.7 KB) to ~44-52 KB (low-water
  41 KB, largest block 27.6 KB).
- **Fixed:** the deck restarted when the PS3 controller connected while the heap was short (music
  mode just turned on, a lyrics poll in flight): esp32-ps3 took a 4 KB buffer for its few-byte
  "start sending reports" command and wrote through NULL when that failed. The link now swaps that
  send for one that allocates what the command needs and retries it (`yellow/padhid`).
- **Fixed:** the PS3 controller's taps went missing, X and O the most: on the radio's default
  balance only ~20 of its reports a second came through next to WiFi and the lights, so a quick tap
  fell between two. Bluetooth now has the radio first (~45 a second at the desk, and fewer WiFi
  drops to the black deck, not more), and a tap shorter than the loop's pass is still caught.
- **Fixed:** a finger resting on L2 or R2 changed the preset and buzzed: the triggers count past
  half travel now, by their pressure (`util/padtrigger`), and the junk report a new connection can
  start with (every face button down at once) is dropped.
- **Changed:** a burst of presses sends each light its latest look at most every 300 ms instead of
  every one (the inside controller takes ~7 writes a second, a look is four).
- **Added:** why the deck last restarted: `[boot] reset reason` and `[crash] last run: panic after
  264s: heap 22108 (min 21224, block 5876), 1 failed allocs (last 4112 B), at music ask`, with the
  last six runs kept in NVS for a USB read after a drive (`common/core/crashlog`). A failed
  allocation logs its size as it happens, and `[mem]` comes every 10 s with the largest free block.
- **Removed:** the weather, the speed-camera band, the car-health screens (pre-flight check, trip
  complete, the trip band and the car's warnings), fill-ups by voice and updates over WiFi. With
  the PS3 controller on Bluedroid's dual mode the deck had ~16 KB of heap left and crashed (lwIP
  out of memory) once WiFi joined; without them it keeps ~37 KB (low-water ~30 KB). The deck now
  does the lights, karaoke, music mode, the PS3 controller and the watch's routes; the car's
  health is for the OBD deck's own display. Every update goes over USB
  (`pio run -e ledpanel -t upload`; `ledpanel_ota` is gone); the boot guard stays.
- **Changed:** the main screen shows the trip's time (`0:16`, over `TRIP`) right of the clock, and
  the lyrics button and page are called KARAOKE (`[KARAOKE]`, `>KARAOKE: NO SIGNAL`, the log's
  `[karaoke]`). The link and voice tasks' stacks are smaller (8 → 5 KB, 6 → 4 KB).
- **Added:** the main screen shows what the black deck plays in its middle band: the track over
  the artist, the time and a progress line (`>NOTHING PLAYING` / `>NO SIGNAL` without a song).
- **Changed:** the header is just `HONDA CITY TOURING` and the controller's battery, with room
  between them.
- **Changed:** the PS3 controller's LED 1 means connected and is always lit while it is; the inside
  and outside lights' links moved to LEDs 2 and 3 (4 is still its battery low). Before, with no
  light linked, a connected controller showed no LED at all.
- **Changed:** night mode keeps each theme's own colour, vivid, instead of turning the screen
  amber. From 18:00 to 06:00 the backlight drops to about a third and every theme takes a fully
  saturated night version of its hue (pure red, a deep marine blue, a phosphor-terminal green, one
  yellow), picked on the panel; problems stay white on the red, orange and pink themes and red on
  the rest. The scanlines lose their amber tint.

### Black deck (S3)
- **Added:** a full-screen karaoke view, switched by the yellow deck's KARAOKE item: `GET
  /karaoke?on=1|0` (signed like `/music`; `on=1` is a keep-alive every 10 s, 30 s without one or
  `on=0` turns it off; `[karaoke] on` / `off` in the log). It has the yellow deck's terminal look, so
  the two decks match: black, in the yellow deck's theme (the colour of the car's inside light,
  `light=N` with each `on=1`; pale by day, vivid at night; green with the light off) and its VT323
  lettering. It shows the song, artist and progress bar, the line just sung, the current line large
  (three rows at 36 px, else at 24 px, before anything is cut) lighting up as it is sung from its
  start to the next line's start (LRCLIB times lines, not words), and the next line. The song and
  the artist share the top row (the song left, the artist right); the walking Pokémon, full size,
  walks on the progress bar under them at the song's progress, as on the normal screen, so it doesn't
  pull the eye along the lyrics. The bar is a thin line that fills solid as the song plays. Switched
  off and on again during a song without lyrics, it shows the "no lyrics" message again. The normal screen is unchanged.
  A song without synced lyrics shows the "no lyrics" message for 30 s, then the normal screen until
  the next song; the OTA, offline and waiting screens and the camera band keep their priority.
  Karaoke and music mode are independent.
- **Added:** `GET /playlists?offset=N` and `GET /playlists/play?id=<playlist id>` for the watch,
  signed like `/state`: one page of `/v1/me/playlists` as text, and start-playback with the
  playlist as context. Spotify is asked on the network task, on the kept-open connection.
- **Changed:** `tools/spotify_auth.py` asks for `playlist-read-private` and
  `playlist-read-collaborative` too; the boot log says when the token lacks them.
- **Added:** camera alerts for fixed speed cameras, electronic speed bumps and red-light cameras.
  Speed comes from the iPhone's GPS (GPS 2 IP over the hotspot). A speed camera or bump is
  announced at 200 m or 12 s ahead, whichever is further; a red-light camera at 200 m. In the
  last 100 m, going over the limit sounds the alarm until you slow down (red-light cameras never
  alarm). Alerts are a chime and a voice line, with beeps when there is no voice pack
  (`tools/gen_camvoice.py`), and they pause the intro or music. The road sign shows on screen.
  `/cam/mute` silences them. The list comes from the open-radars-manaus releases:
  `python tools/cameras.py push`.
- **Added:** marking a camera from the car. Say "mark camera" (Siri, through the yellow deck): the
  deck saves the spot and answers with a chirp, or a low beep when there's no GPS fix.
  With `MARKS_REPO` and `MARKS_TOKEN` in `config.h`, each mark goes straight online as an issue in
  that private GitHub repo (one sent later when there's no signal), ready for open-radars-manaus'
  review map. The top strip shows a GPS pin, struck through while there's no fix.
- **Added:** a WiFi-connected chime at boot. Once the deck is online and the greeting is over, a
  short chime plays and then the montage starts, so the order is greeting, chime, montage start,
  Honda logo. It comes from a fourth roster, `[connect]` in `data/rosters.txt`; with no
  `[connect]` clips the boot stays silent there.
- **Added:** varied boot sounds. The Honda's drive greeting, the montage start and the Honda logo
  each pick one clip at random from their own roster at every boot, never last boot's: the drive
  says "Bem-vindo, moreno", the ETC voice or one of seven Japanese greetings, and the montage and
  the logo get 14 and 12 anime-style voice lines. The montage start always ends before the logo's
  sound starts. Rosters are built locally with `tools/gen_rosters.py` (git-ignored, like the other
  clips); without them each moment plays its single clip as before. They are levelled to the
  loudness of today's clips, so a dense one (the Bem-vindo over its music bed) no longer rasps.
- **Fixed:** clips and the intro videos' sound no longer lose their first syllable. The deck's own
  mic showed the speaker staying near silent for ~0.3 s after each clip powered it up: it now gets
  400 ms to wake, and stays on for 20 s after a clip, so the chime, montage start and logo sound
  that follow start at once and whole.
- **Changed:** a smoother boot. The night drive runs at ~19 fps (was ~16): the layers that never
  move are drawn once, and the underglow and lamp light come from tables. The brake's dip to black is a backlight fade (10+ frames, was 6 dimmed ones).
  The kiss video plays at 16 fps with every frame (it dropped 28 at 20), and both intro videos'
  sound is levelled to the greetings' loudness (the kiss was twice as loud, Max quieter).
- **Fixed:** boot sounds land on their picture. A sound is heard ~0.45 s after it's cued from cold
  (~10 ms with the speaker already on), so the
  intro videos now run on their sound's clock (lips match the voice), the car's lights follow the
  greeting as it's heard, the montage-start voice is cued in the brake's fade so it comes with the
  first photo, and the logo sound is cued early enough to end with the fade to black.
- **Fixed:** more songs get synced lyrics. A plain-only `/api/get` hit no longer ends the lookup:
  the search runs too, and its synced lines replace the plain ones. The search drops title suffixes
  (" - Remastered 2011", " (feat. X)", " - Ao Vivo") and skips entries of another song, reads up to
  128 KB (LRCLIB's answers grew to ~230 KB, past the old 48 KB cap), still runs when `/api/get`
  answers 5xx, and an instrumental entry no longer hides a later synced one. On 95 real songs, 75
  had synced lyrics before and 89 now. The phone can still show lyrics LRCLIB doesn't have.
- **Fixed:** a synced entry with fewer than 3 lines (one "Nightcall" entry holds only its first
  line) counts as plain: the plain text read before it stays, and the search looks for a full one.

### Yellow deck (CYD, `ledpanel`)
- **Changed:** a PS3 controller (DualShock 3) replaces the Xbox controller. Same layout: the D-pad
  moves the focus, cross presses, circle goes back, square is the next page, triangle on/off,
  L1 / R1 `-` / `+`. It speaks Bluetooth Classic, so the deck moved from NimBLE to Bluedroid in
  dual mode, and the lights' BLE link is now `yellow/elklink` on Bluedroid's GATT client.
  `tools/ds3_pair.py` writes the deck's address into the controller over USB.
- **Added:** controller shortcuts on any screen: START lights on / off, L2 / R2 previous / next
  preset, SELECT music mode, L1 / R1 both lights `-` / `+`, PS back to the main screen. The
  controller buzzes once when a shortcut went out (twice when a light isn't connected), and its LEDs
  show which lights are connected (1 inside, 2 outside; 4 its battery low).
- **Added:** the controller's battery on the main screen's header ("PAD" and four cells).
- **Removed:** Xbox controller support, `lib/xboxpad` and the `xboxtest` diagnostic.
- **Added:** the camera band. It follows the black deck's alerts within about a second, says the
  alerts out loud, and has a mute button.
- **Changed:** the white theme is a softer grey-white.
- **Changed:** music mode runs the outside light at 100 % at any hour.
- **Changed:** presets fade in instead of jumping.
- **Added:** when the deck starts, the lights play a welcome in the last look's colour: the inside
  strip fills from both ends inwards over 4 s (the controller's own curtain effect), then the
  outside light rises, about 6 s in all. ">WELCOME" shows when it plays, and "INTRO" on the serial
  port replays it on the desk (`tools/intro_check.py` checks it).
- **Changed:** night mode keeps the colours vivid. From 18:00 to 06:00, once the clock is synced,
  the backlight drops to about a third and every theme takes a night version of its own hue at full
  saturation (pure red, a deep marine blue, a phosphor-terminal green, one yellow), picked on the
  panel; the dimmed day colours looked washed out. Problems stay white on the red, orange and pink
  themes and red on the rest. No amber palette: each theme keeps its colour.
- **Changed:** the theme follows the inside look even when that light isn't connected, so a colour
  tapped away from the car still shows on the screen.

### Upgrading
- To see private and collaborative playlists on the watch, run `python tools/spotify_auth.py`
  again (it replaces `SPOTIFY_REFRESH_TOKEN` in `src/config.h`) and flash the black deck. Without
  it the list has your public playlists only.
- Rebuild the watch app; the yellow deck is unchanged.

## [2.0.0] - 2026-10-28

The CYD became the **yellow deck**, which drives the car's LED lights, and a Garmin watch app
joined it. **Breaking:** the devices now sign their requests to each other and music mode's
packets, so the black deck, the yellow deck and the watch app must all be updated together; an
old device gets `Wrong key` or has its packets dropped.

### Upgrading
- Black deck: one USB `pio run -e s3 -t upload` (the new partition table: two 3 MB app slots for
  updates and a media partition), then `pio run -e s3 -t uploadmedia`. Settings in nvs survive.
- Yellow deck: one USB `pio run -e ledpanel -t upload` (two OTA slots); the touch calibration and
  the looks survive.
- `src/config.h`: add `OTA_PASSWORD` (its own random string; updates over WiFi are off without
  it), `LED_INSIDE_MAC` and `LED_OUTSIDE_MAC` (the yellow deck doesn't build without them), and
  rename `COMPANION_DECK` to `BLACK_DECK_ADDR` (the old name still works). See
  `include/config.example.h`.
- The watch app: rebuild it with `Secrets.mc` holding the same `REMOTE_TOKEN`.

### Black deck (S3)
- **Added:** a random boot opening each boot, never last boot's: your intro video with the boot
  log typed over it, the Honda's night drive, a pixel McQueen's drive with a second greeting, or a
  second video (`tools/gen_intro.py`, `gen_mcqueen_sprite.py`, `gen_greeting.py --alt`; all
  git-ignored). Sonic waits on "Waiting for a song..." on a sunset sidewalk
  (`tools/gen_idle_sprite.py`). Howl plays a real wolf recording when one is built
  (`tools/gen_howl.py`). Colour emoji in names and lyrics (Twemoji; joined emoji as one picture).
- **Added:** firmware updates over WiFi (`pio run -e s3_ota -t upload`) with an update screen:
  Green Hill Zone with Shadow skating the progress rail (`tools/gen_backdrop.py`,
  `gen_shadow_rider.py`), or a battle box.
- **Added:** song colours for the yellow deck's music mode: each song's colour from your picks
  (`src/util/black/songpicks.inc`), a colour word in the title, your playlist's `#colour` tags, or
  a mood from the playlist or genre with the cover choosing; sent signed over UDP while the yellow
  deck asks (`GET /music`).
- **Added:** the phone remote: `http://cardeck.local/?k=<REMOTE_TOKEN>` (`172.20.10.10` on the
  iPhone hotspot) shows what's playing, the deck's status and a Howl button, opens full-screen
  from the Home Screen, and is on "Waiting for a song..." as a QR code in a battle box. `GET
  /state` also returns the song timing, the lyric lines, the trip clock and the firmware version.
- **Changed:** the media (intro videos, sounds, montage, howl, Sonic, McQueen, CJK font) live in
  their own 9.9 MB flash partition, so a code change flashes ~1.3 MB. Big buffers moved to PSRAM.
- **Changed:** Spotify is one `GET /me/player` per poll on a connection kept open (an own client;
  the SpotifyArduino library and its patch are gone); polls slow to every 10 s while nothing
  plays; a 429 waits as long as Spotify asks.
- **Changed:** a shorter boot (about 20 s to the deck); the status screens say why (No WiFi, No
  internet, Spotify error, Spotify busy with a countdown, Login failed), each with its own
  animated Pokémon; the "Nothing playing" screen is now "Waiting for a song..." and also shows
  once a song has stayed paused 30 s.
- **Fixed:** 320 px walk sheets (Doublade) walk again; a USB serial write no longer freezes the
  network task with no monitor open; "connected" without an address reconnects in seconds; a long
  title no longer fails `/state`; the boot scene's stack; a song playing right after boot no
  longer shows as nothing playing.

### Yellow deck (CYD, `ledpanel`)
- **Added:** the car's two ELK-family LED controllers over Bluetooth LE: colours, levels and
  on/off for inside, outside or both, by touch, an Xbox controller and Siri Shortcuts
  (`GET /say`, English and Portuguese).
- **Added:** the CYBERDECK dashboard (a Seegson-style terminal): clock, outside weather
  (Open-Meteo, located by ip-api.com), the trip, both lights and a prompt line; the Lights page
  with BOTH / INSIDE / OUTSIDE / PRESETS tabs; night mode, and a theme that follows the inside
  light.
- **Added:** six presets (ROSE, FIRE, ゴジラ, PURPURA, CIVIC, OCEAN) by touch, voice, the Xbox
  controller and the watch, and `GET /presets`.
- **Added:** the black deck's lyrics on a Lyrics page, Japanese included (a Unifont subset of the
  kana and level-1 kanji).
- **Added:** music mode: the lights take each song's colour from the black deck, steady with a
  fade per song, capped by the time of day; on and off from the screen, the Xbox controller, voice
  ("music", "music off") and the watch; `/status` says `music=on|off`.
- **Added:** updates over WiFi (`pio run -e ledpanel_ota -t upload`) with an `>UPDATING` page,
  and `FW <version>` at boot and in `/status`.
- **Changed:** GOJIRA's colour is called Cyan everywhere (it was "Light blue" in replies,
  `/state`, `/presets` and on the watch); voice still takes "light blue".

### Honda City watch app (Forerunner 255)
- **Added:** `garmin/carlights`: each zone's colour and level as filled pages, the presets as
  bars, music mode, a glance card; signed requests through Garmin Connect. See
  `garmin/README.md`.

### Security
- Updates over WiFi take their own password, `OTA_PASSWORD`, never `REMOTE_TOKEN` (which travels
  in plain-HTTP URLs); espota no longer runs with `--debug`, which printed it.
- Requests between devices are signed (HMAC-SHA256 with `REMOTE_TOKEN`, a timestamp and a
  one-time nonce) and never carry the key; music mode's UDP palettes carry an HMAC tag, a proven
  boot id and a rising counter. Siri and the phone page keep the bearer `k=`.
- Spotify requests verify the server against 7 bundled root certificates.

### Reliability
- New firmware stays pending until it has run a minute online; any reset before that boots the
  previous one, and 3 crash boots in a row before it proves itself roll back too.
- Task watchdogs (90 s) on both decks' long-running tasks; back-offs for a refused Spotify login,
  a missing light and an absent black deck.

### Repository
- Each deck's code in its own folder (`src/black`, `src/yellow`, `src/common`, `src/diag`), the
  pure modules in `src/util/<group>`; CI builds and budget-checks the yellow deck too.
- The Pokéface watch face moved to its own repository,
  [jvitorcosta/garmin-watchfaces](https://github.com/jvitorcosta/garmin-watchfaces).
- Docs: the README is organised by device; `docs/yellow-deck.md` (was `docs/leds-voice.md`) and
  `garmin/README.md` are new.

### Removed
- The CYD's standalone deck build (`cyd`, last in `d4d0a67`) with its DAC audio path, and the
  CYD's companion screen (`cyd_companion`: its lyrics and avatar home, retired on 2026-10-28, in
  git history before that date). The CYD is the yellow deck now.
- The parked SD card cache (last in `0ba409b`), the S3's unused FT6336 touch driver, the
  `PURPLE_TEST` / `CYAN_TEST` hooks and the unused ArduinoJson dependency.

## [1.1.0] - 2026-10-13

### Added
- `TIMEZONE` in `src/config.h` (POSIX TZ string, daylight saving included) sets when night
  mode starts, instead of a fixed UTC-4. **Add the line to your `config.h`**
  (`include/config.example.h` has examples); without it the build warns and uses UTC.

### Changed
- The project is now called CarDeck; the boot log line reads `[boot] cardeck v…`.
- Documentation reorganised: shorter README, new ARCHITECTURE, HISTORY, CONTRIBUTING and
  SECURITY docs, issue and pull request templates, and a list of third-party files in CREDITS.
- The ESP32 core logs errors only.
- CI runs with a read-only token and also builds release tags.

### Removed
- The on-device Spotify setup page, which Spotify could not redirect to. Without a refresh
  token the deck now shows how to get one (`tools/spotify_auth.py`).

### Security
- Spotify requests verify the server's certificate against 7 bundled root certificates.

## [1.0.0] - 2026-10-10

First tagged release: everything the deck does today, on both supported boards.

### Added
- Now playing from Spotify: track, artist, album art, progress, volume, shuffle and repeat.
- Synced lyrics from LRCLIB, with CJK text and accented Latin letters.
- A walking Pokémon picked per track as the progress bar, Gen-3 battle-box styling, day and
  night palettes.
- Genre badges from Apple's iTunes Search API.
- Boot scene: the night drive with the pixel car, a BIOS-style log in the sky, the greeting
  clip and a photo montage with an end card (montage and clips are local, git-ignored).
- Two boards: the ESP32-2432S028R "CYD" (DAC speaker) and the Freenove ESP32-S3 2.8"
  (ES8311 codec).
- Firmware version from the git tag on the boot screen and in the serial log.

[Unreleased]: https://github.com/jvitorcosta/car-cyberdeck/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/jvitorcosta/car-cyberdeck/compare/v1.1.0...v2.0.0
[1.1.0]: https://github.com/jvitorcosta/car-cyberdeck/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/jvitorcosta/car-cyberdeck/releases/tag/v1.0.0
