# Changelog

All notable changes to the deck. The format follows [Keep a Changelog](https://keepachangelog.com),
and versions follow [semantic versioning](https://semver.org) (README "Versioning").

## [Unreleased]

### Changed
- The project is now called CarDeck; the boot log line reads `[boot] cardeck v…`.
- Documentation reorganised: shorter README, new ARCHITECTURE, HISTORY, CONTRIBUTING and
  SECURITY docs, issue and pull request templates, and a list of third-party files in CREDITS.
- CI runs with a read-only token and also builds release tags.
- Night mode follows `TIMEZONE` in `src/config.h` (POSIX TZ string, daylight saving included)
  instead of a fixed UTC-4. **Add the line to your `config.h`**; without it the deck uses UTC.
- Without a refresh token the deck shows how to get one, instead of a setup page that Spotify
  could not redirect to.

### Security
- Spotify requests verify the server's certificate against bundled roots.

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

[Unreleased]: https://github.com/jvitorcosta/car-deck/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/jvitorcosta/car-deck/releases/tag/v1.0.0
