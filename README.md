<div align="center">

# CarDeck

**A wild track appeared!** A Spotify now-playing screen for the car, drawn as a Gen-3 Pokémon battle.

[![CI](https://github.com/jvitorcosta/spotify-car-deck/actions/workflows/ci.yml/badge.svg)](https://github.com/jvitorcosta/spotify-car-deck/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/tag/jvitorcosta/spotify-car-deck?label=release&sort=semver)](CHANGELOG.md)
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
  greeting, a pixel McQueen's drive with the second greeting, or a second video (Max
  Verstappen's F1 intro). All of them end in the montage
  and its logo card, while WiFi connects and Spotify logs in underneath.
- **Built for a car:** never freezes on a slow link, retries a late hotspot in the background,
  and recovers on its own.
- **Status screens that say why:** no WiFi, no internet or Spotify trouble, each with its own
  animated Pokémon; when Spotify asks the deck to slow down it shows how long to wait. On the
  S3, "Waiting for a song..." shows Sonic waiting, tapping his foot as in the 1991 game, on a city
  sidewalk at sunset (at night: stars, a crescent moon, more windows lit). A paused song keeps
  the deck for 30 s first.
- **Phone remote:** set `REMOTE_TOKEN` in `src/config.h` and open
  `http://cardeck.local/?k=<token>` (see [Two decks](#two-decks))
  on a phone on the same network: what's playing, the deck's status and a Howl button. Add it
  to the Home Screen for a full-screen app with its own icon. The "Waiting for a song..."
  screen shows the link as a QR code in a Gen-3 battle box named REMOTE, built from the deck's
  current address (`172.20.10.10` on the iPhone hotspot, the DHCP one elsewhere), and Howl plays a real wolf recording when you
  build one (otherwise a synthesised howl).
- **Yellow deck (the car's lights):** a Cheap Yellow Display (CYD) beside the S3 drives the car's
  two LED controllers over Bluetooth: colours, levels and presets on its touchscreen, by voice
  through a Siri Shortcut, from an Xbox controller or the watch, the S3's lyrics karaoke-style and
  its song colours (music mode). It reads the S3 over the hotspot (build target `ledpanel`; see
  [Car lights by voice](docs/leds-voice.md)).

<!-- Video: the boot scene. Add docs/images/boot-scene.gif, then uncomment:
<p align="center"><img src="docs/images/boot-scene.gif" alt="Boot scene" width="480"></p>
-->

## Hardware

| Board | Build target | Notes |
|---|---|---|
| Freenove FNK0104B, ESP32-S3 2.8" | `s3` | The deck. 8 MB PSRAM, 16 MB flash, ES8311 codec and speaker. |
| ESP32-2432S028R "Cheap Yellow Display" (CYD) | `ledpanel` | The yellow deck, optional: the car's LED lights. ESP32, no PSRAM. Reads the S3's `/state` for lyrics and music mode; needs the S3 with `REMOTE_TOKEN` set. |

Both are 320×240 ILI9341 boards. The CYD's standalone deck build (`cyd`) was retired on
2026-10-21 (git history up to `d4d0a67`), and its companion-screen build (`cyd_companion`, the
lyrics and avatar screen) on 2026-10-28 (git history before that date). You also need a USB car charger
(5 V, 2 A recommended) and a phone with a 2.4 GHz hotspot (on iPhone, turn on
**Maximize Compatibility**). The microSD slot isn't used.

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
4. **Flash:** `pio run -e s3 -t upload`. Add `--upload-port <port>` if needed;
   `pio device list` shows the ports. The S3 keeps its media (see Make it yours) in a flash
   partition of their own: flash them once with `pio run -e s3 -t uploadmedia`, and again only
   when a clip, the montage or the font changes. A code change then flashes ~1.3 MB.

   **An S3 flashed before the media partition** (one 12 MB app) needs both, once, from a PC
   with your media in `data/`: `pio run -e s3 -t upload` (the new partition table; settings
   in nvs survive) and then `pio run -e s3 -t uploadmedia`. Without the media the deck still
   boots, with the default chime, and logs `[media] none`.

   **Updates over WiFi (S3):** with `OTA_PASSWORD` set in `src/config.h` (its own random
   string, not `REMOTE_TOKEN`) and the PC on the deck's network (home
   WiFi on its 2.4 GHz band, see [Two decks](#two-decks); or the PC joined to the iPhone
   hotspot with `--upload-port 172.20.10.10`), run
   `pio run -e s3_ota -t upload`. The deck shows the upload's progress (on Green Hill Zone
   with `data/backdrop.bin`, see Media) and restarts into the new firmware; an interrupted upload never boots. The new firmware is on probation until it has run a
   minute with a good poll and its update listener up: if the deck resets before that (a crash,
   even before `setup()`, or the ignition) the bootloader boots the previous one
   (`[ota] ... holds an update the bootloader rolled back`); re-send it. After that, 3 crash boots
   in a row before it is marked good still go back (`[ota] rolled back`). Windows may ask to let Python through the firewall the first time:
   espota needs the deck to connect back to the PC. Media changes still go over USB
   (`uploadmedia`), and USB `upload` is always the way back.
5. **Play a song** on any Spotify device. The serial log is at 115200 baud:
   `pio device monitor`.

To check a new board without WiFi or Spotify, flash `hwcheck_s3` (S3) or `hwcheck` (CYD): it
tests the display, colours, backlight and audio.

**Yellow deck (optional):** set the same `REMOTE_TOKEN` on both decks and `BLACK_DECK_ADDR` to
the S3's address (`172.20.10.10`, its fixed address on the iPhone hotspot; a `config.h` that
still says `COMPANION_DECK` works too), then flash the CYD with `pio run -e ledpanel -t upload`
(after that, over WiFi with `ledpanel_ota`). Its voice and watch endpoints are in
[Car lights by voice](docs/leds-voice.md).

## Two decks

The black deck (S3) and the yellow deck (a CYD) are meant to share the
iPhone hotspot. There each takes a fixed address, so a Home Screen bookmark never breaks (unless
something already answers there); on any other network they use DHCP and mDNS names.

| Deck | Build target | On the iPhone hotspot | mDNS name (other networks) | Phone page |
|---|---|---|---|---|
| Black deck (S3) | `s3` | `172.20.10.10` | `cardeck.local` | `http://172.20.10.10/?k=<REMOTE_TOKEN>` |
| Yellow deck (CYD) | `ledpanel` | `172.20.10.11` | `cardeck-leds.local` | none: `/say`, `/state`, `/presets`, `/status` ([Car lights by voice](docs/leds-voice.md)) |

- **The hotspot goes first in `WIFI_NETWORKS`.** It gets two tries after a drop, and the fixed
  addresses are pinned only on an iPhone hotspot. A deck stays on the network it joined; off the
  hotspot the yellow deck finds the S3 by its mDNS name.
- **Home WiFi:** the ESP32 speaks 2.4 GHz only. If the router isolates clients, or puts 2.4 and
  5 GHz on separate networks, a phone or PC on the other band can't reach the decks' pages even
  while the two decks see each other (seen 2026-10-15: "Destination host unreachable" from a PC
  on 5 GHz). Use the hotspot, or join the phone or PC to the same 2.4 GHz network.

## Make it yours

All personal media is git-ignored. The S3
packs it (`tools/pack_media.py`) into its media partition with `pio run -e s3 -t uploadmedia`.
Without it the deck uses built-in defaults.

| What | How | Default |
|---|---|---|
| Boot sound | `python tools/gen_greeting.py my.wav` → `data/greeting.pcm` (up to 13 s) | a synthesized chime |
| Second boot sound (S3) | `gen_greeting.py --alt my.wav` → `data/greeting_alt.pcm`, McQueen's opening | McQueen plays the boot sound |
| Montage sounds | `gen_greeting.py --opener` / `--finale my.wav` | silent |
| Photo montage | photos in `montage-photos/`, then `python tools/gen_montage.py` (~5 s of photos; needs ffmpeg) | no montage |
| Pixel McQueen (S3) | a 200 × 66 side view facing right, then `python tools/gen_mcqueen_sprite.py montage-photos/mcqueen.png` → `data/mcqueen.bin` (the defaults fit the approved art; new art sets `--wheels`, `--head`, `--tail`, `--beam`) | two openings: the kiss and the Honda |
| Montage end card | `montage-photos/honda-logo.png` (any logo, white background works best) | none |
| Intro video (S3) | `python tools/gen_intro.py my.mp4` → `data/intro.bin` + `.pcm` (20 fps, 48 kHz sound; `--seg START-END` cuts, played in order; default: the kiss wallpaper's walk-in and kiss, ~13 s; needs ffmpeg). One of the random boot openings | none: the night drive openings only |
| Second intro video (S3) | `python tools/gen_intro.py max-clip/max.mp4 --seg 0-99 --out max --letterbox --q 8 --fps 16` → `data/max.bin` + `.pcm` (the whole 16:9 frame under the boot log; smaller frames at 16 fps, so the deck draws every one). Max Verstappen's F1 intro, another random opening | one video opening fewer |
| Update screen backdrop (S3) | `python tools/gen_backdrop.py` → `data/backdrop.bin` (Green Hill Zone, rebuilt from the s1disasm disassembly, fetched into `sonic-sprites/ghz/`) | the battle box on firmware updates |
| Shadow on the update rail (S3) | `python tools/gen_shadow_rider.py` → `data/shadow.bin` (his Sonic Battle running loop, cut from the sprite sheet it fetches into `sonic-sprites/shadow/`) | two jet glows ride the rail |
| Sonic waiting (S3) | `python tools/gen_idle_sprite.py` → `data/idle.bin` (rebuilt from the s1disasm disassembly, fetched into `sonic-sprites/`) | Pikachu on "Waiting for a song..." |
| Wolf howl (S3) | `python tools/gen_howl.py wolf.ogg` → `data/howl.pcm` (cut 13-19.5 s by default, `--start`/`--end`; needs ffmpeg), for example the public-domain ["Wolf howls.ogg"](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg) | a synthesised howl |
| Car sprite | edit and run `python tools/gen_car_sprite.py` (`--png out.png` previews it) | a silver sedan |

Convert other audio formats to WAV first, for example `ffmpeg -i in.mp3 in.wav`. After
changing your media, run `python tools/media_sizes.py record` and commit
`data/media_sizes.txt` (sizes only): CI builds stand-ins of those sizes, so its flash check
counts your real media on every deck (and in the S3's media pack).

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
pio run -e s3 -e ledpanel                                       # both decks
```

CI builds both decks and both hardware checks,
enforces the flash budget (`s3`, `ledpanel`), runs the host tests with ASan/UBSan and again as 32-bit, and runs
the media tool tests (`python -m unittest discover -s tools -p "test_gen_*.py"`). See
[CONTRIBUTING.md](CONTRIBUTING.md) for setup, test rules and commit style.

### Versioning

[Semantic versioning](https://semver.org), released as git tags `vMAJOR.MINOR.PATCH`; changes
are listed in [CHANGELOG.md](CHANGELOG.md). Each build reads its version from git
(`tools/version.py`) and shows it on the boot screen and in the first serial line,
`[boot] cardeck v1.0.0 (s3)`. Builds past a tag or with local edits say so: `1.0.0+3.gabc1234`.

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

Built on PokéAPI, PMD SpriteCollab, LRCLIB, GNU Unifont, Twemoji, the VT323 font and several
open-source libraries; see [CREDITS.md](CREDITS.md), which also lists the files in this repository
that are **not** covered by the MIT license.

CarDeck is a personal, non-commercial fan project. It is not affiliated with, endorsed by or
sponsored by Spotify, Nintendo, The Pokémon Company, Creatures Inc., GAME FREAK inc. or
Honda. Pokémon and Pokémon character names are trademarks of Nintendo. Spotify is a trademark
of Spotify AB.

Code: [MIT](LICENSE).
