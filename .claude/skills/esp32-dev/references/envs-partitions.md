# PlatformIO envs and partitions

Everything here is read from `platformio.ini` and `partitions_s3.csv`. Re-read them if this file
and the code disagree: the code wins.

## Shared settings

- `platform = espressif32@6.9.0` (Arduino core 2.0.17 on ESP-IDF 4.4), `framework = arduino`.
  Pinned: do not bump it in a feature PR; a core bump changes heap, BLE and USB behaviour.
- `monitor_speed = 115200`, `monitor_filters = esp32_exception_decoder` (decodes a backtrace when
  `pio device monitor` runs from the project folder with the matching `firmware.elf`).
- `-std=gnu++17` (gnu++11 unflagged), `CORE_DEBUG_LEVEL=1` (core logs errors only; the decks'
  own `[tag]` lines print with `Serial` directly).
- TFT_eSPI is configured by `-D` flags (`USER_SETUP_LOADED=1`, `ILI9341_2_DRIVER`), never by
  editing the library's `User_Setup.h`.
- `build_src_flags` adds `-Wall -Wextra -Wshadow ... -Werror` for `src/` only. A header-only
  library that `#warning`s (IRremote) must live in `lib/` (see `lib/irrx`) or the build fails.
- Each deck's env compiles an **allow-list** (`build_src_filter = -<*> +<black/> ...`). A call into
  another deck's folder fails to link: that is the design, not a bug. Add new shared code under
  `src/common/` or the right `src/util/<group>/`.
- Pre-build scripts: `tools/version.py` writes `include/fw_version.h` from the git tag; the
  `*_clip.py` scripts fall back to a default or empty asset when a git-ignored media file is missing.
- `src/config.h` is git-ignored (secrets). A fresh worktree needs one:
  `cp include/config.example.h src/config.h` (what CI does). Never commit it, never print it.
- Media files under `data/` are git-ignored. For a build without them, CI runs
  `python tools/media_sizes.py placeholders` (stand-ins at the real sizes).

## Envs

| Env | Board | Partitions | What it is | Upload |
|---|---|---|---|---|
| `s3` (default) | esp32-s3-devkitc-1, `qio_opi`, 16 MB | `partitions_s3.csv` | black deck firmware | USB (native) |
| `s3_ota` | same | same | black deck over WiFi | espota to `cardeck.local` or `--upload-port 172.20.10.10`, `OTA_PASSWORD` |
| `hwcheck_s3` | same | same | S3 display/backlight/PSRAM/codec check | USB |
| `ledpanel` | esp32dev | `min_spiffs.csv` | yellow deck firmware | USB only (`NO_WIFI_UPDATES`) |
| `ledpanel_probe` | esp32dev | `min_spiffs.csv` | yellow deck + IR effect probe | USB |
| `hwcheck` | esp32dev | `huge_app.csv` | CYD display/touch check | USB |
| `blescan`, `bletest` | esp32dev | `huge_app.csv` | BLE scan / LED controller test (NimBLE 2.5.1) | USB |
| `ps3test`, `irtest` | esp32dev | `huge_app.csv` | PS3 pad / IR receiver tests | USB |
| `pokedeck` / `pokedeck_ota` | esp32-s3 (4 MB, `qio_opi`) | `min_spiffs.csv` | Pokédeck (4.3in panel) base | USB first, then espota to `pokedeck.local` only |
| `obddeck` | esp32dev | `min_spiffs.csv` | OBD deck (no screen) | USB first, then `obddeck_ota` |
| `obddeck_bench` | esp32dev | `min_spiffs.csv` | OBD deck with a simulated drive | USB |
| `obddeck_ota` | esp32dev | `min_spiffs.csv` | OBD deck over WiFi | espota to `cardeck-obd.local` |
| `native` | host | - | Unity host tests; run by `tools/run_tests.ps1`, not `pio test` | - |

CI (`.github/workflows/ci.yml`) builds `-e s3 -e hwcheck_s3`, `-e ledpanel -e hwcheck`,
`-e obddeck -e obddeck_bench`, then runs `tools/check_flash.py` on each `firmware.bin`
(limit 97 % of the app slot) and on the media pack (95 % of 0x9E0000).

## Black deck partition table (`partitions_s3.csv`)

| Name | Type | Offset | Size | Notes |
|---|---|---|---|---|
| nvs | data/nvs | 0x9000 | 0x5000 (20 KB) | settings, crash ring, OBD adapter; small: do not store lists here lightly |
| otadata | data/ota | 0xE000 | 0x2000 | which app slot boots |
| ota_0 | app | 0x10000 | 0x300000 (3 MB) | `board_upload.maximum_size = 3145728` |
| ota_1 | app | 0x310000 | 0x300000 (3 MB) | |
| media | data/0x40 | 0x610000 | 0x9E0000 (~9.9 MB) | the media pack (`tools/pack_media.py`), read-only at run time |
| coredump | data/coredump | 0xFF0000 | 0x10000 | |

The app is ~1.6 MB of its 3 MB slot (51 %, build of 2026-11-19). The media (intro video, clips, montage, CJK font, emoji)
are not in the app: `pio run -e s3 -t uploadmedia` packs `data/` and writes it at 0x610000 with
esptool (`tools/media_upload.py`). Only `data/greeting_default.pcm` is embedded, so a deck with an
empty media partition still boots with sound.

## Yellow and OBD decks (`min_spiffs.csv`, from the core)

nvs 0x9000 (0x5000), otadata 0xE000, two app slots of 0x1E0000 (1,966,080 B), a 128 KB data
partition (`spiffs`, used as LittleFS by the OBD deck's trip log). The yellow firmware is
~1.71 MB of its slot (86.9 %, build of 2026-11-19; the ini comment's 1.39 MB is older): the
yellow deck's flash is getting tight too. The OBD deck is ~1.18 MB (60 %). `huge_app.csv` (diag envs) has one 3 MB app and no OTA.

**NVS stays at 0x9000 / 0x5000 in every table**, so a USB flash that switches tables keeps the
settings (touch calibration, looks, trip). Keep it that way when you touch a table.

## Changing a partition table

- Only over USB (an OTA update cannot rewrite the table).
- Keep nvs and otadata where they are.
- Recompute `board_upload.maximum_size` and the CI `part:` value.
- Note the size budget in the PR: the app's % of its slot from `check_flash.py`.

## Rules that bite

- `PNG_MAX_BUFFERED_PIXELS` (and any flag that changes a library's struct) goes in
  `build_flags`, not `build_src_flags`: the library must see the same struct size as `src/`.
- `pio pkg install -l ...` in a worktree **rewrites `platformio.ini`** and strips its comments.
  Restore the file from git afterwards (memory note `ir-remote`).
- Test hooks (`-D INTRO_FORCE`, `-D NIGHT_TEST`, ...) go in `$env:PLATFORMIO_BUILD_FLAGS` for one
  desk build, never into `platformio.ini` (`docs/ARCHITECTURE.md` "Test hooks").

Sources: `platformio.ini`, `partitions_s3.csv`, `.github/workflows/ci.yml`, `tools/media_upload.py`,
`tools/check_flash.py`, `docs/ARCHITECTURE.md`.
