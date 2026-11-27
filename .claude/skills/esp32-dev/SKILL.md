---
name: esp32-dev
description: Use for any firmware work on the car decks in this repo (black deck = Freenove ESP32-S3, yellow deck = ESP32 CYD, OBD deck = plain ESP32) - building, flashing, serial logs, memory/heap/PSRAM, TFT_eSPI display speed, BLE/WiFi, FreeRTOS tasks, watchdog resets, crashes, upload failures, partitions or platformio.ini. Also when reviewing a firmware PR here.
---

# ESP32 development on the car decks

The embedded side of this repo: three firmwares on two kinds of chip, one `platformio.ini`.
Read the reference that matches the task before changing code:

| Task | Read |
|---|---|
| Which board, pins, port, display, radio | [references/hardware.md](references/hardware.md) |
| envs, build flags, partition tables, CI's flash budget | [references/envs-partitions.md](references/envs-partitions.md) |
| heap, PSRAM, fragmentation, TLS budget, TFT_eSPI speed | [references/memory-display.md](references/memory-display.md) |
| BLE + WiFi coexistence, NimBLE vs Bluedroid, the OBD BLE client | [references/ble-wifi.md](references/ble-wifi.md) |
| tasks, cores, mutexes, the 90 s watchdog, rollback | [references/freertos-watchdog.md](references/freertos-watchdog.md) |
| upload fails, port busy, boot mode, the media pack upload | [references/serial-upload.md](references/serial-upload.md) |
| "has this broken before?" | [references/pitfalls.md](references/pitfalls.md) |

The OBD-II protocol itself (PIDs, ELM327 commands, ECUs) is the `obd2` skill's job.

## Hard rules

1. **Never flash a device unless João asked for it in this session.** Building is always fine.
   Flashing, `uploadmedia`, `erase` and OTA uploads change his car's hardware.
2. **Close every serial monitor/capture before flashing** and before testing a "works only when
   plugged in" bug. One open port blocks the upload; the S3's tx timeout must stay 1 ms, never 0.
3. **Measure byte-addressable heap**, not `ESP.getFreeHeap()`: `mem::byteFree()/byteLargest()`.
   Watch the largest block: a TLS session takes two 16.7 KB record buffers, plan ~40 KB in one
   block of internal RAM (CONTRIBUTING.md).
4. **Yellow deck: no new features without reading the `[mem]` line's min** (~30 KB on the desk,
   panics at 15-19 KB). No TLS on it. No 4th BLE connection on any classic ESP32.
5. **Big CPU-only buffers go to PSRAM on the black deck** (`mem::bigAlloc`); never DMA/ISR buffers.
6. **One task draws.** Network and BLE tasks never touch the display; no `initDMA()`.
7. **Every long-running task feeds the watchdog** (`watchdog::watch()/feed()`, 90 s).
8. **Don't bump `espressif32@6.9.0` or library versions in a feature PR.** Pinned for a reason.
9. **`src/config.h` and `data/` are git-ignored secrets/media.** Never commit, print or paste them;
   the repo is public. A worktree build needs `cp include/config.example.h src/config.h`.
10. **No `-Werror` escapes in `src/`.** Header-only libraries that warn go under `lib/`.

## Build, flash, monitor

PlatformIO lives at `D:\.platformio`; `pio` is on PATH. Build from the repo (or worktree) root.

```bash
# one-time per worktree (what CI does)
cp include/config.example.h src/config.h      # only if src/config.h is missing; real one stays local
python tools/media_sizes.py placeholders       # only if data/ media are missing

pio run -e s3 -e ledpanel -e obddeck           # build the three firmwares (CI also builds hwcheck_s3, hwcheck, obddeck_bench)
powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1   # host tests (Unity + g++)
python -m unittest discover -s tools -p "test_*.py"            # tool tests
```

| Deck | Build | Flash (only when asked) | Monitor / capture |
|---|---|---|---|
| Black (S3, COM12) | `pio run -e s3` | `pio run -e s3 -t upload --upload-port COM12`; media: `pio run -e s3 -t uploadmedia --upload-port COM12`; WiFi: `pio run -e s3_ota -t upload` | `python tools/capture_serial.py COM12 120 out.log --no-reset` |
| Yellow (CYD, CH340 COM13-18) | `pio run -e ledpanel` | `pio run -e ledpanel -t upload --upload-port COMx` (USB only) | `python tools/capture_serial.py COMx 120 out.log` (opening resets it) |
| Pokédeck (4.3in S3, native USB) | `pio run -e pokedeck` | `pio run -e pokedeck -t upload --upload-port COMx` (never the black deck's port), then `pokedeck_ota` | `python tools/capture_serial.py COMx 120 out.log --no-reset` |
| OBD (devkit) | `pio run -e obddeck` | `pio run -e obddeck -t upload --upload-port COMx`, then `obddeck_ota` | same as yellow |

Then `python tools/serial_health.py out.log` (resets, crashes, heap, stacks; `--strict` exits 1
on a crash) and, for the black deck's network timings, `python tools/analyze_session.py out.log`.
`pio device monitor -p COMx` also decodes backtraces (`esp32_exception_decoder`).

## Before flashing: checklist

- [ ] João asked for this flash in this session, and named the deck.
- [ ] Right env for the deck (`s3` black, `ledpanel` yellow, `obddeck` OBD); `pio device list`
      shows the port, and it is that deck's port.
- [ ] Build passes; `python tools/check_flash.py .pio/build/<env>/firmware.bin <slot>` under 97 %
      (`0x300000` s3, `0x1E0000` ledpanel/obddeck).
- [ ] No serial monitor, capture or other session holds the port (ask before stopping another
      session's capture).
- [ ] Partition table unchanged, or the flash is over USB (an OTA can't change it) and NVS stays
      at 0x9000.
- [ ] Media changed? Then `uploadmedia` too (black deck), from main or a branch based on it,
      at the default 460,800 baud (921,600 corrupted it); otherwise skip it (~10 MB).
- [ ] After an OTA: keep the deck powered about a minute until `[ota] update confirmed`.
- [ ] After flashing: capture 60-120 s and run `serial_health.py`; report the heap min and any reset.
