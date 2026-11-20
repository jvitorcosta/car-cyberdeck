# Polling budget and latency over BLE

**Nothing here is measured on the City with the Vgate yet.** The first run should log the
round-trip of `010C` (send → `>`) over a minute of driving; put the number in this file.

## Where the time goes (one mode 01 request)

1. BLE write to the adapter: one connection interval (7.5-50 ms; NimBLE's default for a central
   is ~30-50 ms unless the deck asks for a shorter one).
2. The adapter sends `18DB33F1 02 01 0C` (or `7DF`) on CAN; ECUs answer within P2 = 50 ms, usually
   a few ms.
3. **The ELM waits its timeout after the last reply** to be sure no other ECU answers: default
   `ATST 32` ≈ 200 ms, cut down by adaptive timing (`ATAT1`, default) to roughly the slowest ECU's
   delay plus margin. With three ECUs answering (the City), this wait is most of the cost.
4. The reply comes back as notifications of ≤ 20 bytes each (ATT MTU 23 unless negotiated up),
   one per connection interval or several per interval, depending on the adapter.

## Realistic rates

| Setup | Requests / s | Source |
|---|---|---|
| ELM327 datasheet, single PID, default timing | ~6 | [S2] |
| Same, with the reply-count digit (`010C1`) | ~10-12 | [S2] |
| Bluetooth ELM clones, forum reports | ~15-20 PIDs/s at best (with multi-PID / tricks) | [S10] MEDIUM |
| OBDLink MX (STN chip), CAN | ~50 | [S10] MEDIUM |
| The old clone on the City (2026-10-19) | some replies > 1 s late | VERIFIED (repo) |

Plan for **4-8 requests per second** on the Vgate over BLE until measured. A clone stalling for
1-2 s now and then is normal: the deck's 2 s `ASK_MS` and the late-reply drain cover it.

## The deck's plan (`src/util/car/obdplan.cpp`, `src/obd/obdlink.cpp`)

- Requests are strictly sequential: one in flight, read to `>`, then the next (ELM rule: a byte
  sent mid-reply aborts it with `STOPPED`).
- **Fast set**, round-robin: RPM, speed, then one extra in turn: pedal D/E, MAF/MAP, throttle, MAF/MAP. The air is
  asked twice a round because `fueleco::Trip` counts the time between two air readings as burnt fuel and road
  covered (gaps up to `MAX_STEP_MS` = 5 s count whole). So at N req/s, RPM and speed each refresh at ~N/3 Hz
  (≈ 2 Hz at 6 req/s) and the air about every 6 requests (2 s at 250 ms a request; host model, 2026-11-20).
- **Coolant** (67 on the City) has its own turn every `COOLANT_MS` = 6 s, drawn stale after 20 s.
- **Load** (04) has its own turn every `LOAD_MS` = 3 s, drawn stale after 9 s: it moves with the pedal and is on the Cockpit.
- **Slow set**: one slow value every `SLOW_MS` = 2 s (battery, fuel, intake 68, ethanol, fuel system,
  trims, module voltage, status). 9 slots → each ≈ every 18 s.
- Battery (`ATRV`) also every 4 s with the engine running; 9 in 10 requests while cranking.
- Codes (03 + 07): once after the first live reply, then every 5 min.
- Unsupported PIDs (from the 0100.. scan) are never asked; a `NO DATA` backs off 30 s.
- Staleness: fast values are drawn stale after 3 s, the coolant after 20 s, the load after 9 s, the other slow ones after 45 s (`staleAfterMs`).

## Ways to go faster (each one is a gate change with tests, and a car test)

1. **Reply count digit** (`010C1`): the biggest win, but the City has three responders; only
   safe for PIDs only the engine ECU answers. Measure first.
2. **Physical addressing** to the engine only (`ATCP 18`, `ATSH DA10F1`, `ATCRA 18DAF110`): one
   responder, so the ELM stops waiting sooner. Mode 01 still works physically.
3. **Multi-PID** (`010C0D11` up to 6 PIDs): one round trip, multi-frame reply; `obd::parse`
   would need a multi-PID walker.
4. **`ATAT2`** (aggressive adaptive timing): risk of missing a slow ECU's reply.
5. **Shorter BLE connection interval** (request 7.5-15 ms after connect).
6. `ATSP7` instead of `ATSP0` once `ATDPN` is known: saves the protocol search at connect only.

Add one at a time and log the rate before and after. Never trade correctness for speed: a
missed or shifted reply is worse than a slower gauge.

## Sources

[S2] ELM327 datasheet. [S10] Torque-BHP forum "PIDs/second". See `sources.md`.
