# Gauge Cluster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the old deck's Pokémon-bar OBD gauges with the approved green cluster screen (tach + speed, temperature, FUEL and BATTERY panels, pedal vs throttle), add the readings it needs, and make "read-only" a rule the code enforces.

**Architecture:** Pure, host-tested logic in `src/util/` (the command gate and new PIDs in `obd`, the schedule in `obdplan`, km/l and trip in a new `fueleco`, pedal/throttle scaling and the crank dip in a new `obdlearn`). The Bluetooth task (`companion/obdlink`) feeds every reading through them and publishes finished values in its `View`; the screen (`companion/gauges`) only draws. The link's one write path goes through `obd::allowed()`.

**Tech Stack:** ESP32 (CYD, `cyd_companion` env), Arduino core, PlatformIO, TFT_eSPI 2.5.43, BluetoothSerial (Classic, master), Preferences (NVS), Unity host tests via `tools/ntest.ps1` / `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-19-gauge-cluster-design.md` (builds on `docs/superpowers/specs/2026-10-15-obd-gauges-design.md`). Branch `feat/obd`.

## Global Constraints

- Read-only on the car, always: the only path to the adapter is `obdlink`'s `ask()`, which refuses anything `obd::allowed()` rejects (logs `[obd] BLOCKED <cmd>`, counts as `Reply::Error`).
- Allow-list, exactly: `01` + one PID as two upper-case hex digits; `ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATH1`, `ATSP0`, `ATRV`, `ATI`, `ATDPN`; each optionally ending in a single `\r`. Nothing else (no lower case, no spaces, no second command).
- Screen 320 × 240, black background, three greens only: bright `#41FF6A` (values), mid `#1D8A3A` (labels, frames, stale values), dark `#0E4A1F` (empty track). Green day and night.
- A value the car hasn't reported shows `--` and an empty bar/arc; never a made-up number.
- Heap: ~29–31 KB free with WiFi + Bluetooth; no full-screen or large sprite buffers; partial redraws only.
- Defaults (overridable in `src/config.h`, documented in `include/config.example.h`): `OBD_REDLINE 6500`, `OBD_TANK_L 44`, `OBD_DISPLACEMENT_CC 1497`, `OBD_VE 0.85`, `OBD_ETHANOL_PCT 30` (Brazilian pump gasoline, E30).
- Stoichiometric AFR 14.7 (gasoline), 9.0 (ethanol); densities 0.745 kg/L (gasoline), 0.789 kg/L (ethanol); mixing by mass from the volume fraction.
- Commits: conventional `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A car with neither MAF nor MAP (or no intake temperature):** km/l, AVG and KM LEFT show `--`, nothing divides by zero. Pinned in Task 4 (`test_no_air_reading_gives_nothing`).
2. **Pedal PID 49 unsupported but 4A supported, or neither:** the schedule asks 4A instead; with neither the PEDAL bar stays `--` and no slot is wasted. Pinned in Task 3 (`test_pedal_falls_back_to_e_then_nothing`).
3. **Long idle at 0 km/h:** km/l now is `--`, AVG keeps counting the idle fuel without dividing by zero, KM LEFT stays `--` under 2 km. Pinned in Task 4 (`test_idle_counts_fuel_not_distance`).
4. **A Bluetooth stall of many seconds between readings (and the `millis()` wrap):** no jump in distance or fuel; each step is capped. Pinned in Task 4 (`test_gap_is_capped_across_the_wrap`).
5. **Commands that look almost allowed** (`010c`, `01 0C`, `010C\r04`, `010C\r\r`, `01AB0`, `ATRV1`): refused. Pinned in Task 1 (`test_near_misses_are_refused`).

---

### Task 1: The read-only gate (`obd::allowed`, `obdlink::ask`)

**Files:**
- Modify: `src/util/obd.h`, `src/util/obd.cpp`, `src/companion/obdlink.cpp:38-53` (`ask`)
- Test: `test/test_obd/test_obd.cpp`

**Interfaces:**
- Produces: `bool obd::allowed(const char* cmd);` — true only for the Global Constraints allow-list.

- [ ] **Step 1: Write the failing tests** in `test/test_obd/test_obd.cpp` (register each in `main`):

```cpp
void test_every_command_the_code_sends_is_allowed() {
    for (int i = 0; i < (int)Pid::COUNT; ++i) TEST_ASSERT_TRUE(obd::allowed(obd::request((Pid)i)));
    const char* setup[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATSP0\r", "ATRV\r", "ATI", "ATDPN\r",
                           "ATH0\r", "ATH1\r", "0100\r", "0120\r", "0140\r", "0160\r"};
    for (const char* c : setup) TEST_ASSERT_TRUE_MESSAGE(obd::allowed(c), c);
}

