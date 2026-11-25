# What the Honda City 2022 answers, and the Vgate

Confidence tags: **VERIFIED** = seen on João's car; **HIGH** = real captures from other Cities
or a primary document; **MEDIUM** = one good source or a close sibling car; **LOW** = forum
claim or guess; **UNKNOWN** = nobody has looked yet.

## The car

Honda City Touring 2022, 7th gen (GN), Brazil: 1.5 i-VTEC DOHC (displacement 1.497 L), CVT,
flex fuel (pump gasoline is E30; ethanol up to E100). Tank 44 L.

## Verified on the car (2026-10-19, old dual-mode clone, ATSP0, headers off)

- **Three ECUs** answer a functional mode 01 request (three lines). VERIFIED
- **Supported mode 01:** 01 03 04 06 07 0B 0C 0D 0E 10 11 13 15 1C 1F 21 23 2E 30 31 33 34
  41 42 43 44 45 46 47 49 4A 4C 52 56 67 68 6C. VERIFIED (`docs/superpowers/archive/specs/P26-10-28-car-health-check-design.md`; the raw 0100..0160 hex was not kept)
- **Not supported:** 05 coolant and 0F intake temp (use **67 / 68**), **2F fuel level**, **5E
  fuel rate**, 5C oil temp. VERIFIED
- **Ethanol % (52)** is there: the fuel-economy maths uses it (else E30). VERIFIED in the list;
  whether its value tracks the pump blend is UNKNOWN.
- **Battery:** Honda smart charging idles at ~12.3-12.8 V with the engine running, so the
  "battery low while running" warning is < 12.0 V (`obdplan::batteryWarn`). VERIFIED
- **Clone latency:** some replies came after > 1 s; the deck waits 2 s and drains a late reply
  before the next ask (else every value drifts one request behind). VERIFIED
- The gauge cluster read RPM / speed / temperature / volts in the car ("it worked"); the numbers
  were never compared one by one with the car's own. VERIFIED only as "it runs".

## Protocol: 29-bit, not 11-bit (web, HIGH for other Cities)

OBDb's captures of a 2014 and a 2023 Honda City show requests on **29-bit CAN**: functional
`18DB33F1`, physical `18DAxxF1`, replies from `18DAF1xx` (ELM protocol **7**, ISO 15765-4 CAN
29-bit 500 kbit/s). The engine ECU is **0x10**; a functional `010C` got replies from 0x10, 0x1E
(transmission) and 0xEF — three ECUs, matching what João's car did. WiCAN's Honda profiles also
use `ATSP7`. [S4][S5][S6]

- For João's 2022: **HIGH that it is 29-bit too, UNKNOWN until `ATDPN` is logged** (expect `A7`).
- The deck runs `ATSP0` (auto) and `ATH0`, so its mode 01 / 03 / 07 parsing doesn't care which
  — that is why the old clone worked without anyone noticing.
- **Consequences if you add anything header-based:** physical addressing is `ATCP 18` + `ATSH
  DA10F1` (engine) / `DA1EF1` (transmission), not `ATSH 7E0`; a reply filter is `ATCRA
  18DAF110`. With ATH1 the lines start `18DAF110 ...`, which `obd::anchored()` (7Ex only) does
  not understand — another reason ATH1 stays off. car-can-lab's adapter check refuses protocols
  7 and 9 as "29-bit": on this car that would refuse the right answer (a follow-up for that repo).
- The 2023 capture also supports 019A / 015B (hybrid), so it was probably an e:HEV; byte layouts
  for the engine ECU may still differ on the 1.5 flex. MEDIUM.

## Honda Mode 22 (UDS read data by identifier), from OBDb [S4]

Notation: "DA60 `22 7029`" = `ATCP 18`, `ATSH DA60F1`, send `227029`, reply `62 70 29 ...`
from `18DAF160`. Byte N counts from the first byte after `62 <DID>`. **None of these is in the
deck's gate; adding one follows SKILL.md hard rule 4** (exact request, exact header, a test).
Several replies are long (multi-frame; OBDb marks some "force flow control": `fcm1`).

