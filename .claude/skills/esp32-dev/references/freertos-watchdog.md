# FreeRTOS tasks, cores and the watchdog

## Who runs where (from `xTaskCreatePinnedToCore` calls in `src/`)

**Black deck**

| Task | Core | Stack | Prio | File |
|---|---|---|---|---|
| `loop()` (UI, draws only) | 1 | 8 KB (core default) | 1 | `src/black/main.cpp` |
| `net` (every HTTPS request and PNG decode) | 0 | 10 KB | 1 | `src/black/core/nettask.cpp` |
| `ota` | 0 | 4 KB | 1 | `src/common/net/ota.cpp` |
| `gps` | 0 | 4 KB | 1 | `src/black/car/gps.cpp` |
| `obd-link`, `obd-ask` | 0 | 6 KB each | 1 | `src/obd/obdlink.cpp` |
| `remote` (web server) | 1 | 4 KB | 0 (idle) | `src/black/net/remote.cpp` |
| `music` (UDP palette sender, on demand) | 1 | | 1 | `src/black/audio/musicsvc.cpp` |
| `bootscene`, `intro` (boot only) | 1 | 6 KB | 1 / 2 | `src/black/ui/` |
| sound / `camsnd` (unpinned) | any | 3 KB | 1 / 2 | `audio/greeting`, `car/camsound` |

**Yellow deck**: `loop()` core 1 (screen, touch, pad, IR, NVS saves); `leds-link` core 0, 4 KB;
`voice` core 1 idle, 4 KB; `deck` core 1 idle, 5 KB; `music` core 1 idle, 4 KB, only while music
mode is on.

**OBD deck**: `loop()` core 1; `obd-link`, `obd-ask` core 0; `net` core 1 idle, 6 KB; `ota` core 0.

Arduino-ESP32 puts WiFi/lwIP and the BT controller on core 0 by default; long CPU work on core 0
competes with them.

## Rules

1. **One task owns the display.** The UI loop draws; network/BLE tasks hand results over through
   shared state (black deck: `core/shared.cpp`, one mutex, `AppState` snapshot + a mailbox whose
   results carry `trackGen`, so a skipped song's late art is dropped).
2. **Never block the UI loop on the network.** The original single loop froze the UI ~1.5 s per
   poll and ~19 s per skip (HISTORY items 5-6).
3. **Hold a mutex briefly**: copy out, release, then work. Never draw or do I/O while holding it.
4. **No busy-wait on core 0.** `yield()` does not let a lower-priority idle task run; a library's
   spin loop starved IDLE0 for ~11 s and tripped the 5 s watchdog (`rst:0xc`, HISTORY item 12).
   Use `vTaskDelay(1)` or a blocking wait.
5. **Stack sizes come from the `[mem]` high-water marks**; keep ~2 KB free. An overflow shows as
   `Stack canary watchpoint triggered` or `A stack overflow in task ...`.
6. **A task that deletes itself** calls `watchdog::forget()` first.
7. **Callbacks from stacks** (NimBLE notify, Bluedroid GATT, WiFi events, the failed-alloc hook)
   run in someone else's task: queue the work, return quickly.

## Watchdog (`src/common/core/watchdog.h`)

- `watchdog::begin()` early in `setup()`: task watchdog **90 s**, panic on expiry.
- Long-running tasks call `watchdog::watch()` at start and `watchdog::feed()` every pass; the
  `loop()`s use the core's `enableLoopWDT()`.
- 90 s is above the longest legitimate wait between feeds: the S3's request (DNS 15 + connect 5 +
  TLS 8 + head 8 + body 20 = 56 s), a Bluedroid connect + discovery (~45 s), a WiFi join (~9 s).
  New code that can wait longer than ~50 s must feed inside the wait.
- A hang reboots within 90 s; the reset reason shows `task watchdog` in `[boot] reset reason`.
- Other watchdogs: `TG0WDT_SYS_RESET` / `TG1WDT_SYS_RESET` in the ROM's `rst:` line are the timer
  group watchdogs (TG1 is the interrupt watchdog: an ISR or a critical section that runs too long,
  or a crash in early boot, e.g. the NimBLE 4-connection crash loop on the CYD).

## Staying up after a bad update

- **IDF app rollback**: an OTA image boots on probation until `ota::onOnline` / a good poll
  confirms it (`[ota] update confirmed`), about a minute online. **Keep the deck powered for a
  minute after an update**; any reset before that boots the previous firmware.
- **`bootguard`**: 3 crash boots in a row before the app's own sign of working boot the other slot
  (`[ota] rolled back`). A USB flash resets otadata, so a USB image is never on probation.
- **Self-heal** (black deck): 180 s without a good poll while WiFi is up restarts the board.

Sources: `src/` task creations, `src/common/core/watchdog.h`, `docs/ARCHITECTURE.md` (task tables,
"Shared: network, updates and staying up"), `docs/HISTORY.md` items 5-6, 12; commit bf07d99 (#27).
