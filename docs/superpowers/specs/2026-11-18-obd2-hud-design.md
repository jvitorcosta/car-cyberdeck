# OBD2 live gauges on the black deck (Vgate iCar Pro 2S)

Date: 2026-11-18. Status: decided, phase 1 built and desk-checked; car test owed.

João has a Vgate iCar Pro 2S and wants the car's live data on a CarDeck screen: one screen, start
simple, an original digital gauge display in the spirit of early-2000s tuner dashboards. Out of
scope: anything that writes to the car (clearing codes above all) and the 3rd deck itself.

## What was already there

- `src/util/car/obd` (ELM327 requests and the reply parser: echo, `SEARCHING...`, several ECUs,
  no spaces; read-only gate `obd::allowed`), `obdplan` (request order, link health, staleness,
  warnings), `obdble` (which GATT characteristics carry the serial stream, 16-bit or 128-bit),
  `obdpick` (which scanned device is the adapter). All host-tested.
- `src/obd/obdlink.cpp`: the whole BLE link on NimBLE 2.5.1 (scan, connect, discover, subscribe,
  setup, supported-PID scan, the ask loop, NVS memory of the adapter), written for the screenless
  OBD deck (env `obddeck`). It has never met an adapter: the Vgate hadn't arrived, and the OBD deck
  board was never flashed.
- `tools/obd_sim.py`: a PC-side fake adapter (the reply table is host-tested).

## Decisions

### 1. The link is Bluetooth Low Energy

The iCar Pro 2S works with iPhones without MFi, which only an ELM327 on BLE can do, so it has a BLE
side whatever else it has (sellers disagree on a Classic side; some say BT 3.0 + BLE 4.0, others
BT 5.2). BLE is the one radio every deck can use: the black deck and the 3rd deck are ESP32-S3s,
which have no Bluetooth Classic. The old dual-mode clone's BLE was flaky (2026-10-19), but that was
a different adapter; the link already waits 2 s per reply and drops a late one.

The Vgate's GATT layout is unknown until it's in the car. The link logs every characteristic
(`[obd] gatt <service>/<char> w wn n i`) and `obdble::pick` takes FFF0, FFE0, 18F0, then any
service (128-bit ones kept apart) that has a notify and a write characteristic. The name it
advertises must pass `obd::looksLikeAdapter` (OBD, ELM, V-LINK / VLINK: Vgate's apps call it
`IOS-Vlink`); a nameless device is tried only when it advertises a serial service.

### 2. The black deck owns the link