| Signal | ECU / request | Decode | Confidence for the 2022 City |
|---|---|---|---|
| **Fuel level, litres** | DA60 (meter) `22 7029` | bytes 12-13 (16-bit) / 100 | **HIGH-MEDIUM**: captured on City 2014 and 2023 and Civic / Fit / HR-V; OBDb leaves 2015-2022 unconfirmed. The best fix for the missing PID 2F. |
| Odometer, km | DA60 `22 7022` | bytes 6-8 (24-bit) | MEDIUM (City 2023) |
| Odometer + run time | DA10 (engine) `22 2660` (fcm1) | bytes 43-45 km, 46-47 s | MEDIUM (City 2023) |
| Outside air temp | DA60 `22 7028` (fcm1) | 12-bit signed at bit 132 | MEDIUM (City 2023); PID 46 already gives ambient |
| VTEC switch / solenoid | DA10 `22 2612` | bits 174 / 175 | MEDIUM (City 2023) |
| Knock control / VTC duty | DA10 `22 2662` | byte 10 ×2/255; byte 28 ×100/255 | MEDIUM (City 2023) |
| Misfires, cyl 1-4 | DA10 `22 2663` | 16-bit at bytes 34/36/38/40 | MEDIUM (City 2023) |
| Brake switch / pressure | DA28 `22 4004` / `4005` | bit 127 / bytes 7-8 − 25 bar | MEDIUM (City 2023) |
| CVT fluid temp | DA1E (transmission) `22 2301` | byte 26 − 40 °C | LOW-MEDIUM (Civic 2016-22, not City) |
| Gear P/R/N/D | DA1E `22 3086` | byte 20 | LOW (OBDb lists it unsupported on the 2023 City) |
| Oil life / maintenance | DA60 `22 7060` | 16-bit values | LOW (unsupported on the 2023 City) |
| Oil temp | DA10 `22 2666` | byte 11 − 40 | LOW (unsupported on the 2023 City) |
| CVT temp "Torque" PID | 7E1 `22 1017` | (A×9/5)−58 °F | LOW (forum guess, 11-bit header: wrong bus format for this car) |

**Firmware (UNVERIFIED on the car):** the black deck now reads the fuel-level row itself (`util/car/fuel22`,
`obdlink::readFuelLevel`): `ATSH DA60F1`, `227029`, `ATSH DB33F1`, only after ATDPN says protocol 7, first 15 s
after the link is live, then once a minute, given up after 3 misses in a row. It reads the 16 bits at bytes 12-13
after `62 70 29` / 100 and, while fresh (5 min), the range becomes litres x km/L (`fuelbook::rangeFromLevel`),
winning over the FULL TANK book. The byte offset is a reading of OBDb's `bix 96` and may be wrong: the whole raw
reply is in the log as `[obd] 227029 -> "..."` (200 characters) and the value as `[obd] fuel level N L`. First
car check: compare that number with the cluster's fuel bars; if the offset is off, only `fuel22::parse` changes.
The ELM's default priority (ATCP 18) is relied on, not sent.

The fuel-level row has a PC probe: `python tools/obd_probe.py <adapter> --fuel` (off by default, never in
the decks' gate). It sends `ATCP 18`, `ATSH DA60F1`, `227029`, `ATZ` only when `ATDPN` says 29-bit, prints the
raw reply and both readings of OBDb's bit offset (bytes 12-13 after, or counting, the `62 70 29` header).
Record the result here as VERIFIED or "no answer" before any firmware change.

Before any of these goes into firmware: try it from the PC with `tools/obd_probe.py`-style
read-only code (with the exact request added to its list), log the raw reply, compare with the
car's own display (fuel gauge bars, odometer), and record the result here as VERIFIED.

## The adapter: Vgate iCar Pro 2S (arrived 2026-11-17)

| Fact | Status |
|---|---|
| Dual Bluetooth (BLE + Classic). The deck uses BLE (NimBLE central). | HIGH (product), radio choice is the repo's |
| GATT: Vgate iCar Pro BLE 4.0 uses service **18F0**, notify **2AF0**, write **2AF1**; one report says some units also have a 128-bit service `e7810a71-73ae-499d-8c15-faa9aef0c3f2` | MEDIUM [S7][S8]; LOW for the 128-bit one. `obdble::pick` tries FFF0, FFE0, 18F0, then any. UNKNOWN for the 2S until `[obd] gatt` lines are logged |
| Advertised name(s); two entries ("iOS" / "Android") on some units | UNKNOWN for the 2S; `looksLikeAdapter` matches OBD / ELM / VLINK — **not "Vgate" or "iCar"** (obd_probe.py does). Check the first `[obd] seen` line. |
| `ATZ` / `ATI` answers `ELM327 v2.2` (iCar Pro, Clarity guide); marketing says "2.3" | HIGH for the iCar Pro [S9]; UNKNOWN for the 2S |
| Real chip (ELM clone vs STN) | UNKNOWN (no source) |
| **Auto-sleep after ~30 min without CAN traffic, even with the engine running**; wakes on CAN traffic | HIGH [S9]. Sleep can be turned off with `ATPP` — **never from the deck** (stored setting, hard rule 3) |
| Handles long multi-frame replies better than cheap clones | MEDIUM [S9] |
| ATRV accuracy | UNKNOWN: compare with PID 42 on the first run |
| Shares the port with a courtesy tracker via a Y splitter | VERIFIED (repo spec) — odd replies may be the tracker's traffic |

## First Vgate run: what to log (then update this file)

`[obd] seen` (name), `[obd] gatt` (layout), `ATI`, `ATDPN` (expect A7), `ATRV` vs `0142`, the raw
`0100`/`0120`/`0140`/`0160` lines, one `0100` with ATH1 to learn the three ECU addresses (neither tool allows ATH1 today: a
deliberate, reviewed change to the PC probe, never the deck), and the round-trip time of `010C` (polling-budget.md).
