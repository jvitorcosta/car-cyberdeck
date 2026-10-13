# Changelog

All notable changes to the deck. The format follows [Keep a Changelog](https://keepachangelog.com),
and versions follow [semantic versioning](https://semver.org) (README "Versioning").

## [Unreleased]

### Added
- Phone remote (proof of concept): `http://cardeck.local/?k=<REMOTE_TOKEN>` (`cardeck-cyd.local`
  on the CYD, or the deck's IP) shows a Howl button
  that plays a synthesised wolf howl on the deck while it keeps playing. Off unless
  `REMOTE_TOKEN` is set in `config.h`.

### Changed
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
