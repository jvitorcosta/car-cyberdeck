# PS3 Controller Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The yellow deck's DualShock 3 acts only on meant presses, puts the lights, music mode and karaoke one press away, steps colours with a right-stick flick, buzzes a distinct feel per action, and shows both levels on the BOTH page.

**Architecture:** Every rule is a PURE header in `src/util/yellow/` (or `src/util/lights/`) with a Unity host test, like `padtrigger` / `padfeel` today. `src/yellow/ledpanel.cpp` only wires them in: the Bluetooth task (`onPadReport`) feeds the guard and the flick; the loop (`handlePad`) maps the guarded bits to actions; `padhid` gets a two-motor rumble.

**Tech Stack:** C++17, Arduino-ESP32, esp32-ps3 (`jvpernis/PS3 Controller Host@1.1.0`), Unity host tests via `tools/run_tests.ps1`, PlatformIO env `ledpanel`.

**Spec:** `docs/superpowers/specs/2026-11-12-ps3-pad-redesign-design.md`

## Global Constraints

- Yellow deck heap: Bluedroid dual mode, ~30 KB low water. No new tasks, no buffers over a few hundred bytes, no `std::vector` / `String` in the BT callback.
- Pure logic goes in headers under `src/util/` with a host test in `test/test_<name>/test_<name>.cpp`; run with `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 <suites>`.
- Firmware must build: `pio run -e ledpanel`. Flash the yellow deck over USB only: `pio run -e ledpanel -t upload --upload-port <the CH340 port>` (COM16 or COM17; check Device Manager).
- Rumble and LEDs go out only through `padhid::send`.
- Commits: conventional `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Public repo: no plate, SSIDs, addresses.
- Comments match the file's style: plain sentences saying why, like the existing `pad*` headers.

## Review Focus

1. A quick deliberate tap that lands in a single report at full pressure (~50 ms at ~19-45 reports/s) still acts. Test: `test_a_full_press_in_one_report_counts` (Task 1).
2. A deliberate two-button press (L1+R1 together, X+D-pad) still acts: only 3+ bits block. Test: `test_two_buttons_at_once_still_count` (Task 1).
3. One stuck bit can't lock the pad: after it, a new press still acts. Test: `test_a_stuck_button_does_not_lock_the_others` (Task 1).
4. The guard forgets everything when the pad reconnects (a press held across a reconnect doesn't fire, the rest state restarts). Test: `test_reset_forgets_held_presses_and_rest` (Task 1).
5. A stick resting off-centre (drift up to ±20) or held at the edge never steps more than once. Test: `test_a_held_flick_steps_once` and `test_drift_never_flicks` (Task 3).

---

### Task 1: The guard (`padguard`)

**Files:**
- Create: `src/util/yellow/padguard.h`
- Create: `test/test_padguard/test_padguard.cpp`

**Interfaces:**
- Produces:
  - `namespace padguard` bit positions (the layout `onPadReport` uses today): `enum Bit : uint8_t { SELECT = 0, START = 3, UP = 4, RIGHT = 5, DOWN = 6, LEFT = 7, L2 = 8, R2 = 9, L1 = 10, R1 = 11, TRIANGLE = 12, CIRCLE = 13, CROSS = 14, SQUARE = 15, PS = 16, N_BITS = 17 };`
  - Constants: `PRESS_AT = 64, RELEASE_AT = 24` (pressure buttons), `TRIGGER_PRESS_AT = 200, TRIGGER_RELEASE_AT = 60`, `CHORD = 3`, `STUCK_MS = 5000`, `PAUSE_MS = 500`, `REST_MS = 1000`, `REST_BAND = 12` (sum of |Δx|+|Δy|+|Δz| in raw accelerometer units from the rest reference).
  - `struct Report { uint32_t bits; uint8_t pressure[N_BITS]; int16_t ax, ay, az; uint32_t now; };` — `bits`: the DS3's own on/off bits in the layout above (L2/R2 included); `pressure[b]`: that button's pressure, or 255 for START/SELECT/PS when their bit is on (they have none).
  - `class Guard { public: uint32_t feed(const Report& r); void reset(); bool resting() const; bool paused(uint32_t now) const; uint32_t stuck() const; };` — `feed` returns the bits that count as held right now; `paused` is true inside a handling pause; `stuck` is the bits dropped by rule 4 right now.

Rules `feed` applies, in this order (spec "The guard"):
1. Per button, held/not by pressure with hysteresis (`PRESS_AT`/`RELEASE_AT`; L2/R2 use the trigger pair; START/SELECT/PS follow their bit).
2. If `popcount(r.bits) >= CHORD` (raw bits, however light), no button that wasn't held in the last output may become held this report; held ones may still release.
3. Rest: the pad is resting after `REST_MS` within `REST_BAND` of a reference sample; leaving rest or entering it starts a `PAUSE_MS` window in which no new button becomes held.
4. A button held longer than `STUCK_MS` drops out of the output until its pressure goes under its release line.

- [ ] **Step 1: Write the failing tests** in `test/test_padguard/test_padguard.cpp` (Unity, same shape as `test/test_padtrigger`; a helper `Report rep(uint32_t now, std::initializer_list<std::pair<int,uint8_t>> pressed, int16_t az = -110)` that sets each listed bit and its pressure, and jitters nothing):
  - `test_a_light_touch_is_not_a_press`: X at 30 → bit off; X at 64 → on.
  - `test_a_press_releases_only_well_below_its_line`: X 200 → on; 30 → on; 23 → off.
  - `test_triggers_need_a_firm_squeeze`: R2 at 176 → off; 200 → on; 61 → on; 59 → off.
  - `test_a_full_press_in_one_report_counts`: one report X 255 → on, next report 0 → off.
  - `test_the_measured_set_down_does_nothing`: one report with X 21, O 27, SQUARE 15, TRIANGLE 26, R1 20, R2 1 (all six bits on) → output 0; same report with X raised to 255 → still 0 (rule 2).
  - `test_two_buttons_at_once_still_count`: L1 255 + R1 255 in one report → both on.
  - `test_a_held_button_survives_a_chord`: X 255 held; next report X 255 + O 30 + SQUARE 30 → X still on, O/SQUARE off.
  - `test_a_stuck_button_drops_out_after_five_seconds`: UP 200 held from t=0; at t=4999 on; at t=5000 off; stays off at 200; off→ under 24 → then 200 again → on.
  - `test_a_stuck_button_does_not_lock_the_others`: UP stuck past 5 s; X 255 then → X on.
  - `test_picking_it_up_pauses_presses`: 1100 ms of reports with az -110 ±2 (resting becomes true); then az jumps to -60 at t=1200 with X 255 → X off; X 255 at t=1700 → on.
  - `test_hand_motion_never_pauses`: reports alternating az -110 / -80 every 50 ms for 3 s (never rests) with X 255 at t=2000 → on.
  - `test_reset_forgets_held_presses_and_rest`: X held, resting true, `reset()` → `resting()` false and the next report with X 255 counts as a new press (held), output not carried over before it.
  - `test_millis_wrap`: STUCK and PAUSE timing across `now` wrapping from 0xFFFFFF00.
  - `test_paused_and_stuck_report_their_state`: `paused(now)` true inside the pause of `test_picking_it_up_pauses_presses`, false after; `stuck()` has UP's bit after 5 s held, 0 after its release.

- [ ] **Step 2: Run, expect failure**
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 test_padguard`
Expected: FAIL (padguard.h not found).

