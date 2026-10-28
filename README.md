<div align="center">

# CarDeck

**A wild track appeared!** Two ESP32 screens and a Garmin watch app for the car: Spotify as a
Gen-3 Pokémon battle, and the car's LED lights by touch, voice, gamepad or watch.

[![CI](https://github.com/jvitorcosta/spotify-car-deck/actions/workflows/ci.yml/badge.svg)](https://github.com/jvitorcosta/spotify-car-deck/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/tag/jvitorcosta/spotify-car-deck?label=release&sort=semver)](CHANGELOG.md)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Boards: ESP32 / ESP32-S3](https://img.shields.io/badge/boards-ESP32%20%7C%20ESP32--S3-informational)

<!-- Photo: the decks on the dashboard. Add docs/images/hero.jpg, then uncomment:
<img src="docs/images/hero.jpg" alt="CarDeck on a car dashboard" width="640">
-->

</div>

CarDeck is firmware for two cheap 2.8" ESP32 touchscreens and a Connect IQ app for a Garmin
watch. All three meet on the phone's WiFi hotspot; nothing needs a server.

| Device | Board | Build | What it does | Manual |
|---|---|---|---|---|
| **Black deck** | Freenove FNK0104B, ESP32-S3 2.8" | `s3` | Spotify now playing as a FireRed/Emerald battle: album art, a walking Pokémon, synced lyrics, genre badge; picks each song's colour for the lights | this page, [Architecture](docs/ARCHITECTURE.md) |
| **Yellow deck** | ESP32-2432S028R "Cheap Yellow Display" (CYD) | `ledpanel` | Drives the car's two LED controllers over Bluetooth: a terminal-style dashboard (clock, weather, trip), colours and presets, the black deck's lyrics, music mode; Siri, Xbox pad and watch control | [docs/yellow-deck.md](docs/yellow-deck.md) |
| **Honda City** watch app | Garmin Forerunner 255 | `garmin/carlights` | The lights from the wrist: each zone's colour and level, the presets, music mode, a glance card | [garmin/README.md](garmin/README.md) |

The yellow deck is optional: the black deck works alone. The watch app needs the yellow deck.
The Pokéface watch face that used to live here moved to
[jvitorcosta/garmin-watchfaces](https://github.com/jvitorcosta/garmin-watchfaces).

## Black deck

- **Now playing:** title, artist, album art, progress, playing device, shuffle and repeat.
- **A Pokémon per song:** a random one of 1 025 walks along the HP bar as the song plays,
  using the animated walk cycles from the PMD sprite archive.
- **Synced lyrics** in the battle dialogue box, from LRCLIB, including accented Latin and
  Japanese/Chinese text.
- **Genre badge** styled like a Pokémon type, from Apple's iTunes catalogue.
- **Night palette** with a dimmer backlight from 18:00 to 06:00 in your time zone.
- **Boot scene:** a pixel-art night drive with a BIOS-style log and a boot sound, then a
  montage of your own photos. Each boot picks one opening at random, never last boot's: your
  intro video with the log typed over it, the car's night drive, a pixel McQueen's drive, or a
  second video. All of them end in the montage while WiFi connects and Spotify logs in underneath.
- **Built for a car:** never freezes on a slow link, retries a late hotspot in the background,
  and recovers on its own.
- **Status screens that say why:** no WiFi, no internet or Spotify trouble, each with its own
  animated Pokémon; when Spotify asks the deck to slow down it shows how long to wait.
  "Waiting for a song..." shows Sonic tapping his foot on a city sidewalk at sunset. A paused
  song keeps the deck for 30 s first.
- **Phone remote:** with `REMOTE_TOKEN` set, `http://cardeck.local/?k=<token>` on a phone on the
  same network shows what's playing, the deck's status and a Howl button. Add it to the Home
  Screen for a full-screen app. "Waiting for a song..." shows the link as a QR code.
- **Song colours for the lights:** while the yellow deck's music mode is on, the black deck picks
  each song's colour (your picks, colour words, playlist tags, mood and cover) and sends it
  signed over UDP.

<!-- Video: the boot scene. Add docs/images/boot-scene.gif, then uncomment:
<p align="center"><img src="docs/images/boot-scene.gif" alt="Boot scene" width="480"></p>
-->

## Hardware

| Board | Build | Notes |
|---|---|---|
| Freenove FNK0104B, ESP32-S3 2.8" | `s3` | The black deck. 8 MB PSRAM, 16 MB flash, ES8311 codec and speaker. |
| ESP32-2432S028R "Cheap Yellow Display" (CYD) | `ledpanel` | The yellow deck, optional. ESP32, no PSRAM, resistive touch. |
| Two ELK-family BLE LED controllers (ELK-BLEDDM, MELK-…) | — | The car's inside and outside lights, driven by the yellow deck. |
| Garmin Forerunner 255 (also 255M, 255S, 255S Music) | — | The watch app, optional. |

Both decks are 320×240 ILI9341 boards. You also need a USB car charger (5 V, 2 A recommended)
and a phone with a 2.4 GHz hotspot (on iPhone, turn on **Maximize Compatibility**). The microSD
slots aren't used.

## Quick start

You need [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(or the VS Code extension) and Python 3. Every secret goes in `src/config.h`, which is
git-ignored: copy `include/config.example.h` there first. Both decks build from the same file.

### Black deck

1. **Create a Spotify app** in the [developer dashboard](https://developer.spotify.com/dashboard)
   with the redirect URI `http://127.0.0.1:8888/callback`.
2. **Configure** `src/config.h`: your WiFi networks (the phone hotspot **first**, see
   [On the hotspot](#on-the-hotspot)), the app's client ID and secret, and your time zone.
   For the phone remote and the yellow deck, set `REMOTE_TOKEN` to a long random string.
3. **Authorize:** `python tools/spotify_auth.py`. It opens the browser and writes a refresh
   token into `src/config.h`.
4. **Flash** over USB: `pio run -e s3 -t upload` (add `--upload-port <port>` if needed;
   `pio device list` shows the ports). The S3 keeps its media (see [Make it yours](#make-it-yours))
   in a flash partition of their own: flash them once with `pio run -e s3 -t uploadmedia`, and
   again only when a clip, the montage or the font changes. Without media the deck still boots,
   with the default chime, and logs `[media] none`.
5. **Play a song** on any Spotify device. The serial log is at 115200 baud:
   `pio device monitor`.

**Updates over WiFi:** set `OTA_PASSWORD` (its own random string, not `REMOTE_TOKEN`), put the
PC on the deck's network (the iPhone hotspot with `--upload-port 172.20.10.10`, or the same
2.4 GHz home network) and run `pio run -e s3_ota -t upload`. The deck shows the progress and
restarts into the new firmware; an interrupted upload never boots. **Keep the deck powered for
about a minute afterwards:** the new firmware stays pending (`[ota] update pending`) until it
has run a minute online (`[ota] update confirmed`); a reset before that boots the previous one.
After that, 3 crash boots in a row before a good Spotify poll still roll back. Windows may ask
to let Python through the firewall the first time. Media changes still go over USB, and USB
`upload` is always the way back.

To check a new board without WiFi or Spotify, flash `hwcheck_s3`: it tests the display,
colours, backlight and audio.

### Yellow deck

1. **Find the LED controllers' addresses.** Turn your phone's Bluetooth off (a controller holds
   one link at a time), then `pio run -e blescan -t upload -t monitor`: it lists every
   advertiser and dumps each named one's services; the controllers show as `ELK-BLEDDM` /
   `MELK-…` with a service `FFF0`. The controllers' own phone app also shows the addresses. Which
   one is inside is easiest to tell by switching one off.
2. **Configure** the same `src/config.h`: `LED_INSIDE_MAC` and `LED_OUTSIDE_MAC` (from step 1),
   `WIFI_NETWORKS` (hotspot first), `TIMEZONE`, `REMOTE_TOKEN` (the black deck's; the yellow deck
   joins WiFi only with it), `OTA_PASSWORD`, and `BLACK_DECK_ADDR` (`172.20.10.10`, the black
   deck's fixed address on the iPhone hotspot; a `config.h` that still says `COMPANION_DECK`
   works too).
3. **Flash once over USB:** `pio run -e ledpanel -t upload`. This also writes the two-slot
   partition table; after that, updates go over WiFi from a PC on the hotspot:
   `pio run -e ledpanel_ota -t upload` (keep the deck powered for a minute after, as above).
   To run `blescan` or `bletest` again later, erase the flash first (`pio run -e blescan -t
   erase`): they use NimBLE 1.4 and boot-loop on the bonds the deck's NimBLE 2 leaves.
4. **Calibrate the touch:** on the first boot the deck asks you to tap five crosses with the
   pen. To redo it later, hold **BOOT** at power-up or for 2 s at any time.
5. **Pair an Xbox controller** (optional; Series X|S or One model 1708 and later): hold its pair
   button until the logo flashes fast. The deck takes the first one it finds in pairing mode,
   then reconnects to it whenever it is switched on.
6. **Siri:** make a Shortcut that sends what you say to
   `http://172.20.10.11/say?k=<REMOTE_TOKEN>&text=<words>` and speaks the reply; the steps are in
   [docs/yellow-deck.md](docs/yellow-deck.md#siri-shortcuts).

To check a new CYD without the lights, flash `hwcheck`: display, colours, orientation, touch.

### Watch app

Install the [Connect IQ SDK](https://developer.garmin.com/connect-iq/sdk/) and make a developer
key, copy `garmin/carlights/source/Secrets.mc.example` to `Secrets.mc` (git-ignored) with your
`REMOTE_TOKEN`, then build and copy the `.prg` to the watch. The commands are in
[garmin/README.md](garmin/README.md).

## On the hotspot

The two decks are meant to share the iPhone hotspot. There each takes a fixed address, so a
Shortcut, the watch and a Home Screen bookmark never break (unless something already answers
there); on any other network they use DHCP and mDNS names.

| Deck | Build | On the iPhone hotspot | mDNS name (other networks) | Endpoints |
|---|---|---|---|---|
| Black deck | `s3` | `172.20.10.10` | `cardeck.local` | phone page `/?k=<REMOTE_TOKEN>`, `/state`, `/music` |
| Yellow deck | `ledpanel` | `172.20.10.11` | `cardeck-leds.local` | `/say`, `/state`, `/presets`, `/status` ([yellow-deck.md](docs/yellow-deck.md#voice-and-http)) |

- **The hotspot goes first in `WIFI_NETWORKS`.** It gets two tries after a drop, and the fixed
  addresses are pinned only on an iPhone hotspot. A deck stays on the network it joined; off the
  hotspot the yellow deck finds the black deck by its mDNS name.
- **Both decks need the same `REMOTE_TOKEN`**, and so does the watch's `Secrets.mc`. Devices sign
  their requests with it and never send it (see [SECURITY.md](SECURITY.md)), so update all of
  them together.
- **Home WiFi:** the ESP32 speaks 2.4 GHz only. If the router isolates clients, or puts 2.4 and
  5 GHz on separate networks, a phone or PC on the other band can't reach the decks even while
  the two decks see each other. Use the hotspot, or join the phone or PC to the same 2.4 GHz
  network.

## Make it yours

All personal media is git-ignored. The black deck packs it (`tools/pack_media.py`) into its
media partition with `pio run -e s3 -t uploadmedia`. Without it the deck uses built-in defaults.

| What | How | Default |
|---|---|---|
| Boot sound | `python tools/gen_greeting.py my.wav` → `data/greeting.pcm` (up to 13 s) | a synthesized chime |
| Second boot sound | `gen_greeting.py --alt my.wav` → `data/greeting_alt.pcm`, McQueen's opening | McQueen plays the boot sound |
| Montage sounds | `gen_greeting.py --opener` / `--finale my.wav` | silent |
| Photo montage | photos in `montage-photos/`, then `python tools/gen_montage.py` (~5 s of photos; needs ffmpeg) | no montage |
| Montage end card | `montage-photos/honda-logo.png` (any logo, white background works best) | none |
| Pixel McQueen | a 200 × 66 side view facing right, then `python tools/gen_mcqueen_sprite.py montage-photos/mcqueen.png` → `data/mcqueen.bin` (new art sets `--wheels`, `--head`, `--tail`, `--beam`) | his opening is left out |
| Intro video | `python tools/gen_intro.py my.mp4` → `data/intro.bin` + `.pcm` (20 fps, 48 kHz sound; `--seg START-END` cuts, played in order; needs ffmpeg). One of the random boot openings | the drive openings only |
| Second intro video | `python tools/gen_intro.py clip.mp4 --seg 0-99 --out max --letterbox --q 8 --fps 16` → `data/max.bin` + `.pcm` (the whole 16:9 frame under the boot log, 16 fps), another random opening | one video opening fewer |
| Update screen backdrop | `python tools/gen_backdrop.py` → `data/backdrop.bin` (Green Hill Zone, rebuilt from the s1disasm disassembly, fetched into `sonic-sprites/ghz/`) | the battle box on firmware updates |
| Shadow on the update rail | `python tools/gen_shadow_rider.py` → `data/shadow.bin` (his Sonic Battle running loop, fetched into `sonic-sprites/shadow/`) | two jet glows ride the rail |
| Sonic waiting | `python tools/gen_idle_sprite.py` → `data/idle.bin` (rebuilt from the s1disasm disassembly, fetched into `sonic-sprites/`) | Pikachu on "Waiting for a song..." |
| Wolf howl | `python tools/gen_howl.py wolf.ogg` → `data/howl.pcm` (cut 13-19.5 s by default, `--start`/`--end`; needs ffmpeg), for example the public-domain ["Wolf howls.ogg"](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg) | a synthesised howl |
| Car sprite | edit and run `python tools/gen_car_sprite.py` (`--png out.png` previews it) | a silver sedan |
| Song colours | your picks in `src/util/black/songpicks.inc`, colour `#tags` in a playlist's description ([how](docs/yellow-deck.md#which-colour)) | mood, genre and cover |

Convert other audio formats to WAV first, for example `ffmpeg -i in.mp3 in.wav`. After
changing your media, run `python tools/media_sizes.py record` and commit
`data/media_sizes.txt` (sizes only): CI builds stand-ins of those sizes, so its flash check
counts your real media.

## How it works

On the black deck the ESP32-S3's two cores split the work: one task polls Spotify and downloads
art, lyrics, genres and sprites; the other only draws. Results are tagged with the song they
belong to, so a skipped song's art never lands on the next one. The yellow deck keeps Bluetooth
(lights, gamepad) on one core and the screen on the other, with a small web server for voice and
the watch.

- [Architecture](docs/ARCHITECTURE.md): both decks' tasks and endpoints, the contracts between
  the devices, the module layout.
- [Yellow deck](docs/yellow-deck.md): its screens, presets, music mode, voice and watch.
- [Design and performance history](docs/HISTORY.md): what was measured on the device and how
  each freeze, leak and crash was fixed.
- [Backlog](docs/BACKLOG.md): open items and ideas.

## Development

```sh
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1   # host unit tests (Unity, g++)
python -m unittest discover -s tools -p "test_*.py"            # media tool tests
pio run -e s3 -e ledpanel                                       # both decks
```

CI builds both decks (`s3`, `ledpanel`) and their hardware checks (`hwcheck_s3`, `hwcheck`),
checks each app's flash budget and the S3's media pack, runs the host tests with ASan/UBSan and
again as 32-bit, and runs the media tool tests. It doesn't build the watch app. See
[CONTRIBUTING.md](CONTRIBUTING.md) for setup, test rules and commit style.

### Versioning

[Semantic versioning](https://semver.org), released as git tags `vMAJOR.MINOR.PATCH`; changes
are listed in [CHANGELOG.md](CHANGELOG.md). Each build reads its version from git
(`tools/version.py`): the black deck shows it on its boot and status screens and in its first
serial line, `[boot] cardeck v2.0.0 (s3)`; the yellow deck types `FW 2.0.0` under its boot lines
and adds it to `/status`. Builds past a tag or with local edits say so: `2.0.0+3.gabc1234`.

- **major:** an update needs action, such as new wiring, a new partition table, a changed
  `config.h` format, or a protocol change that means updating every device together.
- **minor:** a `feat` commit: something new on a screen, the speaker, the lights or the watch.
- **patch:** `fix`, `perf` and any other change to the firmware's behaviour.

To release: add the version's section to `CHANGELOG.md`, commit, then
`git tag -a v2.0.0 -m "v2.0.0"` and `git push origin main v2.0.0`.

## Security

Credentials live only in the git-ignored `src/config.h` (and the watch's `Secrets.mc`). See
[SECURITY.md](SECURITY.md) for what each endpoint accepts, known limitations and how to report
a problem.

## Credits and legal

Built on PokéAPI, PMD SpriteCollab, LRCLIB, Open-Meteo, GNU Unifont, Twemoji, the VT323 font and
several open-source libraries; see [CREDITS.md](CREDITS.md), which also lists the files in this
repository that are **not** covered by the MIT license.

CarDeck is a personal, non-commercial fan project. It is not affiliated with, endorsed by or
sponsored by Spotify, Nintendo, The Pokémon Company, Creatures Inc., GAME FREAK inc., SEGA,
Microsoft, Garmin or Honda. Pokémon and Pokémon character names are trademarks of Nintendo.
Spotify is a trademark of Spotify AB.

Code: [MIT](LICENSE).
