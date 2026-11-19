# OBD-II modes and standard PIDs

Formulas are SAE J1979 / ISO 15031-5 (the free summary is Wikipedia's "OBD-II PIDs" page, [S1]).
A, B, C, D are the data bytes after `41 <pid>`. **City** = the 2022 City listed it as supported on
2026-10-19 (verified on the car, old adapter). The deck's code is `src/util/car/obd.cpp`
(`value()`); the offline decoder is `tools/obd2/decode.py` (`PIDS`).

## Modes (services)

| Mode | What | Deck | Notes |
|---|---|---|---|
| 01 | Live data | yes | one PID per request on the deck; up to 6 per request on CAN (see polling-budget.md) |
| 02 | Freeze frame | no | `02 <pid> <frame>`; same formulas as 01 |
| 03 | Stored DTCs | yes | read-only; see dtc.md |
| 04 | **Clear DTCs + freeze frame, reset monitors** | **never** | refused by `obd::allowed()` |
| 05 | O2 monitor results | — | not used on CAN; replaced by mode 06 |
| 06 | On-board monitor results (misfire counts, cat) | not yet | untested on the City; read-only, allowed only if added to the gate |
| 07 | Pending DTCs (this / last drive cycle) | yes | |
| 08 | Control of on-board system (EVAP test) | **never** | actuates hardware |
| 09 | Vehicle info: 02 VIN, 04 CALID, 06 CVN, 0A ECU name | not yet | 09 02 is the VIN: **personal data**, never log or commit it |
| 0A | Permanent DTCs | not yet | read-only; survives mode 04 |
| 22 | (UDS / ISO 14229) read data by identifier | not yet | manufacturer PIDs; see honda-city-2022.md |

## Support bitmaps

`0100`, `0120`, `0140`, `0160`, `0180`, `01A0`, `01C0` each return 4 bytes: bit 31 (MSB of A) =
PID block+1 ... bit 0 (LSB of D) = PID block+0x20, which also says "the next block exists".
Each ECU answers its own line: OR them. `obd::parseSupported` / `isSupported`; decoder
`supported()`.

## Mode 01 PIDs

| PID | Name | Bytes | Formula | Unit | City |
|---|---|---|---|---|---|
| 01 | Monitor status since DTCs cleared | 4 | A7 = MIL on, A6..A0 = stored DTC count; B..D monitor ready bits | — | yes |
| 03 | Fuel system status | 2 | A = system 1 (1 open-cold, 2 closed, 4 open-decel/fuel cut, 8 open-fault, 16 closed-fault) | — | yes |
| 04 | Calculated engine load | 1 | A·100/255 | % | yes |
| 05 | Coolant temperature | 1 | A−40 | °C | **no** (use 67) |
| 06 / 07 | Short / long fuel trim bank 1 | 1 | (A−128)·100/128 | % | yes |
| 0B | Intake manifold pressure | 1 | A | kPa | yes |
| 0C | Engine RPM | 2 | (256A+B)/4 | rpm | yes |
| 0D | Vehicle speed | 1 | A | km/h | yes |
| 0E | Timing advance | 1 | A/2−64 | ° before TDC | yes |
| 0F | Intake air temperature | 1 | A−40 | °C | **no** (use 68) |
| 10 | MAF air flow | 2 | (256A+B)/100 | g/s | yes |
| 11 | Throttle position | 1 | A·100/255 | % | yes |
| 13 | O2 sensors present | 1 | bit map | — | yes |
| 15 | O2 bank 1 sensor 2 | 2 | A/200 V; (B−128)·100/128 % trim (B=FF: not used) | V | yes |
| 1C | OBD standard this vehicle meets | 1 | enum (J1979 table; the City's value is not logged yet) | — | yes |
| 1F | Run time since engine start | 2 | 256A+B | s | yes |
| 21 | Distance with MIL on | 2 | 256A+B | km | yes |
| 23 | Fuel rail pressure (gauge, direct inject) | 2 | 10·(256A+B) | kPa | yes |
| 2E | Commanded EVAP purge | 1 | A·100/255 | % | yes |
| 2F | Fuel tank level | 1 | A·100/255 | % | **no** |
| 30 | Warm-ups since codes cleared | 1 | A | — | yes |
| 31 | Distance since codes cleared | 2 | 256A+B | km | yes |
| 33 | Barometric pressure | 1 | A | kPa | yes |
| 34 | O2 sensor 1 (wide-band) λ and current | 4 | λ = (256A+B)·2/65536; mA = (256C+D)/256 − 128 | — | yes |
| 41 | Monitor status this drive cycle | 4 | bit map | — | yes |
| 42 | Control module voltage | 2 | (256A+B)/1000 | V | yes |
| 43 | Absolute load | 2 | (256A+B)·100/255 | % | yes |
| 44 | Commanded air-fuel equivalence (λ) | 2 | (256A+B)·2/65536 | — | yes |
| 45 | Relative throttle position | 1 | A·100/255 | % | yes |
| 46 | Ambient air temperature | 1 | A−40 | °C | yes |
| 47 | Absolute throttle position B | 1 | A·100/255 | % | yes |
| 49 / 4A | Accelerator pedal D / E | 1 | A·100/255 | % | yes |
| 4C | Commanded throttle actuator | 1 | A·100/255 | % | yes |
| 51 | Fuel type | 1 | enum (J1979 table) | — | **no** |
| 52 | Ethanol fuel % | 1 | A·100/255 | % | yes |
| 56 | Long term secondary O2 trim bank 1 | 1 | (A−128)·100/128 | % | yes |
| 5C | Engine oil temperature | 1 | A−40 | °C | **no** |
| 5E | Engine fuel rate | 2 | (256A+B)/20 | L/h | **no** |
| 67 | Engine coolant temperature (2 sensors) | 3 | A bit0 = sensor 1 present; B−40 (sensor 1), C−40 (sensor 2) | °C | yes |
| 68 | Intake air temperature (sensors) | 3 on most cars | A = present bits; B−40 | °C | yes |
| 6C | Commanded throttle actuator / position | 5 | bit map + values | % | yes |
| A4 | Transmission actual gear | 4 | (256C+D)/1000 ratio (J1979-2017) | — | unknown: blocks 0180+ never asked |
| A6 | Odometer | 4 | (A·2²⁴+B·2¹⁶+C·2⁸+D)/10 | km | unknown: blocks 0180+ never asked |

The deck reads 0x67 / 0x68 sensor 1 only and returns `NAN` when bit 0 of A is clear
(`obd::value`). PID 68's length differs by car (J1979: 3 or 7 bytes); the deck only needs B.

## Fuel without PID 2F / 5E

The City gives neither tank level nor fuel rate. The deck computes fuel from air mass
(`util/car/fueleco`): fuel g/s = MAF / AFR, with the AFR for the ethanol % from PID 52 (gasoline
14.7, E100 9.0, linear blend), or speed-density from MAP + RPM + IAT when MAF is missing
(displacement 1.497 L, VE 0.85). Fuel cut (PID 03 = 4, or closed pedal at speed) counts zero.
The tank level for range comes from the fuel book (FULL TANK on the phone page), not the car.

## Sources

[S1] https://en.wikipedia.org/wiki/OBD-II_PIDs — mode 01/09 tables and formulas (SAE J1979 summary).
Full list in `sources.md`.