void test_writes_and_other_modes_are_refused() {
    const char* bad[] = {"04\r", "0400\r", "03\r", "0300\r", "0200\r", "0800\r", "0902\r", "2F0101\r",
                         "3B01\r", "2201\r", "2101\r", "ATSH7E0\r", "ATMA\r", "ATCRA7E8\r", "ATCSM0\r",
                         "ATBD\r", "ATPP0CSV01\r", "ATWS\r", "", nullptr};
    for (const char* c : bad) TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c ? c : "null");
}

void test_near_misses_are_refused() {
    const char* bad[] = {"010c\r", "01 0C\r", "010C\r04\r", "010C\r\r", "01AB0\r", "01G0\r", "ATRV1\r",
                         "atz\r", "ATZ ", "\r"};
    for (const char* c : bad) TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c);
}
```

- [ ] **Step 2: Run** `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_obd/test_obd.cpp src/util/obd.cpp` — Expected: COMPILE FAILED (`allowed` is not a member of `obd`).

- [ ] **Step 3: Implement `bool obd::allowed(const char* cmd)`** in `obd.cpp`: strip at most one trailing `\r`, then the rest must be exactly one of the AT list, or `01` followed by exactly two characters in `0-9A-F`. Header comment: the only gate to the car (spec "Read-only guarantee").

- [ ] **Step 4: Run** the same command — Expected: all tests pass, `OK`.

- [ ] **Step 5: Gate the link.** In `obdlink.cpp`'s `ask()`, before `s_bt.print(cmd)`: if `!obd::allowed(cmd)`, `Serial.printf("[obd] BLOCKED %s\n", cmd)` (with `\r` shown as nothing), set `out[0] = '\0'`, return `false`. Confirm with `grep -n "s_bt\.\(print\|write\|println\)" src/companion/obdlink.cpp` that `ask()` holds the only call. Build: `pio run -e cyd_companion` → `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add src/util/obd.h src/util/obd.cpp src/companion/obdlink.cpp test/test_obd/test_obd.cpp
git commit -m "feat(obd): an allow-list gate in front of every command to the car"
```

---

### Task 2: New readings and the supported-PID scan (`obd`)

**Files:**
- Modify: `src/util/obd.h`, `src/util/obd.cpp`
- Test: `test/test_obd/test_obd.cpp`

**Interfaces:**
- Consumes: `obd::allowed` (Task 1).
- Produces:
  - `enum class Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, PedalD, PedalE, Fuel, Map, Maf, Ethanol, FuelSys, COUNT };` (new entries after `Battery`; existing order unchanged)
  - values: `PedalD` (`0149`), `PedalE` (`014A`), `Fuel` (`012F`), `Ethanol` (`0152`): `A × 100 / 255` %; `Map` (`010B`): `A` kPa; `Maf` (`0110`, 2 bytes): `(256A + B) / 100` g/s; `FuelSys` (`0103`, 1 byte read): `A` as a float (bit 2 = 4 means open loop on deceleration, i.e. fuel cut).
  - `const char* obd::supportRequest(int block);` → `"0100\r"`, `"0120\r"`, `"0140\r"`, `"0160\r"` for block 0–3, else `nullptr`.
  - `bool obd::parseSupported(int block, const char* raw, uint32_t* bits);` — the 4 bytes after `41 <block*0x20>`; bit 31 = PID `block*0x20 + 1`. Same tolerance as `parse` (spaces, headers, echo).
  - `bool obd::isSupported(const uint32_t bits[4], uint8_t pid);` — pid 0x01–0x80; pid 0 → false.

- [ ] **Step 1: Write the failing tests:**

```cpp
void test_new_pids_formulas() {
    float v = 0;
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::PedalD, "41 49 80\r\r>", &v)); TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.2f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Fuel, "412FFF", &v));          TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Map, "41 0B 23", &v));         TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.0f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Maf, "41 10 04 D2", &v));      TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.34f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Ethanol, "7E8 03 41 52 4D", &v)); TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.2f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::FuelSys, "41 03 04 00", &v));  TEST_ASSERT_EQUAL_FLOAT(4.0f, v);
    TEST_ASSERT_EQUAL_STRING("0110\r", obd::request(Pid::Maf));
    TEST_ASSERT_EQUAL_STRING("014A\r", obd::request(Pid::PedalE));
}

