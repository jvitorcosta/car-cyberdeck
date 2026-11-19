# Known pitfalls in these repos

Each one cost real time once. Pointers: commit / PR in this repo, `docs/HISTORY.md` item, or a
memory note (João's Claude memory for this project; the name is the note's slug).

## Memory

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| `ESP.getFreeHeap()` lies | read "~100 KB free" while byte RAM was exhausted; TLS failed 500 of 600 s | measure `MALLOC_CAP_8BIT \| INTERNAL`, watch the largest block | HISTORY 8-9 |
| TLS `-32512` (SSL_ALLOC_FAILED) | 16,717 B mbedTLS buffer found at most 9.7-16.4 KB contiguous | fixed boot buffers, plain HTTP for art, gate TLS on the largest block | HISTORY 8-9 |
| `getString()` on big JSON | `IncompleteInput` on a fragmented heap | HTTP/1.0 + filtered stream parse | HISTORY 3 |
| Two 16 KB slots + 16 KB stack | largest block 41 KB, art malloc failed during TLS | slots 10 KB, net stack 10 KB (HWM 5.3 KB) | HISTORY 7 |
| SD card on the S3/CYD | mount cost ~26 KB heap (~10 KB with `max_files=1`); lwIP allocfails | no SD; keep the slot unused | memory `sd-card-parked` |
| Yellow deck panics | 15-19 KB free, block 124 B-4 KB, 67 B allocs failing under Bluedroid dual mode + WiFi | `heapguard`, WiFi buffers capped (`--wrap=esp_wifi_init`), stacks trimmed | #66, 81f01da; memory `yellow-heap-guard` |
| Yellow deck feature creep | PS3 port left ~16 KB, lwIP aborted after WiFi joined | features removed; read `[mem]` min before adding any | #52, 3705276; memory `yellow-deck-scope` |
| Music mode eats the yellow heap | heap 33-38 KB on link checks, 21-27 KB with karaoke + music | cut music mode's 4 KB task first if tight | memory `terminal-line-link-tags` |
| I2S install per beep | ~8 KB churn every 1.5 s | install once per alarm | camera alerts branch notes |
| Static DRAM full | `.bss` overflowed by 4 KB (frame pool) | move to heap / PSRAM (`mem::bigAlloc`) | HISTORY 4; #22, e5e1b49 |
| Logging from the alloc-fail hook | printing from the WiFi driver slowed it | count atomically, `esp_rom_printf` only | HISTORY 9, 12 |

