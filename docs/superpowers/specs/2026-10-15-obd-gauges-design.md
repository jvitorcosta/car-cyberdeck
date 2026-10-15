# OBD-II gauges on the old deck (ELM327 over Bluetooth Classic)

**Date:** 2026-10-15
**Status:** draft for review
**Build:** `cyd_companion` only (the old deck). The new deck (`s3`) and the single-deck `cyd`
build don't change.
**Builds on:** `docs/superpowers/specs/2026-10-14-avatar-home-design.md` (the companion's screens,
her message bar, the theme).

## Goal

A temporary showcase: the old deck reads live engine data from the owner's ELM327 mini adapter
and shows it as Pokémon-style gauges while the car runs. Nothing else on the old deck changes:
without the adapter it is exactly today's Zero Two home screen.

## Decisions taken while brainstorming

- **Adapter:** the owner's "Scanner Bluetooth Automotivo" ELM327 mini: **Bluetooth Classic**
  (SPP), Android-only, pairing PIN 1234, 0000 or 8888. The car is a Honda City 2022 (Honda from
  2007 is listed as supported). Only the old deck's ESP32 can talk to it: the S3 has BLE only.
- **What it shows:** live gauges, read-only. No fault codes, no clearing, no demo mode.
- **Style:** the deck's FireRed/Emerald battle look: values as status boxes with HP-style bars.
- **Without the adapter:** the current home (Zero Two) as today. The gauges appear when the
  adapter answers and give the screen back to her when it stops.
- **Approach:** the ESP32 core's `BluetoothSerial` plus our own small ELM327 client, its
  request/reply logic a pure, host-tested `util/` module. Chosen over the ELMduino library (a new
  dependency, quirks with cheap v1.5/v2.1 clones, logic we can't test on the PC).

## Values

| Value | Request | Formula (A, B = reply bytes) | How often |
|---|---|---|---|
| RPM | `010C` | (256·A + B) / 4 | every cycle |
| Speed | `010D` | A km/h | every cycle |
| Coolant | `0105` | A − 40 °C | every ~2 s |
| Engine load | `0104` | A · 100 / 255 % | every ~2 s |
| Intake air | `010F` | A − 40 °C | every ~2 s |
| Throttle | `0111` | A · 100 / 255 % | every ~2 s |
| Battery | `ATRV` | the adapter's own reading ("12.6V") | every ~2 s |

A cheap clone answers in ~50–150 ms, so RPM and speed refresh about 4–5 times a second. A value
the car doesn't support ("NO DATA") is shown as `--` and asked again only every ~30 s.

## Link (`companion/obdlink`)

- **Its own FreeRTOS task:** a Bluetooth Classic connect blocks for seconds; the app's loop and
  the WiFi link to the new deck must not wait on it.
- **Connect:** `BluetoothSerial` as master; connect by name, trying `OBD_NAME` from `config.h`
  (default: "OBDII", then "OBD2", "V-LINK") with `OBD_PIN` (default "1234"). Not found: retry
  every 30 s. Bluetooth starts only in the `cyd_companion` build.
- **Init:** `ATZ` (reset), `ATE0` (no echo), `ATL0` (no linefeeds), `ATS0` (no spaces),
  `ATSP0` (automatic protocol). The first PID request may answer "SEARCHING..." for seconds.
- **Replies:** read up to the `>` prompt with a per-request timeout (1 s; 5 s for the first
  PID). Each reply goes through the pure parser.
- **State published under a lock:** `Off` (no adapter) / `Connecting` / `NoEngine` (adapter up,
  but the car doesn't answer: ignition off) / `Live`, plus the latest values and when each was
  read. The app copies a snapshot, the same pattern as `companion/link`.
- **Gone:** 3 failed requests in a row, or the Bluetooth link drops, counts as gone: state `Off`,
  reconnect cycle starts.

## Parser (`util/obd`, pure, host-tested)

- Builds requests (`"010C\r"`) and parses replies into values, tolerant of what clones send: a
  leftover echo, `SEARCHING...`, `BUS INIT: ...OK`, spaces or none, several lines, a trailing
  `>`, and the error words `NO DATA`, `UNABLE TO CONNECT`, `CAN ERROR`, `STOPPED`, `?`.
- Checks the reply's mode and PID (`41 0C` for `010C`) before reading bytes; anything else is
  an error, never a wrong number.
- `ATRV`: "12.6V" → 12.6.
- Also pure: which PID is due next (fast pair every cycle, slow ones in turn every ~2 s,
  unsupported ones every ~30 s) and the bar colour bands.

## Screen (`companion/gauges`)

320×240, theme palette (night palette included), the battle UI's boxes and font.

- **RPM:** a wide status box at the top, "RPM" as the name and the number, an HP-style bar to the
  redline (6500 for the City's 1.5 L; `OBD_REDLINE` in `config.h` to change): green, yellow from
  70 %, red from 90 %.
- **Speed:** the largest number on screen, "km/h" small beside it.
- **Four small boxes:** coolant (bar blue below 60 °C, green to 105 °C, red above), battery
  (red below 11.8 V, yellow below 12.2 V, else green; the engine running reads 13.5–14.5 V), load
  and throttle (plain bars).
- **Message bar:** her line strip at the bottom stays, so she can say "Car connected, darling!"
  when the gauges appear and "The car's asleep~" when they go.
- Redraws only the boxes whose value changed.

## Switching screens

- The app shows the gauges while the link reports `Live`, from any screen, and returns to the
  screen it left when the link leaves `Live` for 5 s (so a single missed reply doesn't flicker).
- `NoEngine` (adapter plugged in, ignition off) keeps Zero Two on screen; her bar says the car
  is asleep.
- The phone page doesn't change (a CAR button can come later).

## Coexistence and budgets

- WiFi stays on (lyrics from the new deck, the phone page). The ESP32 shares its radio between
  WiFi and Bluetooth (coexistence is built into the core).
- Bluetooth Classic adds roughly 500–700 KB of flash and 40–60 KB of RAM. The companion is at
  53.7 % flash, has no TLS, and stays under the 97 % flash budget; the plan measures free heap
  and the largest block before and after (`[mem]` lines).

## Failure handling

- No adapter, wrong PIN, adapter asleep: state `Off`, retry every 30 s, nothing on screen
  changes.
- Car doesn't answer (`UNABLE TO CONNECT`, ignition off): `NoEngine`, ask again every 5 s.
- A reply that doesn't parse: that value keeps its last reading and shows it dimmed after 3 s.
- Bluetooth stack can't start (no memory): logged once, the companion runs as today.

## Testing

- **Host tests (`test_obd`):** every formula at its ends (RPM 0 and 16383.75, coolant −40 and
  215 °C); replies with and without spaces, with echo, with `SEARCHING...`, split over lines, with
  the prompt; every error word; a reply for the wrong PID; a cut-off reply; `ATRV` forms; the PID
  schedule (fast pair each cycle, slow ones in turn, unsupported ones backed off); the colour
  bands at their edges.
- **Build:** `cyd_companion` builds and stays under 97 % flash; CI checks it.
- **On the car (needs the owner):** ignition on, the deck powered in the car: the log shows the
  connect, the init replies, values updating; the gauges follow the engine (idle ~700 rpm, a rev);
  unplugging the adapter brings Zero Two back. Heap measured with WiFi and Bluetooth both up.

## Out of scope (later)

Fault codes (read or clear), a demo mode, a CAR button on the phone page, logging trips, other
adapters (BLE, WiFi), the new deck.
