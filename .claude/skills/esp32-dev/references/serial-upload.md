# Serial, upload and boot-mode troubleshooting

## The two USB paths

| | Black deck (S3) | Yellow / OBD deck (classic ESP32) |
|---|---|---|
| USB | the chip's own USB Serial/JTAG (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`; `Serial` is `HWCDC`) | CH340 USB-UART bridge |
| Auto reset into the bootloader | esptool's USB-JTAG reset sequence | DTR/RTS on the bridge |
| Opening the port | does **not** reset, if DTR/RTS stay low | **resets** the board (DTR/RTS) |
| During a reset | the COM port disappears and comes back (re-enumeration) | the port stays |
| Manual download mode | hold BOOT, tap RESET, release BOOT | same (BOOT = GPIO 0) |

## Symptom -> cause -> fix

**"Could not open COMx, the port doesn't exist" / `PermissionError(13)` / Access denied**
- Another program holds the port: a `pio device monitor`, a `capture_serial.py`, a `cap.py`-style
  recorder from another Claude session, Arduino IDE, or the Garmin/phone tools. On Windows "doesn't
  exist" is often really "in use" (memory `home-wifi-band-isolation`).
- Fix: find and stop the holder (ask before stopping another session's capture), then retry once.

**Upload drops mid-write at a random % ("A device which does not exist was specified")**
- Seen on the S3 at COM12 (2026-10-15) at 1 %, 9 %, 79 %, even at 115200 baud. A hub, a long or
  charge-only cable, or another deck on the same hub. `--no-stub` is not supported over the S3's
  USB-JTAG.
- Fix: ask João to reseat: other deck unplugged, a direct port, then manual download mode
  (BOOT + RESET). Do not loop retries (memory `s3-flash-drops`). A direct esptool write also works:
  `python <pio>/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port COM12 write_flash -z 0x10000 .pio/build/s3/firmware.bin`.

**Deck dark and silent after a successful flash**
- After manual download mode the S3 can stay in the ROM bootloader (the PC's reset keeps it
  there): no log, black screen.
- Check: `esptool.py --chip esp32s3 --port COM12 --before no_reset chip_id` -> "Staying in
  bootloader" means it is still in download mode. Fix: one RESET press.

**The S3 misbehaves only while plugged into the PC ("No internet", stale songs) and recovers
the moment a monitor opens**
- `HWCDC::write` with `setTxTimeoutMs(0)` waits **forever** in core 2.0.17 when the port is
  enumerated but nobody reads it. The first log line froze the network task.
- Fix (in place since bf5866a): `Serial.setTxTimeoutMs(1)` in `src/black/board/s3/board_s3.cpp`;
  a dropped log line is fine. `serialcmd` raises it to 1000 ms only while `SHOT` streams a
  screenshot. **Never set it to 0.** Test this class of bug with the port closed.

**Every capture reboots the S3**
- pyserial raises DTR/RTS on open, which the S3's USB-JTAG reads as a reset request.
- Fix: open with `dtr=False`, `rts=False` before `open()`: `capture_serial.py ... --no-reset` does
  this, and reopens the port when it re-enumerates. The CYD resets on open by design (fine for a
  boot log).

**Sounds are quiet on the desk**: not a bug. While `HWCDC::isPlugged()` (a PC host on USB) the S3
plays at a tenth (-20 dB) (#69). A car charger sends no USB frames: full volume. In the car, a USB
port that is a data host (the head unit) also makes the deck think a PC is there: use a plain
charger.

**Brownout resets (`Brownout detector was triggered`, reset reason `brownout`)**
- WiFi TX + backlight + audio peaks on a weak supply: the car's USB port or a thin cable. Use the
  5 V 2 A charger. Not a firmware bug (memory `yellow-heap-guard`).

**Wrong board / wrong port**
- `pio run -e ledpanel -t upload` to the S3's port (or `s3` to the CYD) fails at chip detection
  ("This chip is ESP32-S3, not ESP32"). Check `pio device list` first; the CH340 number changes
  between sessions (COM13, 16, 17, 18 seen).

**`uploadmedia`: the media pack (~10 MB) to 0x610000**
- `tools/media_upload.py` runs esptool `write_flash -z 0x610000 .pio/build/s3/media.bin` at the
  env's `$UPLOAD_SPEED`. Nothing in `platformio.ini` sets `upload_speed`, so it is the board
  file's default: **460,800 baud** for both `esp32-s3-devkitc-1` and `esp32dev`
  (`<pio>/platforms/espressif32/boards/*.json`, `"upload": {"speed": 460800}`). On the S3's
  USB-JTAG the number is nominal (USB runs at USB speed); on a CH340 it is the real line rate.
  It takes minutes; a drop mid-way leaves a half pack, which the firmware rejects by CRC and
  treats as no media (the default chime still plays).
- If it drops: same as any S3 drop (reseat, direct port), then rerun `uploadmedia`. The app need
  not be reflashed. Lowering the speed (`--baud 115200` in a direct esptool call) did not help the
  S3's drops on 2026-10-15: the cause was the USB link, not the rate.
- **Stay at 460,800 for the pack.** A direct esptool call at 921,600 on COM12 failed at 14 % with
  `A fatal error occurred: Corrupt data, expected 0x1000 bytes but received 0xc5f bytes`; the same
  job at 460,800 went through (2026-11-19, the 2D car animation session, PR #76). A CH340 behind a
  USB hub also fails at 921,600 (2026-11-13).
- Reading the pack back to verify it: `esptool.py --chip esp32s3 --port COM12 --baud 460800
  read_flash 0x610000 <size> media_read.bin`, then check each file's CRC (the pack header starts
  `CDMEDIA1`; `tools/pack_media.py` has the layout) or compare with `.pio/build/s3/media.bin`.
- **Run `uploadmedia` from main** (or a branch based on it). A branch cut before #37 packs an
  older file list and wipes the boot sound rosters (`.snd`): the boot log then shows
  `[boot] sounds: drive=file ...` and the same sound every boot. Check the `[boot] cardeck v...`
  line, then rerun from main (#37 092795c, #44 9b3c9e6).
- On a CYD (CH340), if an upload fails with "Invalid head of packet" or timeouts, retry once at
  115,200 with a direct esptool call (`--baud 115200 write_flash 0x10000
  .pio/build/ledpanel/firmware.bin`). Don't commit an `upload_speed` change for a one-off.

**OTA (`s3_ota`, `obddeck_ota`) can't reach the deck**
- The PC must be on the same 2.4 GHz network. A PC on the same SSID at 5 GHz could not reach a
  deck at 2.4 GHz on one router (band isolation): flash over USB instead.
- On the hotspot: `--upload-port 172.20.10.10` (black) / `.12` (OBD). Windows may ask for a
  Python firewall exception (espota listens for the deck's call back).
- `OTA_PASSWORD` must be set in `src/config.h` (`tools/ota_auth.py`).
- After the upload, keep the deck powered ~1 min (`[ota] update confirmed`) or it rolls back.

**NimBLE env boot-loops right after flashing a diag build**: old bonds in NVS from another NimBLE
major version. `pio run -e <env> -t erase` (erases the whole flash, NVS included: the deck's
settings and touch calibration go too; ask João first).

## Decoding a crash

1. Capture from boot: `python tools/capture_serial.py COMx 120 out.log [--no-reset]`.
2. `python tools/serial_health.py out.log`: reset reasons, the crash ring, Guru Meditation /
   abort / watchdog / brownout, heap and stack lows.
3. Backtrace: `pio device monitor` from the repo root decodes it live (`esp32_exception_decoder`);
   offline, `xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/s3/firmware.elf <addresses>`
   (`xtensa-esp32-elf-addr2line` for the CYD) with the exact `firmware.elf` that was flashed.
4. The crash ring survives the reboot: `[crash] last run: panic after 312s: heap ... at <phase>`
   names the phase that was running (`crashrecord::mark`).

Sources: memory notes `s3-usb-serial-stall`, `s3-flash-drops`, `home-wifi-band-isolation`,
`ir-remote`, `black-obd-gauges`; `docs/HISTORY.md` item 15; `tools/capture_serial.py`,
`tools/media_upload.py`, `tools/pack_media.py`; commits bf5866a, 334edd7 (#69).
