# Trouble codes (DTCs)

## Modes

| Mode | Reply | What it holds |
|---|---|---|
| 03 | `43 <n> <2 bytes per code>` | **Stored / confirmed** codes: the ones that light (or lit) the MIL |
| 07 | `47 <n> ...` | **Pending**: seen once this or last drive cycle, not yet confirmed |
| 0A | `4A <n> ...` | **Permanent**: only the ECU clears them, after its own monitor passes; survive mode 04 and battery disconnect |
| 01 01 | `41 01 A B C D` | A bit 7 = MIL on, A bits 0-6 = stored-code count; B-D = readiness monitors |
| 02 | freeze frame | the PID snapshot when the first stored code set |

On CAN (ISO 15765-4) the count byte `<n>` follows the mode byte; on old K-line protocols it
does not (the deck only does CAN). With no codes the ECU sends `43 00` or the adapter prints
`NO DATA`. Each ECU answers on its own line (the City has three), and a long list arrives
multi-frame (`0:` / `1:` lines, see can-isotp.md). `obd::parseCodes` joins, de-duplicates and caps
at 4; `tools/obd2/decode.py` `codes()` does the same offline.

## Decoding two bytes `b0 b1`

- Letter = `b0 >> 6`: 0 P (powertrain), 1 C (chassis), 2 B (body), 3 U (network).
- Then `(b0 >> 4) & 3` (0 = SAE generic, 1 = manufacturer, 2-3 depend on letter),
  then `b0 & 0F`, `b1 >> 4`, `b1 & 0F` as hex digits.
- `01 33` → P0133, `C1 55` → U0155, `00 00` → padding (skip).

## Clearing (mode 04) — never from the deck

Mode 04 clears stored and pending codes **and** the freeze frame, mode 06 results, and resets
every readiness monitor (PID 01 B-D) and the "distance / warm-ups since cleared" counters (PIDs
30 / 31). The car then shows "not ready" until it completes full drive cycles, which can fail an
inspection and hides the history a mechanic would want. Permanent codes (0A) stay anyway, so a
clear doesn't even hide a real fault. Rule: **the deck never clears codes**, and an agent never
proposes code that does; João clears with his own scanner if he decides to (SKILL.md hard rule 2).

## What the deck does

`readCodes()` in `src/obd/obdlink.cpp`: 03 then 07 once the car answers live data, then every
5 min (`obdplan::CODES_MS`), a failed read retried after 30 s. `[obd] codes N P....` /
`[obd] pending N ...` in the log. "NO DATA" from a silent car is not "no codes": the read waits
for a live reply first.

## Sources

[S1] Wikipedia "OBD-II PIDs" (modes 03/07/0A, DTC byte layout). [S2] ELM327 datasheet (mode 03
formats, `NO DATA`). [S12] California BAR on permanent codes. [S13] Snap-on on mode 04's side
effects. The 30 / 31 counter reset is inferred from the J1979 PID definitions. See `sources.md`.