- [ ] **Step 3: Implement `padguard::Guard` in `src/util/yellow/padguard.h`** — fixed-size state only (per-bit held flag and held-since time, rest reference and since, pause-until); all time compares as `static_cast<int32_t>(a - b)`.

- [ ] **Step 4: Run, expect pass**
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 test_padguard`
Expected: PASS, 14 tests.

- [ ] **Step 5: Commit** (padtrigger stays until Task 2 removes its last user).

```bash
git add src/util/yellow/padguard.h test/test_padguard
git commit -m "feat(yellow): a pad guard that ignores set-downs, light touches and stuck buttons"
```

### Task 2: Wire the guard into the firmware

**Files:**
- Modify: `src/yellow/ledpanel.cpp:1611-1650` (`onPadReport`, its atomics), `:1652-1665` (`handlePad`'s connect change), `:1729-1736` (the press log)
- Delete: `src/util/yellow/padtrigger.h`, `test/test_padtrigger/`
- Modify: `CHANGELOG.md` (an Unreleased entry), `docs/yellow-deck.md:90-101` ("L2 and R2 count past half travel" → the guard)

**Interfaces:**
- Consumes: `padguard::Guard`, `padguard::Report`, `padguard::Bit` (Task 1).
- Produces: `onPadReport` stores the guard's output in `padHeld` / `padSeen` with the same bit layout as today, so `handlePad`'s mapping is unchanged in this task.

- [ ] **Step 1:** In `onPadReport`, build a `padguard::Report` from `Ps3.data.button`, `Ps3.data.analog.button.*` and `Ps3.data.sensor.accelerometer` (`now = millis()`), call a file-static `padguard::Guard padGuard` (BT task only, like `padL2` today), and use its result as `raw`. While `!padArmed`, call `padGuard.reset()` before feeding. Remove `padL2`/`padR2`/`padtrigger`; keep `padL2At`/`padR2At` for the log.
- [ ] **Step 2:** Log once per change of the guard's verdict: when `popcount(report.bits) >= CHORD` starts, `[pad] ignored: N buttons at once`; when `resting()` turns on/off, `[pad] resting` / `[pad] picked up`; when `stuck()` gains a bit, `[pad] stuck: <name>`. The BT task stores these in atomics; the loop prints them, like `padXNoBit` today.
- [ ] **Step 3:** Run all host tests and build.
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` then `pio run -e ledpanel`
Expected: all suites PASS (test_padtrigger gone); `[SUCCESS]`.
- [ ] **Step 4:** Commit.

