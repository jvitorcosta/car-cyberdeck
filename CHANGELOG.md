# Changelog

All notable changes to CarDeck: the black deck, the yellow deck and the watch app. The format follows [Keep a Changelog](https://keepachangelog.com),
and versions follow [semantic versioning](https://semver.org) (README "Versioning").

## [Unreleased]

### Changed
- Yellow deck: the white theme is a softer grey-white.
- Music mode: the outside light runs at 100 % at any hour.
- Yellow deck: presets fade in instead of jumping, and the lights rise to the last look when the
  deck starts.

### Removed
- Yellow deck: night mode. The screen keeps its colours and full backlight at any hour. The black
  deck keeps its own.

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