| Deck | Why not / why |
|---|---|
| Yellow (ESP32 CYD) | Bluedroid dual mode for the PS3 pad and two LED controllers already runs it at 15-40 KB of heap; restarts were heap exhaustion (#66). Car health was taken off it on purpose (#52). No. |
| OBD deck (plain ESP32, no screen) | Its code exists, but it's a 4th board João hasn't got in the car, and it can't show anything. It stays as an option, unchanged. |
| **Black (ESP32-S3)** | **Yes.** No Bluetooth in use today, 8 MB PSRAM, the biggest screen, and it already serves the other decks over the hotspot. NimBLE's host memory goes to PSRAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`) and it allows 1 connection, so Spotify's TLS keeps its internal RAM (measured on the desk, below). |

The black deck compiles the same `src/obd/obdlink.cpp` the OBD deck uses (one link, two boards),
behind `-D OBD_LINK=1` in env `s3`. The link only ever talks to one adapter; the LED controllers
(by MAC and by name) are never tried, and the yellow deck keeps them.

### 3. How the other decks get the data

The black deck serves `GET /obd` (signed or `k=`, like `/link`): one flat JSON line from
`util/car/obdwire`, version 1:

```
{"v":1,"link":3,"rpm":2450,"kmh":62,"cool":88,"iat":34,"thr":18.4,"load":41.2,"volt":14.1,"seq":812}
```

`link` is `obdplan::Link` (0 off, 1 connecting, 2 engine off, 3 live); a missing or stale reading
is `null`. A reader refuses another `v`. The yellow deck doesn't read it yet (its heap); the 3rd
deck reads it, or owns its own link by compiling `obdlink` like the black deck. Either way the HUD
draws from the same `obdwire::Live`, so a deck that shows gauges doesn't care where they came from.

### 4. Which values

The ones on the screen, all mode 01 plus the adapter's own voltage:

| Value | Request | City note | Pace |
|---|---|---|---|
| RPM | 010C | | fast (every ~3rd request) |
| Speed | 010D | | fast |
| Throttle | 0111 | | fast extra (1 in 3 with pedal, air) |
| Coolant | 0105, else 0167 | the City has only 67 | slow (~every 22 s) |
| Intake air | 010F, else 0168 | only 68 | slow |
| Engine load | 0104 | | slow |
| Battery | ATRV | Honda smart charging idles 12.3-12.8 V | every 4 s with the engine on |

`obdplan` keeps its order (it also feeds the OBD deck's fuel and health checks, and asks 03 / 07
every 5 min, read-only). The HUD greys a value out once `obdplan::staleAfterMs` passes (3 s fast,
30 s slow).

### 5. The screen: CITY DATALINK

An original layout for the 320x240 panel, dark, built from blocks and lines (no copied gauge art):

```
+---------------------------------------------------------------+
| CITY DATALINK            [SHIFT]                     LINK LIVE|  header, 16 px
|  ▁▁▂▂▃▃▄▄▅▅▆▆▇▇██████   (24 slanted blocks, rising)            |  RPM ramp, ~56 px
|  0   1   2   3   4   5   6   7        x1000 RPM   2450        |  scale + digits
|                                                               |
|   88  88  88        km/h          THR [#####.....] 18%        |  speed: 7-segment digits
|                                   LOAD[########..] 41%        |
| +---------+ +---------+ +---------+                           |
| |COOL  88C| |IAT   34C| |BATT 14.1V|                         |  tiles, inverted on a warning
+---------------------------------------------------------------+
```

- **RPM ramp**: 24 slanted blocks that grow taller to the right; green below 5000, amber to
  the redline (6500, check it on the City's own tach), red past it. A peak marker stays on the
  highest block reached for 1.5 s, then falls one block every 60 ms.
- **Shift light**: from 85 % of the redline (`obdplan::rpmWarn`), the header's SHIFT tag blinks.
- **Speed**: the TFT's 7-segment font (`LOAD_FONT7`, already on for the camera sign), 3 digits.
- **Bars**: throttle and load, 10 cells each.
- **Tiles**: coolant (warning over 105 C), intake, battery (warning under 12.0 V running, 12.2 V
  off: `obdplan::batteryWarn`). A warning inverts the tile.
- **Link tag**: OFF / SEARCH / KEY ON / LIVE. Not live: values go grey, the ramp empties.
- Redraws only what changed (each block, number and tile keeps what it last drew), at most 10
  times a second.

The pure parts (`util/car/gaugehud`: blocks lit, zones, peak hold, shift blink, number text,
warnings) are host-tested; the drawing (`black/ui/gaugeview`) only follows them.

### 5b. Street look (2026-11-18, after the mocks)

João picked mock D from the four mocks (refined, neon tuner, mono OLED, merged): the same readings
in an early-2000s street-racing look, with a character on the screen the way the in-dash monitors
of that era had one. It takes from two things he saved: the Civic trip computer with an anime
drawing beside its numbers, and a fan remake of the lap-time monitor in a 2003 street-racing film
(name logo, waveform box, stacked chrome bars, a big angry character). Everything is drawn from
scratch; no art or layout is copied.

```
+---------------------------------------------------------------+
| CITY DATALINK ///  [SHIFT]  10:39                        LIVE |  header, clock added
|  ▁▁▂▂▃▃▄▄▅▅▆▆▇▇██████   cyan / pink past 5000 / red past 6500 |  RPM ramp, as before
|  0   1   2   3   4   5   6   7                                |
| 8 8 8  RPM 2450   | +--------------------------------------+ |
|        KM/H       | | ||||||||||||  RPM, last 12 s         | |  street panel (sprite)
| THR #####  LD ### | |      the navigator: twin tails,       | |
| ( 91 C    COOLANT)| |      headset, face by the engine      | |
|  ( 36 C   INTAKE) | |  stripes run with the speed           | |
|   (14.1 V BATTERY)| +--------------------------------------+ |
+---------------------------------------------------------------+
```

- **Palette**: deep violet ground; cyan, pink and red for the ramp; the speed in pink segments.
- **Pills**: coolant, intake, battery as chrome pills stepping right as they go down (the film's
  stage bars); a warning turns the pill red.
- **Street panel** (`util/car/streetface`, host-tested; drawn in a 140 x 144 PSRAM sprite pushed every
  frame): red with pink 45-degree stripes that scroll at 1.2 px/s per km/h; a trace box with the RPM
  of the last 12 s (32 bars, idle 500 to 7000) as mirrored bars; and the navigator, an original chibi
  with pink twin tails and a racing headset. Her face follows the link and the engine: glancing
  around while no adapter answers, asleep (Zz) at KEY ON, a small smile under 2500 rpm, focused
  brows to 5000, then angry brows and gritted teeth. She blinks every 3.7 s; the headset light
  flashes. With SHIFT, the panel's frame flashes pink too.
- Throttle and load lost their percentages (10 cells each still show them) to make room.

### 6. When it shows

A GAUGES switch, kept in NVS (`hud`/`on`): on, the black deck shows the HUD instead of the music
screens; off, it never does. Set from the phone page's GAUGES button or `GET /gauges?on=1|0`
(signed or `k=`). It stays on across power cycles, so João turns it on once and the deck starts on
the gauges in the car. The camera band and update screen still draw over it. A yellow-deck menu
item for it is phase 2 (needs the yellow heap checked).

## Measured on the desk (2026-11-18, black deck on COM12, no adapter)

| Build | Internal heap after the link starts (free / largest block) |
|---|---|
| before (no Bluetooth), at the boot scene's end | 172 KB / 147 KB |
| NimBLE, 3 connections, host in internal RAM | 95 KB / 86 KB |
| **NimBLE, 1 connection, host in PSRAM (this PR)** | **107 KB / 98 KB** |

The boot, WiFi, the Spotify login and polls, the camera service and music mode ran as before; the
link scanned every 5-60 s (`[obd] no adapter: next try in N s`). `GAUGES DEMO` drew the HUD from
the simulated drive: SEARCH, KEY ON with dashes, then LIVE with values (`tools/deck_shot.py --black`).

## Testing

- Host tests, written first: `test_obdwire` (encode / decode, nulls, versions), `test_gaugehud`
  (the HUD's maths) and `test_obdfake`: a fake Vgate in C++ that answers like the City does
  (echo before ATE0, `SEARCHING...` on the first PID, two ECUs on 0100, NO DATA for 05 / 0F, 67 /
  68 instead, ignition-off `NO DATA`, a cut reply) driven through `obdplan` + `obd::parse` until
  the HUD's seven values arrive, then the engine stops and the link reads KEY ON.
- On the desk: the black deck boots with the link on, scans, logs `[obd] no adapter`, the
  `[mem]` line shows the internal heap with WiFi, Spotify and NimBLE up, and the HUD draws with
  OFF and dashes.
- In the car: João's steps (PR body and the yellow-deck doc's checklist).

## Car test (João)

1. Phone: open the deck's page (the `?k=` link), tap GAUGES (it turns GAUGES ON). The black deck
   shows CITY DATALINK with OFF or SEARCH.
2. Vgate in the OBD port, ignition on, the iPhone's Bluetooth off (the adapter takes one link).
   Within ~30 s the tag goes SEARCH, then KEY ON (battery shows), then LIVE once the engine runs.
3. Compare with the car's own gauges: RPM at idle and at 3000, speed at 40 and 80 km/h (within
   2-3 km/h), coolant once warm (~85-95 C). Rev past 5500: SHIFT blinks.
4. Engine off: KEY ON, values go to dashes within 3 s (RPM, speed), the battery stays.
5. Music: Spotify keeps playing; GAUGES again turns the HUD off, the song screen comes back.
6. If it never goes past SEARCH: a USB log of the black deck (`[obd] seen ...`, `[obd] gatt ...`
   lines) says what the Vgate advertises; send it over.

## Later

- Phase 2: GAUGES on the yellow deck's Menu and pad, after a heap check there.
- Load as a fast value, if 22 s feels slow on the screen.
- The 3rd deck (JC4827W543, 480x272): the same `gaugehud`, a wider layout.