```bash
git add -A src/yellow/ledpanel.cpp src/util/yellow test CHANGELOG.md docs/yellow-deck.md
git commit -m "feat(yellow): the pad's presses go through the guard"
```

### Task 3: The colour flick and colour-only change (pure)

**Files:**
- Create: `src/util/yellow/padflick.h`, `test/test_padflick/test_padflick.cpp`
- Modify: `src/util/yellow/bothlook.h`, `test/test_bothlook/test_bothlook.cpp`
- Modify: `src/util/lights/ledlook.h`, `test/test_ledlook/test_ledlook.cpp`

**Interfaces:**
- Produces:
  - `padflick::Flick { int feed(int8_t rx); void reset(); }` → `-1` / `+1` once per flick, else `0`. Constants `EDGE = 90` (~70 % of 127), `CENTRE = 30`: a flick fires when |rx| first reaches `EDGE`, and re-arms only after |rx| < `CENTRE`.
  - `ledlook::stepGrid(int swatch, int dir) -> int`: the next/previous of the 8 grid colours, wrapping; from `CYAN` → `0` (dir +1) or `N_GRID - 1` (dir -1).
  - `bothlook::recolour(int swatch, Look* both, Look* in, Look* out)`: sets the colour on all three; each keeps its own `pct` and `lit`.

- [ ] **Step 1: Failing tests**
  - `test_padflick`: `test_a_flick_right_steps_once` (0, 50, 95, 127 → 0,0,+1,0); `test_a_held_flick_steps_once` (127 ×20 → one +1); `test_it_rearms_at_centre` (127, 40, 127 → +1,0,0; then 10, 127 → 0,+1); `test_drift_never_flicks` (values in −20..20 → all 0); `test_left_is_minus_one`; `test_reset_rearms`.
  - `test_ledlook`: `test_step_grid_wraps` (YELLOW +1 → RED; RED −1 → YELLOW); `test_step_grid_from_cyan` (CYAN +1 → RED; CYAN −1 → YELLOW).
  - `test_bothlook`: `test_recolour_keeps_each_level` (in PINK 60 lit, out PINK 40 lit → recolour BLUE → in BLUE 60 lit, out BLUE 40 lit, both BLUE); `test_recolour_keeps_an_off_light_off` (out unlit stays unlit, takes BLUE).
- [ ] **Step 2:** Run `tools\run_tests.ps1 test_padflick test_ledlook test_bothlook` → FAIL (missing symbols).
- [ ] **Step 3:** Implement the three.
- [ ] **Step 4:** Same run → PASS.
- [ ] **Step 5:** Commit `feat(yellow): a right-stick flick and a colour-only change for both lights`.

