# ISO 15765-4: OBD on CAN, 500 kbit/s

ELM protocol **6** = 11-bit IDs, **7** = 29-bit IDs (`ATDPN` prints `A6` / `A7` when the auto
search found it). **Hondas, the City included, answer on 29-bit** in every public capture
(honda-city-2022.md): expect `A7`, confirm on the first Vgate run.

## 29-bit IDs (the City)

| ID | Direction | Meaning |
|---|---|---|
| `18DB33F1` | tester → all | functional request |
| `18DAxxF1` | tester → ECU xx | physical (City: 10 engine, 1E transmission, 60 meter, 28 brakes) |
| `18DAF1xx` | ECU xx → tester | reply |

ELM: `ATCP 18` sets the priority byte, `ATSH DA10F1` the rest; `ATCRA 18DAF110` filters.

## 11-bit IDs (most other cars, and most guides)

| ID | Direction | Meaning |
|---|---|---|
| `7DF` | tester → all | functional broadcast (the ELM's default header for OBD requests) |
| `7E0`-`7E7` | tester → one ECU | physical request to ECU #0-7 (7E0 engine, 7E1 transmission on most cars) |
| `7E8`-`7EF` | ECU → tester | reply = request ID + 8 (7E8 engine, 7E9 transmission, ...) |

The City answers with **three** ECUs (2026-10-19 log, headers off, so which IDs is unknown; OBDb's
2023 City: 10, 1E, EF). With `ATH1` 11-bit lines start `7E8`, `7E9`...; 29-bit ones `18DAF110`...; the deck keeps ATH0 and ORs / sums across lines.
To ask one ECU only: `ATSH 7E0` (+ `ATCRA 7E8` to filter replies). **Not in the deck's gate**
today; adding it is a gate change with a test (SKILL.md rule 4).

## ISO-TP frames (ISO 15765-2)

Every CAN frame is 8 data bytes (padded, usually `00` or `55`/`AA`). The first nibble is the
PCI (protocol control information):

| PCI | Frame | Layout |
|---|---|---|
| `0L` | Single frame | L = 1..7 payload bytes: `7E8 03 41 0D 2A` |
| `1L LL` | First frame | 12-bit total length, then 6 payload bytes |
| `30 BS ST` | Flow control (tester sends) | the ELM sends it automatically (`ATCFC1`, default) |
| `2N` | Consecutive frame | N = sequence 1..F, 0, 1...; 7 payload bytes each |

Timing: the tester's flow control must be `30 00 00` (ISO 15765-4); the ECU should answer
within P2 = 50 ms (P2* up to 5 s after a "response pending"
`7F xx 78`). Over a BLE clone the round trip is far longer (polling-budget.md).

## How the ELM prints a multi-frame reply (ATH0, ATCAF1)

```
00A
0: 43 04 01 33 03 01
1: 04 20 01 71 00 00 00
```

The first line is the total byte count in hex, then `0:`, `1:` ... `F:`, `0:` lines with the
payload (PCI bytes removed). With ATH1 you get the raw frames instead (`7E8 10 0A 43 04 ...` / `18DAF110 10 0A 43 ...`,
then `21 ...` frames). Replies that go multi-frame: mode 09 02 (VIN, 20 bytes — personal data, never
log), 09 04 / 06 / 0A, mode 03 / 07 / 0A with 3+ codes from one ECU, and a 6-PID mode 01 request.
`obd::parseCodes` joins `n:` lines; `tools/obd2/decode.py` too.

## Reply ordering and several ECUs

With the functional `7DF` request every ECU that supports the PID answers; the ELM prints them in
arrival order, not ID order. The ELM waits its timeout (`ATST`, default ~200 ms, adaptive with
`ATAT1`) after the last reply before the prompt, unless the request ends with a reply count
(`010C1`: "stop after 1 reply" — much faster, see polling-budget.md).

## Sources

[S2] ELM327 datasheet (protocol 6, ATH/ATCAF/ATCFC, multi-frame format, the count suffix).
[S3] ISO 15765-4:2005 (IDs, flow control, timing). [S4]-[S6] the 29-bit captures for Honda.
[S1] Wikipedia "OBD-II PIDs". See `sources.md`.
