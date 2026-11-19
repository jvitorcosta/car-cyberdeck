# BLE, Bluetooth Classic and WiFi together

The ESP32 and ESP32-S3 share one 2.4 GHz radio between WiFi and Bluetooth (software
coexistence). Everything below is about heap and airtime, not just API calls.

## Which stack where

| Deck | Stack | Why |
|---|---|---|
| Black (S3) | NimBLE-Arduino 2.5.1, central + observer only, 1 connection, host memory in PSRAM | the Vgate OBD adapter; the S3 has BLE only (no Classic) |
| Yellow (CYD) | Bluedroid dual mode (ESP-IDF GATT client + Classic) | the PS3 pad needs Classic; the PS3 library starts Bluedroid; NimBLE cannot share the controller with it |
| OBD deck | NimBLE 2.5.1, central + observer | the adapter only |
| Diag envs `blescan`/`bletest` | NimBLE 2.5.1 | |

Flags on the NimBLE envs: `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED=1`,
`CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED=1` (smaller, ~14 KB flash), and on the S3
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`, `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=1`.

## Hard limits learned on the device

- **Classic ESP32: at most 3 BLE connections.** The precompiled Arduino sdkconfig reserves
  controller memory for 3; NimBLE max 4 crash-looped (TG1WDT 2.3 s after boot, 6 of 6 tries).
  That is why the OBD link got its own board, then moved to the black deck.
- **Bluedroid dual mode costs most of the CYD's heap.** With WiFi up the yellow deck had ~16 KB
  after the PS3 port and crashed when WiFi joined; features came off (#52) and the heap guard and
  WiFi buffer cap came in (#66). Do not add features to the yellow deck without reading the
  `[mem]` line's `min` first.
- **WiFi power save stays on when BT is on.** With BT enabled, IDF refuses `WIFI_PS_NONE`
  (coexistence). Don't call `WiFi.setSleep(false)` on a deck that runs Bluetooth.
- **Cap WiFi's buffers on the yellow deck.** `-Wl,--wrap=esp_wifi_init` (`src/yellow/wifibufs`)
  sets 12 RX / 12 TX buffers and no AMPDU RX, so a packet burst can't take Bluetooth's heap.
- **mDNS next to Bluetooth often misses** on the yellow deck: the lookup runs in the background and
  the answer is saved in NVS (`deck`/`addr`) (#60, 9045a49).
- **NimBLE 1.4 vs 2.x bonds**: an env on NimBLE 1.4 boot-loops on bonds NimBLE 2 left in NVS.
  Every env here is on 2.5.1 now; if an old env comes back, erase first (`pio run -e <env> -t erase`).
- **The PS3 library's 4 KB send writes through NULL on a short heap.** The yellow deck wraps it
  (`-Wl,--wrap=ps3_l2cap_send_hid`, `yellow/padhid`) with a right-sized, retrying send (#54, b98c0c2).

## The OBD BLE client pattern (`src/obd/obdlink.cpp`, `src/util/car/obdble`, `obdpick`)

Used by the black deck (`-D OBD_LINK`) and the OBD deck. Two tasks on core 0:

1. **`obd-link`** finds and connects: the remembered address from NVS (`obd`: `bleaddr`,
   `bletype`, `blename`) first, then a 4 s scan. Retry 5 s apart, doubling to 60 s while no adapter
   answers. The whole scan is weighed (`obdpick`): an adapter-like name wins, a nameless device
   with a serial-like service only when nothing matched; the LED controllers (`ELK`/`MELK`, their
   MACs from `config.h`) are never picked; a device that failed is skipped for 10 min.
2. **GATT**: pick one service that has both a notify and a write characteristic (FFF0/FFF1/FFF2
   style, or an unknown 128-bit one) and pair them **within one service** only (874f2cc).
3. **`obd-ask`** while connected: ELM327 setup, the supported-PID scan, PIDs in `obdplan` order,
   `ATRV` at least every 4 s, modes 03/07 every 5 min. Replies arrive as 20-byte notifications;
   assemble them up to the `>` prompt and anchor the reply markers so another PID's data isn't
   taken (29273a4).
4. **Read-only gate**: every write goes through `obd::allowed()` (mode 01/03/07 and adapter setup;
   never mode 04 or `ATH1`). Keep it that way; the OBD2 skill (`.claude/skills/obd2/`) has the
   protocol side.
5. **Watchdog**: feed before a connect and before discovery (a connect can wait ~30 s, discovery
   ~10 s); both are under the 90 s task watchdog.
6. **Notify callbacks run in the NimBLE host task**: copy the bytes into a queue or ring buffer and
   return; never block, allocate large, log heavily or touch the display there.

The bench build (`obddeck_bench`) and `tools/obd_sim.py` / `tools/obd_probe.py` let you test this
without the car.

## Bluedroid GATT client (yellow deck, `src/yellow/elklink`)

Two LED controllers, service FFF0, characteristic FFF3, **written by handle**. Connect in rounds
5 s apart, doubling to 60 s while one is missing. A connect is waited on up to 35 s (the stack's
own 30 s timeout) and discovery up to 10 s. A light gets its latest look at most every 300 ms
during a burst (#54).

## Bluetooth gets the radio first (yellow deck)

Raising Bluetooth's share took the DS3's reports from ~20 to ~45 a second at the desk (#54).
If pad input feels laggy, check coexistence before the input code.

Sources: `platformio.ini` comments, `docs/ARCHITECTURE.md` (Yellow deck, Live gauges, OBD deck),
commits 81f01da (#66), b98c0c2 (#54), 9045a49 (#60), 334edd7 (#69), 874f2cc, 29273a4; the health
branch notes (4th connection crash loop) and the music-branch research note (WiFi PS with BT).
