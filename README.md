<div align="center">

# CarDeck

**A wild track appeared!** A Spotify now-playing screen for the car, drawn as a Gen-3 Pokémon battle.

[![CI](https://github.com/jvitorcosta/car-deck/actions/workflows/ci.yml/badge.svg)](https://github.com/jvitorcosta/car-deck/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/tag/jvitorcosta/car-deck?label=release&sort=semver)](CHANGELOG.md)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Boards: ESP32 / ESP32-S3](https://img.shields.io/badge/boards-ESP32%20%7C%20ESP32--S3-informational)

<!-- Photo: the deck on the dashboard. Add docs/images/hero.jpg, then uncomment:
<img src="docs/images/hero.jpg" alt="CarDeck on a car dashboard" width="640">
-->

</div>

CarDeck is firmware for a cheap 2.8" ESP32 touchscreen. It shows what is playing on your
Spotify account as a FireRed/Emerald battle screen, and runs off your phone's hotspot.

## Features

- **Now playing:** title, artist, album art, progress, playing device, shuffle and repeat.
- **A Pokémon per song:** a random one of 1 025 walks along the HP bar as the song plays,
  using the animated walk cycles from the PMD sprite archive.
- **Synced lyrics** in the battle dialogue box, from LRCLIB, including accented Latin and
  Japanese/Chinese text.
- **Genre badge** styled like a Pokémon type, from Apple's iTunes catalogue.
- **Night palette** with a dimmer backlight from 18:00 to 06:00 in your time zone.
- **Boot scene:** a pixel-art night drive with a BIOS-style log and a boot sound, optionally
  followed by a montage of your own photos. On the S3 each boot picks one opening at random,
  never last boot's: your intro video with the log typed over it, the Honda's drive with its
  greeting, or a pixel McQueen's drive with the second greeting. All of them end in the montage
  and its logo card, while WiFi connects and Spotify logs in underneath.
- **Built for a car:** never freezes on a slow link, retries a late hotspot in the background,
  and recovers on its own.
- **Status screens that say why:** no WiFi, no internet or Spotify trouble, each with its own
  animated Pokémon; when Spotify asks the deck to slow down it shows how long to wait. On the
  S3, "Spotify is silent" shows Sonic waiting, tapping his foot as in the 1991 game, on a city
  sidewalk at sunset (at night: stars, a crescent moon, more windows lit), and a music box
  plays softly until the song comes back. A paused song keeps the deck for 30 s first.
- **Phone remote:** set `REMOTE_TOKEN` in `src/config.h` and open
  `http://cardeck.local/?k=<token>` (`cardeck-cyd.local` on the CYD; see [Two decks](#two-decks))
  on a phone on the same network: what's playing, the deck's status and a Howl button. Add it
  to the Home Screen for a full-screen app with its own icon. On the S3 the "Spotify is silent"
  screen shows the link as a QR code, built from the deck's current address (`172.20.10.10` on
  the iPhone hotspot, the DHCP one elsewhere), and Howl plays a real wolf recording when you
  build one (otherwise, and always on the CYD, a synthesised howl).
- **Companion screen:** a second (CYD) deck beside the S3 shows the current lyric line big,
  with the next one dimmed underneath. It reads the S3 over the hotspot and has no Spotify
  client of its own (build target `cyd_companion`).
- **Avatar (companion):** a character of your choice becomes the companion's home screen, drawn
  2x and centred on her own. She sings along with synced lyrics, greets you, remarks on new
  songs and reacts when you play a sound or reset the trip. The Lyrics, Sounds and Trip screens,
  Howl and trip reset (Vroom once the new deck offers it) are on the companion's own phone page.
  Her portraits come from your own images (`tools/gen_avatar.py`).

<!-- Video: the boot scene. Add docs/images/boot-scene.gif, then uncomment:
<p align="center"><img src="docs/images/boot-scene.gif" alt="Boot scene" width="480"></p>
-->

## Hardware

| Board | Build target | Notes |
|---|---|---|
| ESP32-2432S028R "Cheap Yellow Display" (CYD) | `cyd` | ESP32, no PSRAM. Optional speaker on the 2-pin SPEAK connector. |
| Freenove FNK0104B, ESP32-S3 2.8" | `s3` | 8 MB PSRAM, 16 MB flash, ES8311 codec and speaker. |
| A second CYD, as a companion screen | `cyd_companion` | Optional. Reads the S3's `/state`; needs the S3 with `REMOTE_TOKEN` set. |

Both are 320×240 ILI9341 boards and run the same code. You also need a USB car charger
(5 V, 2 A recommended) and a phone with a 2.4 GHz hotspot (on iPhone, turn on
**Maximize Compatibility**). The microSD slot isn't used: the SD cache is parked because it
competes with TLS for RAM.

## Quick start

You need [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(or the VS Code extension), Python 3, and a Spotify account.

1. **Create a Spotify app** in the [developer dashboard](https://developer.spotify.com/dashboard)
   with the redirect URI `http://127.0.0.1:8888/callback`.
2. **Configure:** copy `include/config.example.h` to `src/config.h` (git-ignored) and fill in
   your WiFi networks, the app's client ID and secret, and your time zone. Networks are tried
   in order: put the phone hotspot **first** (see [Two decks](#two-decks)).
3. **Authorize:** run `python tools/spotify_auth.py`. It opens the browser and writes a refresh
   token into `src/config.h`.
4. **Flash:** `pio run -e cyd -t upload` (or `-e s3`). Add `--upload-port <port>` if needed;
   `pio device list` shows the ports.
5. **Play a song** on any Spotify device. The serial log is at 115200 baud:
   `pio device monitor`.

To check a new board without WiFi or Spotify, flash `hwcheck` (CYD) or `hwcheck_s3` (S3): it
tests the display, colours, backlight and audio.

**Companion screen (optional):** set the same `REMOTE_TOKEN` on both decks and
`COMPANION_DECK` to the S3's address (`172.20.10.10`, its fixed address on the iPhone hotspot),
then flash the second CYD with `pio run -e cyd_companion -t upload`. It shows
**Looking for the deck...** until it reaches the S3, and **Wrong key** if the tokens differ.
With an avatar file (see Make it yours) the companion serves its own menu remote at
`http://172.20.10.11/?k=<REMOTE_TOKEN>` (its fixed hotspot address).

## Two decks

The new deck (S3) and the old one (a CYD, as the companion or on its own) are meant to share the
iPhone hotspot. There each takes a fixed address, so a Home Screen bookmark never breaks (unless
something already answers there); on any other network they use DHCP and mDNS names.

| Deck | Build target | On the iPhone hotspot | mDNS name (other networks) | Phone page |
|---|---|---|---|---|
| New deck (S3) | `s3` | `172.20.10.10` | `cardeck.local` | `http://172.20.10.10/?k=<REMOTE_TOKEN>` |
| Old deck as the companion | `cyd_companion` | `172.20.10.11` | `cardeck-companion.local` | `http://172.20.10.11/?k=<REMOTE_TOKEN>` (with an avatar) |
| Old deck on its own | `cyd` | `172.20.10.11` | `cardeck-cyd.local` | `http://172.20.10.11/?k=<REMOTE_TOKEN>` |

- **The hotspot goes first in `WIFI_NETWORKS`.** It gets two tries after a drop, and the fixed
  addresses are pinned only on an iPhone hotspot. A deck stays on the network it joined; off the
  hotspot the companion finds the S3 by its mDNS name.
- **Home WiFi:** the ESP32 speaks 2.4 GHz only. If the router isolates clients, or puts 2.4 and
  5 GHz on separate networks, a phone or PC on the other band can't reach the decks' pages even
  while the two decks see each other (seen 2026-10-15: "Destination host unreachable" from a PC
  on 5 GHz). Use the hotspot, or join the phone or PC to the same 2.4 GHz network.

## Make it yours

All personal media is git-ignored and embedded at build time. Without it the deck uses
built-in defaults.

| What | How | Default |
|---|---|---|
| Boot sound | `python tools/gen_greeting.py my.wav` → `data/greeting.pcm` (up to 13 s) | a synthesized chime |
| Second boot sound (S3) | `gen_greeting.py --alt my.wav` → `data/greeting_alt.pcm`, McQueen's opening | McQueen plays the boot sound |
| Montage sounds | `gen_greeting.py --opener` / `--finale my.wav` | silent |
| Photo montage | photos in `montage-photos/`, then `python tools/gen_montage.py` (~5 s of photos; needs ffmpeg) | no montage |
| Pixel McQueen (S3) | a 200 × 66 side view facing right, then `python tools/gen_mcqueen_sprite.py montage-photos/mcqueen.png` → `data/mcqueen.bin` (the defaults fit the approved art; new art sets `--wheels`, `--head`, `--tail`, `--beam`) | two openings: the kiss and the Honda |
| Montage end card | `montage-photos/honda-logo.png` (any logo, white background works best) | none |
| Intro video (S3) | `python tools/gen_intro.py my.mp4` → `data/intro.bin` + `.pcm` (20 fps, 48 kHz sound; `--seg START-END` cuts, played in order; default: the kiss wallpaper's walk-in and kiss, ~13 s; needs ffmpeg). One of the random boot openings | none: the night drive openings only |
| Sonic waiting (S3) | `python tools/gen_idle_sprite.py` → `data/idle.bin` (rebuilt from the s1disasm disassembly, fetched into `sonic-sprites/`) | Pikachu on "Spotify is silent" |
| Music-box tunes (S3) | `python tools/gen_tune.py song.mid --name "National Park"` → `data/tunes/national_park.txt`, one per song (the melody line; `--track N` if it picks the accompaniment); all of `data/tunes/` loop in random order | two public-domain tunes (`data/tunes_default.txt`) |
| Wolf howl (S3) | `python tools/gen_howl.py wolf.ogg` → `data/howl.pcm` (cut 13-19.5 s by default, `--start`/`--end`; needs ffmpeg), for example the public-domain ["Wolf howls.ogg"](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg) | a synthesised howl |
| Avatar (companion) | images + `avatar-photos/avatar.json`, then `python tools/gen_avatar.py` (needs ffmpeg; see the script's header) | lyrics only |
| Car sprite | edit and run `python tools/gen_car_sprite.py` (`--png out.png` previews it) | a silver sedan |

Convert other audio formats to WAV first, for example `ffmpeg -i in.mp3 in.wav`. After
changing your media, run `python tools/media_sizes.py record` and commit
`data/media_sizes.txt` (sizes only): CI builds stand-ins of those sizes, so its flash check
counts your real media on every deck.

## How it works

The ESP32's two cores split the work: one task polls Spotify and downloads art, lyrics,
genres and sprites; the other only draws. Results are tagged with the song they belong to,
so a skipped song's art never lands on the next one. Everything is sized to fit in about
110 KB of RAM next to a TLS session.

- [Architecture](docs/ARCHITECTURE.md): tasks, modules and layers.
- [Design and performance history](docs/HISTORY.md): what was measured on the device and how
  each freeze, leak and crash was fixed.
- [Backlog](docs/BACKLOG.md): open items and ideas.

## Development

```sh
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1   # host unit tests (Unity, g++)
pio run -e cyd -e s3                                            # both firmwares
```

CI builds both boards, the companion and both hardware checks, enforces the flash budget
(`cyd`, `cyd_companion`, `s3`), runs the host tests with ASan/UBSan and again as 32-bit, and runs
the media tool tests (`python -m unittest discover -s tools -p "test_gen_*.py"`). See
[CONTRIBUTING.md](CONTRIBUTING.md) for setup, test rules and commit style.

### Versioning

[Semantic versioning](https://semver.org), released as git tags `vMAJOR.MINOR.PATCH`; changes
are listed in [CHANGELOG.md](CHANGELOG.md). Each build reads its version from git
(`tools/version.py`) and shows it on the boot screen and in the first serial line,
`[boot] cardeck v1.0.0 (cyd)`. Builds past a tag or with local edits say so: `1.0.0+3.gabc1234`.

- **major:** an update needs action, such as new wiring, a new partition table, or a changed
  `config.h` format.
- **minor:** a `feat` commit: something new on the screen or the speaker.
- **patch:** `fix`, `perf` and any other change to the firmware's behaviour.

To release: add the version's section to `CHANGELOG.md`, commit, then
`git tag -a v1.1.0 -m "v1.1.0"` and `git push origin main v1.1.0`.

## Security

Credentials live only in the git-ignored `src/config.h`. See [SECURITY.md](SECURITY.md) for
known limitations and how to report a problem.

## Credits and legal

Built on PokéAPI, PMD SpriteCollab, LRCLIB, GNU Unifont and several open-source libraries;
see [CREDITS.md](CREDITS.md), which also lists the files in this repository that are **not**
covered by the MIT license.

CarDeck is a personal, non-commercial fan project. It is not affiliated with, endorsed by or
sponsored by Spotify, Nintendo, The Pokémon Company, Creatures Inc., GAME FREAK inc. or
Honda. Pokémon and Pokémon character names are trademarks of Nintendo. Spotify is a trademark
of Spotify AB.

Code: [MIT](LICENSE).