### Task 4: The feel (two motors, a pattern per action)

**Files:**
- Modify: `src/util/yellow/padfeel.h`, `test/test_padfeel/test_padfeel.cpp`
- Modify: `src/yellow/padhid.h`, `src/yellow/padhid.cpp:52-73`
- Modify: `src/yellow/ledpanel.cpp:1551-1580` (`padSend`, `padFelt`, `padFeelTick` onto the new API; every existing buzz becomes `Feel::Tick, on = true` so behaviour is unchanged until Task 5)

**Interfaces:**
- Produces:
  - `padhid::send(uint8_t leds, uint32_t smallMs, uint32_t bigMs, uint8_t bigPower) -> bool` (replaces the 3-argument form): small motor on/off for `smallMs`, big motor at `bigPower` for `bigMs`, both in the DS3's ~20 ms units as today.
  - `padfeel::Pulse { uint16_t smallMs, bigMs; uint8_t bigPower; uint16_t gapMs; }` and `enum class Feel { Tick, Double, Long, Firm }`.
  - `padfeel::Buzz::start(Feel f, bool on, bool partial, uint32_t now)` and `bool due(uint32_t now, Pulse* out)`; `busy()` unchanged.
  - Starting shapes (spec "The feel"; builder tunes on the hand): `Tick` = small 40 ms; `Double` = small 40, gap 120, small 40; `Long` = big 400 ms at 160; `Firm` = big 150 ms at 255. `on == false` (turning something off) plays the same shape on the big motor at 90 in place of the small motor, and at half power for big pulses. `partial` (a light the action was for isn't linked) appends one big 200 ms pulse at 90 after a 160 ms gap.
  - `padfeel::buzzes` goes; `padfeel::leds` is unchanged.

- [ ] **Step 1: Failing tests** in `test_padfeel`: `test_tick_is_one_small_pulse`; `test_double_is_two_small_pulses_120_apart`; `test_long_is_one_big_pulse`; `test_off_moves_small_to_big_and_softens`; `test_partial_link_adds_a_weak_big_pulse` (Firm+partial → 2 pulses, the second big 200 at 90 starting 160 ms after the first ends); `test_a_new_start_replaces_a_running_one`; keep the LED tests and `millis` wrap test.
- [ ] **Step 2:** Run `tools\run_tests.ps1 test_padfeel` → FAIL.
- [ ] **Step 3:** Implement `padfeel`; change `padhid::send` (bytes 1-4 of the report: right/small duration+on, left/big duration+power).
- [ ] **Step 4:** Move `padSend` / `padFelt` / `padFeelTick` onto the new API (signature `padFelt(padfeel::Feel f, bool on, bool toIn, bool toOut)`; all current callers pass `Feel::Tick, true, true, true`). Run `test_padfeel` → PASS and `pio run -e ledpanel` → `[SUCCESS]`.
- [ ] **Step 5:** Commit `feat(yellow): a pad feel per action on its two motors`.

### Task 5: The new mapping in the firmware

**Files:**
- Modify: `src/yellow/ledpanel.cpp:1482-1492` (header comment), `:1551-1609` (`padSend`, `padFelt`, `padLightsPower`, `padLevel`, `padPreset`), `:1630-1650` (flick on the BT task), `:1738-1781` (the mapping)
- Modify: `CHANGELOG.md`, `docs/yellow-deck.md` (the mapping table)

**Interfaces:**
- Consumes: `padflick::Flick`, `ledlook::stepGrid`, `bothlook::recolour` (Task 3); `padfeel::Feel`, `Buzz::start`, `padhid::send` 4-arg (Task 4); guarded bits (Task 2).
- Consumes also: `padFelt(padfeel::Feel f, bool on, bool toIn, bool toOut)` (Task 4).
- Produces: `static void padColour(int dir)`, `static void padKaraoke()`.

- [ ] **Step 1:** Give each caller of `padFelt` its feel (below); karaoke and music mode pass `toIn = toOut = false` (no light, so never `partial`).
- [ ] **Step 2:** Mapping in `handlePad` (spec table):
  - L2/R2 → `padPreset` with `Feel::Tick`.
  - L1/R1 → `padLevel(±1)` with `Feel::Tick` on every screen; remove the look-page `L_MINUS`/`L_PLUS` presses.
  - START → `padLightsPower` with `Feel::Firm`, `on` = the new power.
  - SELECT → music mode with `Feel::Double`, `on` = new state.
  - Triangle → `padKaraoke()`: `setKaraoke(!karaokeOn)`, redraw the Menu's KARAOKE item if on the Menu, `Feel::Long`, `on` = new state; on every screen. Remove Triangle's page on/off.
  - Flick: `onPadReport` feeds `Ps3.data.analog.stick.rx` to a BT-task `padflick::Flick` (reset with the guard) and ORs ±1 into an `std::atomic<int8_t> padFlickDir` (last wins); `handlePad` takes it with `exchange(0)` and calls `padColour(dir)`: `ledlook::stepGrid` from `looks[INSIDE].swatch`, then `bothlook::recolour` on `looks[BOTH/INSIDE/OUTSIDE]`, end music mode via `musicYield()` like `change()`, push both lights, `save()`, `drawLooks()`, `Feel::Tick`. The BT task drops a flick while `padGuard.paused(now)`.
  - PS, D-pad, X, O, Square unchanged.
- [ ] **Step 3:** Update the header comment, `CHANGELOG.md`, `docs/yellow-deck.md` mapping table.
- [ ] **Step 4:** Run all tests and build.
Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` then `pio run -e ledpanel`
Expected: all PASS; `[SUCCESS]`.
- [ ] **Step 5:** Commit `feat(yellow)!: karaoke on Triangle, colours on the right stick, L1/R1 everywhere`.

### Task 6: Both levels on the BOTH page

**Files:**
- Modify: `src/util/yellow/bothlook.h`, `test/test_bothlook/test_bothlook.cpp`
- Modify: `src/yellow/ledpanel.cpp:767-788` (`drawLevel`), `:16-24` and `:161-163` (comments saying "MIX")

**Interfaces:**
- Produces: `bothlook::levelText(const ledlook::Look& in, const ledlook::Look& out, char* buf, size_t n)` → `"60%"` / `"OFF"` when they match, else `"IN 60%  OUT 40%"` with `OFF` for an unlit light (`"IN OFF  OUT 80%"`).

- [ ] **Step 1: Failing tests:** `test_level_text_matching` ("60%", "OFF"); `test_level_text_mixed` ("IN 60%  OUT 40%"); `test_level_text_one_off` ("IN OFF  OUT 80%"); `test_level_text_fits` (the longest, "IN 100%  OUT 100%", is < 20 chars and the call never overruns an `n` of 8: truncates, NUL-terminated).
- [ ] **Step 2:** Run `test_bothlook` → FAIL. **Step 3:** implement. **Step 4:** PASS.
- [ ] **Step 5:** `drawLevel`: when mixed, the bar fills to the inside light's level and the text is `levelText` in `Face::SMALL` (the 108 px `deckmenu::LEVEL` rect can't fit it in `TITLE`); matching lights keep `TITLE`. Build `pio run -e ledpanel` → `[SUCCESS]`.
- [ ] **Step 6:** Commit `feat(yellow): the BOTH page shows each light's level when they differ`.

### Task 7: Desk check on the yellow deck

**Files:** none (tuning changes go back into Tasks 1/3/4's constants with their tests updated).

- [ ] **Step 1:** Flash: `pio run -e ledpanel -t upload --upload-port <CH340 port>`; expect `Hash of data verified` and `[SUCCESS]`.
- [ ] **Step 2:** Record 2-3 min of the serial log while the owner: holds still 5 s, sets the pad down face up and picks it up ×5, presses X/D-pad/L2/R2/Triangle/START/SELECT deliberately, flicks the right stick both ways, then leaves it down. Expected: no `[pad] press` lines during set-downs (only `ignored` / `resting` / `picked up`), every deliberate press logged once, one colour step per flick, each feel distinguishable by hand, the BOTH page showing `IN x%  OUT y%` after a split preset.
- [ ] **Step 3:** If a threshold misfires, adjust it in its header, update its test, re-run the suite, reflash, re-record. Commit `fix(yellow): tune the pad guard on the desk` if anything changed.
- [ ] **Step 4:** Note in `docs/BACKLOG.md` the car check still owed: the handling pause with road motion, and the feels while driving.
