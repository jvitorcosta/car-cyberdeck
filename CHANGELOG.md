# Changelog

All notable changes to the deck. The format follows [Keep a Changelog](https://keepachangelog.com),
and versions follow [semantic versioning](https://semver.org) (README "Versioning").

## [Unreleased]

### Added
- A random boot opening on the S3, never the same as last boot: your intro video with the boot
  log typed over it, the Honda's night drive with "bem vindo moreno", or a pixel Lightning
  McQueen's drive with "eu sou a velocidade" (`tools/gen_mcqueen_sprite.py`, git-ignored; red
  rims). All of them end in the photo montage and the POWERED BY HONDA card.
  `-D INTRO_FORCE=\"kiss\"` (or `honda`, `mcqueen`) forces one for testing.
- A music box on the S3's "Nothing playing" screen: soft plucked tunes (5 % of the clips'
  volume) loop in random order until the song comes back. Make your own from MIDI with
  `tools/gen_tune.py` (`data/tunes/`, git-ignored); without them it plays Brahms' Lullaby and Greensleeves.
- Avatar on the companion (old deck): your character is the home screen, drawn 2x and centred.
  She sings along with synced lyrics, greets you, remarks on new songs (at most every 5 min) and
  reacts to Howl and trip reset. The Lyrics, Sounds and Trip screens open from the companion's
  own phone page (`http://172.20.10.11/?k=<REMOTE_TOKEN>` on the hotspot). Portraits come
  from your own images via `tools/gen_avatar.py` (`avatar-photos/`, git-ignored); without one
  the companion shows the lyrics as before. The companion now sends Howl and trip reset to the
  new deck.
- Boot intro video on the S3: your own clip (`tools/gen_intro.py`, git-ignored), one of the
  random boot openings, with its sound, 20 fps / 48 kHz, while WiFi connects and Spotify logs in
  underneath; then the montage. `--seg START-END` cuts play in the order given.
- A second boot greeting on the S3 (`tools/gen_greeting.py --alt`): McQueen's opening plays it.
- The S3's Howl plays a real wolf recording when one is built (`tools/gen_howl.py`, e.g. the
  public-domain "Wolf howls.ogg" from Wikimedia Commons); otherwise the synthesised howl.