void test_supported_bitmap() {
    uint32_t bits[4] = {};
    TEST_ASSERT_TRUE(obd::parseSupported(0, "SEARCHING...\r41 00 BE 1F B8 13\r\r>", &bits[0]));
    TEST_ASSERT_EQUAL_HEX32(0xBE1FB813u, bits[0]);
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x01));    // bit 31
    TEST_ASSERT_FALSE(obd::isSupported(bits, 0x02));
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x0C));
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x20));    // bit 0: block 1 exists
    TEST_ASSERT_FALSE(obd::isSupported(bits, 0x49));   // block 2 unread: 0
    TEST_ASSERT_FALSE(obd::parseSupported(1, "41 00 BE 1F B8 13", &bits[1]));   // wrong block
    TEST_ASSERT_FALSE(obd::parseSupported(0, "41 00 BE 1F", &bits[0]));         // cut off
    TEST_ASSERT_FALSE(obd::parseSupported(0, "NO DATA", &bits[0]));
    TEST_ASSERT_NULL(obd::supportRequest(4));
    TEST_ASSERT_EQUAL_STRING("0140\r", obd::supportRequest(2));
}
```

- [ ] **Step 2: Run** `tools/ntest.ps1 test/test_obd/test_obd.cpp src/util/obd.cpp` — Expected: COMPILE FAILED (no `Pid::PedalD`).

- [ ] **Step 3: Implement** the new `DEFS` rows (FuelSys: 1 byte), the formulas in `value()`, `supportRequest`, `parseSupported` (reuse `fromLine`'s hex normalisation: look for `41` + block byte, read 4 bytes), `isSupported`.

- [ ] **Step 4: Run** the test — Expected: `OK`, including Task 1's gate test (every new `request()` is allowed).

- [ ] **Step 5: Commit** `feat(obd): pedal, fuel, airflow, ethanol and fuel-system readings; the supported-PID scan`.

---

### Task 3: The schedule, staleness and warnings (`obdplan`)

**Files:**
- Modify: `src/util/obdplan.h`, `src/util/obdplan.cpp`
- Test: `test/test_obdplan/test_obdplan.cpp`

**Interfaces:**
- Consumes: `obd::Pid` and `obd::isSupported` (Task 2).
- Produces:
  - `void Plan::setSupported(const uint32_t bits[4]);` — before it's called every PID is assumed supported (today's behaviour). After: PIDs the car says it lacks are never asked; `Battery` is always asked.
  - `void Plan::crankWatch(bool on);` — while on, `next()` returns `Battery` and `Rpm` only, `Battery` 9 times out of 10.
  - Fast set, in this repeating pattern: `Rpm, Speed, X` where `X` cycles `Pedal, Throttle, Air`. `Pedal` = `PedalD`, else `PedalE` if supported, else skipped. `Air` = `Maf`, else `Map`, else skipped.
  - Slow set (one per `SLOW_MS`, unchanged 2000): `Coolant, Battery, Fuel, Intake, Ethanol, Load, FuelSys`.
  - `uint32_t obdplan::staleAfterMs(obd::Pid p);` → 3000 for the fast set (Rpm, Speed, PedalD/E, Throttle, Maf, Map), 30000 for the rest.
  - `bool obdplan::batteryWarn(float volts, bool engineRunning);` → running: `volts < 13.0`; off: `volts < 12.2`.
  - `bool obdplan::coolantWarn(float c);` → `c > 105` (the existing Hot band). `bool obdplan::rpmWarn(float rpm, float redline);` → `rpm >= 0.85 × redline`.

- [ ] **Step 1: Write the failing tests** (keep the existing ones; update `test_fast_pair_every_cycle_slow_ones_in_turn`'s `isFast` to the new fast set and its expectations accordingly):

```cpp
void test_rpm_and_speed_at_least_three_a_second() {
    obdplan::Plan plan;
    int rpm = 0, speed = 0;
    for (uint32_t t = 1000; t < 11000; t += 80) {          // 10 s at 80 ms a request
        const Pid p = plan.next(t);
        rpm += p == Pid::Rpm; speed += p == Pid::Speed;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_GREATER_OR_EQUAL(30, rpm);
    TEST_ASSERT_GREATER_OR_EQUAL(30, speed);
}

void test_pedal_falls_back_to_e_then_nothing() {
    uint32_t bits[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu & ~(1u << (32 - (0x49 - 0x40))), 0};
    obdplan::Plan plan;
    plan.setSupported(bits);
    bool d = false, e = false;
    for (uint32_t t = 0; t < 5000; t += 80) { const Pid p = plan.next(t); d |= p == Pid::PedalD; e |= p == Pid::PedalE; plan.done(p, Reply::Ok, t); }
    TEST_ASSERT_FALSE(d); TEST_ASSERT_TRUE(e);
    bits[2] &= ~(1u << (32 - (0x4A - 0x40)));
    obdplan::Plan none;
    none.setSupported(bits);
    for (uint32_t t = 0; t < 5000; t += 80) { const Pid p = none.next(t); TEST_ASSERT_FALSE(p == Pid::PedalD || p == Pid::PedalE); none.done(p, Reply::Ok, t); }
}

void test_maf_preferred_map_otherwise() { /* bits with 0x10 set: Maf asked, Map never; 0x10 clear: Map asked */ }

void test_crank_watch_is_mostly_battery() {
    obdplan::Plan plan;
    plan.crankWatch(true);
    int bat = 0, rpm = 0;
    for (uint32_t t = 0; t < 2000; t += 100) { const Pid p = plan.next(t); bat += p == Pid::Battery; rpm += p == Pid::Rpm; TEST_ASSERT_TRUE(p == Pid::Battery || p == Pid::Rpm); }
    TEST_ASSERT_EQUAL_INT(18, bat); TEST_ASSERT_EQUAL_INT(2, rpm);
}

void test_stale_and_warnings() {
    TEST_ASSERT_EQUAL_UINT32(3000, obdplan::staleAfterMs(Pid::Rpm));
    TEST_ASSERT_EQUAL_UINT32(30000, obdplan::staleAfterMs(Pid::Coolant));
    TEST_ASSERT_TRUE(obdplan::batteryWarn(12.9f, true));  TEST_ASSERT_FALSE(obdplan::batteryWarn(12.9f, false));
    TEST_ASSERT_TRUE(obdplan::batteryWarn(12.1f, false)); TEST_ASSERT_FALSE(obdplan::batteryWarn(14.1f, true));
    TEST_ASSERT_TRUE(obdplan::coolantWarn(106));           TEST_ASSERT_FALSE(obdplan::coolantWarn(105));
    TEST_ASSERT_TRUE(obdplan::rpmWarn(5525, 6500));        TEST_ASSERT_FALSE(obdplan::rpmWarn(5500, 6500));
}
```

Write `test_maf_preferred_map_otherwise` with real assertions in the same style as the pedal test (the comment above states them).

- [ ] **Step 2: Run** `tools/ntest.ps1 test/test_obdplan/test_obdplan.cpp src/util/obdplan.cpp src/util/obd.cpp` — Expected: COMPILE FAILED.

- [ ] **Step 3: Implement** in `obdplan.cpp`; the existing NO DATA back-off (`RETRY_MS`) still applies on top of the supported bits. Remove `rpmBand/coolantBand/batteryBand`/`Band` only if nothing uses them after Task 6 (decide in Task 6; leave them here).

- [ ] **Step 4: Run** — Expected: `OK`.

- [ ] **Step 5: Commit** `feat(obd): a faster schedule for the cluster, crank watch, staleness and warnings`.

---

### Task 4: km/l, trip average and km left (`util/fueleco`)

**Files:**
- Create: `src/util/fueleco.h`, `src/util/fueleco.cpp`
- Test: `test/test_fueleco/test_fueleco.cpp`

**Interfaces:**
- Produces:

```cpp
namespace fueleco {
struct Car { float displacementL, ve, tankL, ethanolPct; };        // from config (1.497, 0.85, 44, 30)
// Air mass flow, g/s: MAF when maf >= 0, else speed-density from MAP (kPa), RPM and intake (°C);
// NAN when neither works (no MAF, and MAP/RPM/intake missing: pass NAN for a missing value).
float airGs(float maf, float mapKpa, float rpm, float intakeC, const Car& c);
float afr(float ethanolPct);             // stoichiometric for the blend (by mass)
float densityKgL(float ethanolPct);      // blend density
float litresPerHour(float airGs, float ethanolPct);
class Trip {
public:
    // One step at nowMs: speed km/h, air g/s (NAN = unknown), fuelCut (no fuel burnt),
    // ethanolPct (car's value, or the default). Steps longer than 2 s count as 2 s.
    void step(uint32_t nowMs, float speedKmh, float airGs, bool fuelCut, float ethanolPct);
    float nowKmL() const;    // smoothed (~1.5 s); NAN below 3 km/h or without air; 99.9 cap (fuel cut)
    float avgKmL() const;    // NAN until some fuel has been counted
    float kmLeft(float fuelPct, const Car& c) const;   // NAN under 2 km driven or fuelPct NAN
    float km() const;
};
}
```

- [ ] **Step 1: Write the failing tests** (`test/test_fueleco/test_fueleco.cpp`, Unity, same file layout as `test_obdplan`):

```cpp
static const fueleco::Car CITY{1.497f, 0.85f, 44.0f, 30.0f};

void test_blend_for_e30() {
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 12.27f, fueleco::afr(30));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7582f, fueleco::densityKgL(30));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.7f, fueleco::afr(0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, fueleco::afr(100));
}
void test_speed_density_and_maf() {
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 12.3f, fueleco::airGs(NAN, 50, 2000, 26.85f, CITY));   // 300 K
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.34f, fueleco::airGs(12.34f, 50, 2000, 27, CITY)); // MAF wins
}
void test_no_air_reading_gives_nothing() {
    TEST_ASSERT_TRUE(isnan(fueleco::airGs(NAN, NAN, 2000, 27, CITY)));
    TEST_ASSERT_TRUE(isnan(fueleco::airGs(NAN, 50, 2000, NAN, CITY)));
    fueleco::Trip t;
    for (uint32_t ms = 0; ms < 10000; ms += 250) t.step(ms, 80, NAN, false, 30);
    TEST_ASSERT_TRUE(isnan(t.nowKmL())); TEST_ASSERT_TRUE(isnan(t.avgKmL()));
    TEST_ASSERT_TRUE(isnan(t.kmLeft(50, CITY)));
}
void test_steady_cruise() {          // 12.3 g/s at 80 km/h, E30: ~16.8 km/l
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 200000; ms += 250) t.step(ms, 80, 12.3f, false, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 16.8f, t.nowKmL());
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 16.8f, t.avgKmL());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4.44f, t.km());
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 0.5f * 44 * 16.8f, t.kmLeft(50, CITY));
}
void test_fuel_cut_is_free() {
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 60000; ms += 250) t.step(ms, 80, 12.3f, false, 30);
    const float before = t.avgKmL();
    for (uint32_t ms = 60250; ms <= 80000; ms += 250) t.step(ms, 70, 9.0f, true, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.9f, t.nowKmL());
    TEST_ASSERT_GREATER_THAN_FLOAT(before, t.avgKmL());
}
void test_idle_counts_fuel_not_distance() {
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 60000; ms += 250) t.step(ms, 0, 2.5f, false, 30);
    TEST_ASSERT_TRUE(isnan(t.nowKmL()));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.km());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.avgKmL());      // fuel burnt, no distance: 0 km/l
    TEST_ASSERT_TRUE(isnan(t.kmLeft(50, CITY)));
}
void test_gap_is_capped_across_the_wrap() {
    fueleco::Trip t;
    t.step(0xFFFFF000u, 100, 15, false, 30);
    t.step(0x00002000u, 100, 15, false, 30);                 // ~12 s later, across the wrap
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f * 2 / 3600, t.km());   // counted as 2 s
}
```

- [ ] **Step 2: Run** `tools/ntest.ps1 test/test_fueleco/test_fueleco.cpp` — Expected: COMPILE FAILED.

- [ ] **Step 3: Implement.** Speed-density: `air g/s = MAP_kPa·1000 × displacementL/1000 × RPM/120 × VE / (287.05 × (intakeC + 273.15)) × 1000`. Blend: volume fraction `e = pct/100`, mass fraction `w = 0.789e / (0.789e + 0.745(1−e))`, `afr = 1/(w/9.0 + (1−w)/14.7)`, density `0.789e + 0.745(1−e)`. `L/h = air / afr / (density·1000) × 3600`. `nowKmL`: exponential smoothing of speed/(L/h) with a ~1.5 s time constant, capped at 99.9; fuel cut sets it to 99.9. Trip integrates km and litres per step (fuel cut adds 0 L).

- [ ] **Step 4: Run** — Expected: `6 Tests 0 Failures`.

- [ ] **Step 5: Commit** `feat(obd): km/l from the engine's air, trip average and km left`.

