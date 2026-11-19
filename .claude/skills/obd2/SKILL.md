---
name: obd2
description: Use for any OBD-II work on the car decks — the Vgate / ELM327 link (src/obd/obdlink.cpp), the request parser and its safety gate (src/util/car/obd.*), the polling plan (obdplan), new gauges or readings, trouble codes, Mode 22 / Honda extended PIDs, reading a deck's [obd] log, or tools/obd_probe.py / obd_sim.py / obd2/decode.py. Holds the hard safety rules (read-only, never clear codes), what the Honda City 2022 actually answers, the ELM327 init sequence, PID formulas, the CAN/ISO-TP details and the realistic polling budget over BLE.
---

# OBD-II on the car decks

The car is a **Honda City Touring 2022 (Brazil, 1.5 i-VTEC, CVT, flex fuel)**. The adapter is a
**Vgate iCar Pro 2S** (arrived 2026-11-17, not yet run in the car), read over **BLE GATT** by the
black deck (`-D OBD_LINK`, NimBLE, one connection) and by the OBD deck (`env:obddeck`). The older
dual-mode clone ("OBDII" Classic / "OBDBLE" BLE) is the only adapter that has read this car so far
(2026-10-19). It answered mode 01 from **three ECUs** (verified). The bus is ISO 15765-4 CAN 500 kbit/s,
very likely with **29-bit IDs** (ELM protocol 7: `18DB33F1` / `18DAF1xx`, as OBDb's captures of
other Cities show), not the 11-bit `7DF`/`7E8` most guides assume. No `ATDPN` answer from this car
is logged yet: record it on the first Vgate run. The deck's `ATSP0` + `ATH0` works either way.

## Hard rules (never break these)

1. **Read-only, always.** The car only ever gets mode 01 (one PID), 03, 07 (and, if added, 0A / 09)
   plus the adapter's own AT setup. `obd::allowed()` in `src/util/car/obd.cpp` is the one gate:
   every write goes through `send()` in `src/obd/obdlink.cpp`, which calls it. Never add a path
   around it. Never widen it to 04, 08, 10, 11, 14, 27, 2E, 2F, 31, 34-37, 3B, 3E or 85.
2. **Never clear codes (mode 04 / UDS 14)** — not in firmware, not in a probe, not "just once".
   Clearing erases the freeze frame and resets every readiness monitor. Only João does that, with
   his own scanner, after he decides to.
3. **No writes to the bus.** No ATMA/monitor in deck firmware (car-can-lab's sniffer is the place
   for that, with its own gate), no ATSH to other ECUs' write services, no ATCSM0, no ATBRD/ATPP
   (those change the adapter's stored settings).
4. **Mode 22 is a new gate entry, not a free-for-all.** If a Honda extended PID is added, allow
   that exact `22xxxx` request (and the exact `ATCP`/`ATSH` it needs: 29-bit, e.g. `ATSH DA60F1`) and nothing else; a test must prove
   the neighbouring IDs stay refused. See `references/honda-city-2022.md` for what is known.
5. **ATH1 stays off** in the deck (`obd::parse` anchors assume ATH0; it tolerates headers but the
   setup never turns them on). ATS0 / ATE0 / ATL0 stay on in setup.
6. **Treat every reply as untrusted.** A tracker shares the port through a Y splitter; clones
   answer late (>1 s). Out-of-range, cut-off or stale values become `NAN`, never a wrong number.
7. **No personal data in the repo** (it may be public): no VIN (mode 09 02), no plate, no GPS
   home location in logs, tests or docs. Redact a VIN to `***` if a log line carries one.
8. **Car tests are João's.** Nothing OBD merges as "car-verified" until he has driven with it.

## Checklist: adding a new reading

1. Check it is in the City's supported list (`references/honda-city-2022.md`). If not, it is a
   Mode 22 question: stop and write down the source and confidence first.
2. Add the `Pid` enum value, its `DEFS` row (request, PID byte, data bytes) and its formula in
   `value()` (`src/util/car/obd.cpp`). Formula from `references/pids.md`; mirror it in
   `tools/obd2/decode.py` `PIDS`.
3. Add it to `PID_BYTE` and to the fast or slow set in `src/util/car/obdplan.cpp`; set its
   staleness in `staleAfterMs`. Check the budget (`references/polling-budget.md`): every fast PID
   slows RPM/speed.
4. If it has a request string that is new, make sure `obd::allowed()` accepts exactly it.
5. Host tests: `test/test_obd` (parse, with and without headers, several ECUs, NO DATA, cut-off
   reply), `test/test_obdplan` (order, back-off). Add a reply to `tools/obd_sim.py` so the bench
   sim answers it.
6. Wire it: `obdwire` (JSON slot) if other decks need it, the view that draws it.
7. Run `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 test_obd test_obdplan
   test_obdwire` and `python -m unittest discover -s tools -p "test_*.py"`, then the build.
8. Write the car check for João: what to compare against (the car's own gauge / trip computer).

## Debugging a link or a reading

- Decode a deck log offline: `python tools/obd2/decode.py --log capture.txt` (every
  `[obd] <cmd> -> "<raw>"` line), or one reply: `python tools/obd2/decode.py "7E8 03 41 0D 2A"`.
- Probe an adapter from the PC (read-only list): `python tools/obd_probe.py [name|address]`.
- Bench without the car: `tools/obd_sim.py` (an ELM327 stand-in over BLE).

## References

- `references/elm327.md` — AT commands, the deck's init sequence, timing, the Vgate's specifics.
- `references/pids.md` — modes, standard mode 01 PIDs with formulas, mode 09.
- `references/can-isotp.md` — ISO 15765-4: IDs, frames, multi-frame as the ELM prints it.
- `references/honda-city-2022.md` — what this car answers (verified) and Honda Mode 22 (web, with
  confidence).
- `references/polling-budget.md` — latency and PIDs per second over BLE; the plan's cadence.
- `references/dtc.md` — modes 03 / 07 / 0A, decoding, why clearing is João's call.
- `references/sources.md` — every source, and which facts are verified on the car.