- Phone remote (proof of concept): `http://cardeck.local/?k=<REMOTE_TOKEN>` (`cardeck-cyd.local`
  on the CYD, or the deck's IP) shows a Howl button
  that plays a synthesised wolf howl on the deck while it keeps playing. Off unless
  `REMOTE_TOKEN` is set in `config.h`.
- The phone page opens full-screen from the iPhone Home Screen with its own icon, and shows
  what's playing and the deck's status. On the iPhone hotspot the decks take fixed addresses,
  `172.20.10.10` (S3) and `172.20.10.11` (CYD), unless something already answers there, so a
  bookmark keeps working; DHCP on any other network.
- Companion screen (`cyd_companion`): a second CYD shows the S3's current lyric line at double
  size with the next line dimmed, and moves to the next line on its own clock between polls.
  New `config.h` option `COMPANION_DECK` (the S3's address).
- `GET /state` on the S3 also returns the song timing, the current and next lyric line and a
  trip clock; `POST /trip/reset` restarts the trip clock.

### Changed
- The S3 keeps its big buffers in PSRAM (album art, both walker slots, the walk-sheet download
  buffer, the PNG decoder): about 50 KB more internal RAM for TLS and WiFi all the time, and 45 KB
  more during a walker decode. The download buffer grew from 12 KB to 64 KB, so large walk
  sheets no longer fall back to the still sprite, and walkers keep all their frames.
- The deck asks Spotify once per poll (`/v1/me/player`: song, progress and device together)
  instead of two endpoints, on a connection kept open between polls instead of a new TLS
  handshake each time. A 429's wait comes with the answer (no extra request), a revoked access
  token is refreshed and the request retried at once, and the podcast and ad cases no longer
  read uninitialised fields. The SpotifyArduino library and `tools/patch_spotify_lib.py` are
  gone (own client in `spotify/client`, `util/playerjson`, `util/jsonpath`).
- The "Nothing playing" screen now says **Waiting for a song...** (same screen, same timing).
- The S3's "Waiting for a song..." screen shows the phone page's link as a QR code again, left of
  Sonic in a Gen-3 battle box named **REMOTE** (with a phone icon): 3 px modules (half as big
  again as before) on the battle boxes' cream, so phones read it from further away.
  `http://<deck address>/?k=<REMOTE_TOKEN>` with the address it has now (`172.20.10.10` on the
  iPhone hotspot, the DHCP one on any other network), redrawn when the address changes. None
  while offline or without `REMOTE_TOKEN`: the sunset shows instead. Sonic's frames now repaint
  only his own box, not the whole scene.
- A shorter S3 boot: about 19-21 s from power-on to the deck (measured: McQueen 19.2 s, Honda
  19.3 s, kiss 21.1 s; it was 27-35 s), most of it waiting for WiFi. The car openings start
  driving at once instead of after WiFi connects; the kiss is cut to the walk-in and the kiss
  (~13 s, was 23 s); the photo montage is ~5 s (was ~7.5 s) and the end card ~2.5 s.
- Without WiFi, the kiss holds its last frame with `WI-FI ... RETRYING...` until the hotspot is
  back; the drives keep cruising as before.
- A paused song keeps the deck for 30 s before "Nothing playing" shows (it used to show only
  when Spotify had nothing at all).
- On the S3, "Nothing playing" shows Sonic waiting (Sonic 1's foot tap, with the game's timing)
  instead of Pikachu when you build his sprite (`tools/gen_idle_sprite.py`, git-ignored). He
  waits on a city sidewalk at sunset: banded sky, striped low sun, skyline with lit windows,
  street lamps and the road; at night a navy sky with stars and a crescent moon, and more
  windows lit.
- The S3's app partition is 12 MB (was 8 MB; taken from the unused storage area of its 16 MB
  flash), and the boot intro plays its whole clip in story order (`gen_intro.py`'s default cut:
  11.8 s to the end, then 0-11.8 s). Flash the S3 once with `pio run -e s3 -t upload` to write the
  new partition table.
- "No signal..." now says why: **No WiFi...** (lists the saved networks), **No internet...**
  (connected, but the network or hotspot passes no traffic) or **Spotify error** (HTTP code or
  rejected certificate).
- The status screens are animated, with a Pokemon per cause: Zubat searching for WiFi (with a
  rolling Poke Ball), Psyduck when there's no internet, Slowpoke while Spotify rate-limits
  (**Spotify busy...** counts down to the next try), Porygon for a rejected certificate,
  Pikachu for "Nothing playing". All bundled in flash: they show when nothing can download.
- The CYD no longer probes the microSD slot at boot (~0.6 s faster, no `Card Failed` errors);
  the parked SD cache is built only with `-DSD_CACHE`.
- When Spotify answers 429 (too many requests), the deck waits as long as Spotify's
  `Retry-After` asks (falling back to 8, 16, 32, then 60 s), skips the player-details call,
  and shows **Spotify busy...** with the time left right away.
- Fewer Spotify requests while nothing is playing: a "nothing playing" answer asks the player
  only when there is a song on screen to keep.
- The config template (`include/config.example.h`) lists the hotspot first.
- CI also builds the companion, checks its flash budget, counts the S3's real intro, howl and
  greeting sizes in its flash check, and runs the media tool tests.

### Fixed
- The boot scene's task had 1.8 KB of stack left at its end; it now has 6 KB instead of 4 KB.
- Right after boot, a song playing on the phone no longer shows as **Waiting for a song...**
  when Spotify's currently-playing endpoint wrongly answered "nothing playing": the deck now
  reads `/me/player`, which knows.
- The S3 froze its network task ("No internet", stale songs) while plugged into a PC with no
  serial monitor open: a USB serial write with a 0 ms timeout waits forever in this core. Writes
  now give up after 1 ms.
- A "nothing playing" answer from Spotify while the phone still plays now checks the player
  before showing **Nothing playing**.
- The S3 logs into Spotify and fetches the first song while its intro plays, instead of a black
  "Spotify auth..." screen after it.
- Both decks join the hotspot first (two tries after a drop from it), else the home WiFi, and
  stay there. The companion finds the new deck by name (`cardeck`, mDNS) off the hotspot, and
  after 30 s without it moves to the other network only if that one is in range (in the car it
  never leaves the hotspot).
- `GET /state` no longer fails (HTTP 500, the companion slowing to 5 s polls) with a long title
  and long lyric lines: its buffer is now 2 KB.
- The S3 no longer hangs at boot when the intro's task can't start: it skips the intro.
- After a hotspot reconnect the WiFi driver could report "connected" with no address, and the
  deck failed every Spotify lookup for ~40 s. Online now means connected with an address, so it
  reconnects within ~5-10 s.

### Removed
- The CYD's standalone deck build (`cyd`): the S3 is the deck and the CYD stays as its companion
  screen (`cyd_companion`) and keeps its hardware check (`hwcheck`). Gone with it: the CYD's DAC
  audio path (`board/cyd/sink_dac`, `util/dacstream`) and its branches in `main.cpp` and
  `audio/greeting`. The last commit with it is `d4d0a67`.

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

[Unreleased]: https://github.com/jvitorcosta/car-deck/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/jvitorcosta/car-deck/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/jvitorcosta/car-deck/releases/tag/v1.0.0