---

### Task 5: Pedal/throttle scaling and the crank dip (`util/obdlearn`)

**Files:**
- Create: `src/util/obdlearn.h`, `src/util/obdlearn.cpp`
- Test: `test/test_obdlearn/test_obdlearn.cpp`

**Interfaces:**
- Produces:

```cpp
namespace obdlearn {
// Maps a raw position % onto 0–100 between the lowest and highest seen, starting from lo0/hi0.
class Range {
public:
    Range(float lo0, float hi0);
    float scale(float raw);          // learns, then returns 0..100
};
// The battery's dip while the engine cranks. feed() every battery reading with whether the engine
// runs (RPM > 0). value(): the lowest reading before the first running sample, NAN if the first
// reading already had the engine running (missed) or the engine hasn't started yet.
class StartDip {
public:
    void feed(float volts, bool running);
    float value() const;
};
}
```

Starting ranges: pedal `Range(16, 80)`, throttle `Range(12, 80)` (in Task 6's link code).

- [ ] **Step 1: Write the failing tests:**

```cpp
void test_range_learns_and_clamps() {
    obdlearn::Range r(16, 80);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.scale(16));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, r.scale(80));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, r.scale(48));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.scale(14));        // learns a lower released position
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, r.scale(86));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, r.scale(50));         // (50-14)/(86-14)
}
void test_dip_caught_when_waiting_at_key_on() {
    obdlearn::StartDip d;
    d.feed(12.5f, false); d.feed(12.4f, false); d.feed(10.1f, false); d.feed(11.6f, false);
    TEST_ASSERT_TRUE(isnan(d.value()));                        // not started yet
    d.feed(14.0f, true);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
    d.feed(9.0f, false); d.feed(14.1f, true);                  // a stall and restart: first start kept
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.1f, d.value());
}
void test_dip_missed_when_already_running() {
    obdlearn::StartDip d;
    d.feed(14.1f, true);
    TEST_ASSERT_TRUE(isnan(d.value()));
    d.feed(13.9f, true);
    TEST_ASSERT_TRUE(isnan(d.value()));
}
```

- [ ] **Step 2: Run** `tools/ntest.ps1 test/test_obdlearn/test_obdlearn.cpp` — Expected: COMPILE FAILED.
- [ ] **Step 3: Implement** both classes.
- [ ] **Step 4: Run** — Expected: `3 Tests 0 Failures`.
- [ ] **Step 5: Commit** `feat(obd): pedal and throttle scaling, the battery's dip at engine start`.

---

### Task 6: The link: scan, remembered adapter, crank watch, derived values (`companion/obdlink`)

**Files:**
- Modify: `src/companion/obdlink.h`, `src/companion/obdlink.cpp`, `include/config.example.h`

**Interfaces:**
- Consumes: Tasks 1–5.
- Produces: `obdlink::View` gains (all `NAN` when unknown):

```cpp
    float kmlNow, kmlAvg, kmLeft;    // fueleco
    float pedalPct, throttlePct;     // obdlearn::Range, 0..100
    float atStart;                   // obdlearn::StartDip
    bool fuelCut;                    // FuelSys bit 2, else the fallback rule
    uint32_t supported[4];           // the scan (0 before)
```

- [ ] **Step 1: The supported scan.** After `init()`, ask `obd::supportRequest(0..3)` while the previous block's PID `block*0x20+0x20` bit says the next exists; `plan.setSupported(bits)`; log one line `[obd] supported: 04 05 0B 0C ...` (two-digit hex of every supported PID). A failed scan logs `[obd] supported: unknown` and leaves the plan's default (all supported).
- [ ] **Step 2: Remember the adapter.** On a successful `init()`, store the address (`addr.toString()`) and name in NVS (`Preferences`, namespace `"obd"`, keys `"addr"`, `"name"`). At the start of each loop, if an address is stored, `s_bt.connect(BTAddress(stored))` first; on failure fall back to `findAdapter()` as today.
- [ ] **Step 3: Crank watch and NoEngine.** While the link is `NoEngine` or the last RPM was absent or 0: `plan.crankWatch(true)` and pace requests ~100 ms apart instead of today's `delay(NO_ENGINE_MS)`; otherwise `crankWatch(false)`. Every `Battery` reading feeds `StartDip::feed(v, rpm > 0)`.
- [ ] **Step 4: Derived values.** On each reading: `Maf`/`Map` (with the latest RPM and intake) → `fueleco::airGs`; then `Trip::step(now, speed, air, fuelCut, ethanol)` where `ethanol` = the car's `Ethanol` reading if present else `OBD_ETHANOL_PCT`; `fuelCut` = `((int)FuelSys value & 4) != 0` when FuelSys is supported, else `pedalPct < 2 && rpm > 1200 && speed > 20`. Pedal/throttle through their `Range`. Publish the new `View` fields under the lock. Add the `#ifndef` defaults (`OBD_TANK_L 44`, `OBD_DISPLACEMENT_CC 1497`, `OBD_VE 0.85`, `OBD_ETHANOL_PCT 30`) at the top of `obdlink.cpp` and document them in `include/config.example.h` beside `OBD_REDLINE`.
- [ ] **Step 5: Build** `pio run -e cyd_companion` → `SUCCESS`; `tools/run_tests.ps1` → `fail=0`. Flash the old deck (COM13; it moves between COM13/COM14 — check `pio device list`) and capture 60 s: boot clean, `[obd]` lines show discovery or the remembered connect attempt (no car needed for this).
- [ ] **Step 6: Commit** `feat(companion): supported-PID scan, fast reconnect, crank watch and the derived readings`.

---

### Task 7: The cluster screen (`companion/gauges`, `avatarview`, `app`)

**Files:**
- Modify: `src/companion/gauges.cpp` (rewrite the drawing), `src/companion/gauges.h` (header comment), `src/companion/avatarview.h`, `src/companion/avatarview.cpp`, `src/companion/app.cpp` (gauge branch of `loop()`), `src/util/obdplan.h/.cpp` (drop `Band` and the `*Band` functions if unused)

**Interfaces:**
- Consumes: `obdlink::View` (Task 6), `obdplan::staleAfterMs/batteryWarn/coolantWarn/rpmWarn` (Task 3).
- Produces: `avatarview::place(x, y, avatarview::HALF)` with `constexpr int HALF = -1;` — a 46 × 46 portrait (every other pixel of the 92 × 92 art), one still frame (entry 0, frame 0), no name tag.

- [ ] **Step 1: Zero Two at half size.** Add `HALF` to `avatarview`; in `show()`, for `HALF` push every other pixel of every other row, draw once, skip the name tag and never animate.
- [ ] **Step 2: The screen.** Rewrite `gauges::draw` to the spec's table (the layout in `.superpowers/brainstorm/571-1792425251/content/simple-v1.html`): tach arc via TFT_eSPI `drawArc` (thick ~16 px, centre ~(160,126), radius ~102, 0→`OBD_REDLINE`, the zone from `0.85 × OBD_REDLINE` drawn in mid green when empty), speed ~48 px (font 4 at text size 2 or font 6/7), `KM/H · <rpm> RPM` small under it, the lever row left blank, `TEMP` top right, the FUEL and BATTERY panels, PEDAL and THROTTLE bars; `--` for `NAN`/missing; stale (`staleAfterMs`) in mid green; warnings inverted. `CHARGING` shows when battery ≥ 13.2 V and RPM > 0. Keep the "redraw only what changed" approach (cache the shown text per field; redraw the arc only across the changed span).
- [ ] **Step 3: App.** In `app.cpp`'s gauge branch: on entering, fill black, `avatarview::place(4, 4, avatarview::HALF)`; drop `screens::bar(...)` while the gauges show; the night-palette repaint no longer applies to the gauges (they're green both ways) but keep the invalidate on entry. Leaving restores the screen as today.
- [ ] **Step 4: Build and look.** `pio run -e cyd_companion` → `SUCCESS`; `tools/run_tests.ps1` → `fail=0`. Temporarily build with `-D GAUGE_DEMO` (a `#ifdef` in `app.cpp` that feeds a fixed fake `View`: 87 km/h, 3850 RPM, 89 °C, 61 % fuel, 13.8 km/l, 14.1 V, pedal 62, throttle 41) to see the screen without a car; flash, photograph-free check by the owner; remove the flag from the build afterwards (keep the `#ifdef` code, documented in the gauges header).
- [ ] **Step 5: Commit** `feat(companion): the green gauge cluster`.