## Serial and upload

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| HWCDC tx timeout 0 | S3 froze on the first log line when a PC had the port but nobody read it | `Serial.setTxTimeoutMs(1)`, never 0 | bf5866a; HISTORY 15; memory `s3-usb-serial-stall` |
| Capture reboots the S3 | pyserial's DTR/RTS on open | `dtr=False, rts=False`; `capture_serial.py --no-reset` | memory `ir-remote` |
| Mid-write upload drops (COM12) | random % failures even at 115200 | reseat USB, direct port, BOOT+RESET; don't loop retries | memory `s3-flash-drops` |
| Stuck in bootloader after manual mode | dark, silent deck | `esptool --before no_reset chip_id`, one RESET press | memory `s3-flash-drops` |
| "Port doesn't exist" | another session's capture held COM12 | find the holder first | memory `home-wifi-band-isolation` |
| CH340 moves | yellow deck seen on COM13, 16, 17, 18 | `pio device list` every time | memory `terminal-line-link-tags` |
| esptool at 921,600 | media pack job died at 14 % ("Corrupt data, expected 0x1000 bytes but received 0xc5f"); CH340 behind a hub fails too | 460,800 (the board default) | 2026-11-19 2D car animation session (#76); 2026-11-13 session |
| `uploadmedia` from an old branch | pre-#37 pack list wiped the boot sound rosters: same sound every boot | check `[boot] cardeck v...`, rerun from main | #37 092795c, #44 9b3c9e6 |
| `SHOT` fails every other try (S3) | no `SHOT END` | retry; it is the desk tool | memory `black-obd-gauges` |
| `pio pkg install -l` in a worktree | rewrote `platformio.ini`, stripped comments | restore it from git | memory `ir-remote` |

## Display

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| TFT_eSPI `initDMA()` | every later draw black, with or without `deInitDMA()` | no DMA; ~50 ms a frame at 20 fps, skip late frames | dfbca0e, 6ac2565; HISTORY 16 |
| S3 SPI at 80 MHz | faster (24 -> 14 ms a frame) but parts of the picture corrupted | 40 MHz | `platformio.ini` comment (2026-11-03) |
| Full redraw per value | 1 Hz black flash | redraw only the changed region | health / gauges work |
| A CYD unit with inverted colours | white background on one board | `TFT_INVERT` for that unit; CYD batches differ | 2026-11-13 session |
| PNG sheets > 316 px | Doublade's 320 px RGBA sheet fell back | `PNG_MAX_BUFFERED_PIXELS=4128` in `build_flags` (not `build_src_flags`) | #7, f4c907a |
| UI froze on network | ~1.5 s per poll, ~19 s per skip | network task on core 0, mailbox with `trackGen` | HISTORY 5-6 |

## Bluetooth and WiFi

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| 4th BLE connection on a classic ESP32 | TG1WDT crash loop 2.3 s after boot, 6/6 | max 3; the OBD link moved boards | health branch notes; ARCHITECTURE "OBD deck" |
| PS3 lib's 4 KB send | wrote through NULL on a short heap: crash on pad connect | `-Wl,--wrap=ps3_l2cap_send_hid` (`yellow/padhid`) | #54, b98c0c2 |
| NimBLE and Bluedroid together | can't share the controller | yellow deck is Bluedroid only (lights via IDF GATT client) | #50, e7ed4f7 |
| NimBLE 1.4 vs 2 bonds | 1.4 envs boot-loop on 2.x bonds | all on 2.5.1; erase if an old env returns | #31, 4f27e67 |
| mDNS next to Bluetooth | yellow deck's lookup of `cardeck` mostly failed at home | background lookup, address saved in NVS | #60, 9045a49 |
| `WIFI_PS_NONE` with BT on | IDF refuses it | leave power save on | music-branch research note |
| 5 GHz PC vs 2.4 GHz deck | OTA/ping failed on one router's band isolation | flash over USB | memory `home-wifi-band-isolation` |
| GATT notify/write from two services | wrong pairing on some adapters | pair within one service | 874f2cc |
| OBD replies from another PID | taken as ours | anchor reply markers | 29273a4 |
| Car USB port = data host | deck quiet in the car (thinks a PC is there) | plain charger | memory `black-obd-gauges` |

## Tasks, watchdog, updates

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| Library spin with `yield()` on core 0 | IDLE0 starved ~11 s, `rst:0xc` task watchdog | watchdog 90 s, `vTaskDelay`, feed per request | HISTORY 12; #27, bf07d99 |
| Task blocked forever | deck frozen for the rest of the drive | task watchdog on every long task | #27, bf07d99 |
| Update crashed before `setup()` | crash loop until USB | IDF rollback + `verifyRollbackLater()`; bootguard | #27, bf07d99 |
| Reset right after an OTA | booted the old firmware | keep it powered ~1 min (`[ota] update confirmed`) | ARCHITECTURE "Firmware updates" |
| Library prints secrets | SpotifyArduino's hard-coded `SPOTIFY_DEBUG` logged the refresh token | pre-build patch turned it off (later replaced by the own client, #4) | HISTORY 12 |

## Flash and storage

| Pitfall | What happened | Fix / rule | Pointer |
|---|---|---|---|
| LittleFS fills by blocks | 128 KB partition full after ~28 power-ons | cap by real block use, 70 % | 74e0e71 |
| Media in the app | every code flash moved MBs | media partition + `uploadmedia` | #8, 05e695d |
| NVS is 20 KB | camera list would not fit alongside | keep NVS for settings; big data in the media pack or LittleFS | camera alerts branch notes |
| Header-only lib warns | `-Werror` in `src/` failed the build | wrap it in `lib/` (IRremote -> `lib/irrx`) | #58, bcd0b63 |

## Open (cause not found yet)

- Black deck: one Guru Meditation `StoreProhibited` on core 0, then `SW_CPU_RESET` (2026-11-13).
- Yellow deck: one Guru Meditation `IllegalInstruction` on core 0 (build v2.0.0+143, 2026-11-13).
- Brownouts in the car (2 at boot, 1 after 265 s, 2026-11-14/15): the car's USB power at engine
  start, not firmware; try another charger before debugging code.

If one comes back: capture from boot, decode the backtrace against that build's `firmware.elf`
(`serial-upload.md` "Decoding a crash"), and add the cause here.

Sources are cited per row; branch notes live outside the repo with the worktree notes.