---

### Task 8: Docs, all builds, and the on-car check

**Files:**
- Modify: `README.md`, `CHANGELOG.md` (Unreleased), `docs/ARCHITECTURE.md` (OBD section and the pure-module list: `fueleco`, `obdlearn`), `docs/BACKLOG.md` (the gear side task)

- [ ] **Step 1: Docs**, in each file's style: the cluster screen, the new readings, the config defaults, **read-only and how it's enforced** (the allow-list gate), the gear side task under "Ideas".
- [ ] **Step 2: Verify:** `tools/run_tests.ps1` → `fail=0`; `python -m unittest discover -s tools -p "test_gen_*.py"` → `OK`; `pio run -e s3`, `-e cyd`, `-e cyd_companion` → `SUCCESS`.
- [ ] **Step 3: Commit** `docs: the gauge cluster, its readings and the read-only gate`.
- [ ] **Step 4: On the car (owner; supersedes the 2026-10-15 plan's Task 7, on hold until the owner resumes it).** ELM327 plugged in, deck logged over USB. Check: the `[obd] supported:` line (save it); no `[obd] BLOCKED` line; speed, RPM, fuel %, temperature against the car's own cluster; km/l plausible on a steady cruise (~14–18 at 80 km/h); AT START caught when waiting ~5 s at key-on before cranking, `--` when cranking at once; the screen readable at a glance.
