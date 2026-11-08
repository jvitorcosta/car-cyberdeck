# Car Health Check on the Yellow Deck: Implementation Plan (part 1 of 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Read the City over the Vgate BLE adapter on a third deck (the OBD deck) and show the agreed screens A-D on the yellow deck, with instant warnings, a per-minute trip log, and fuel (voice fill-up, R$, range). This covers spec phases 1 and 2.

**Architecture** (updated 2026-10-29; see "Architecture change" before Task 8):
- **Pure, host-tested modules** in a new group `src/util/car/`: ported from `feat/obd` plus new ones for health, the trip summary, fuel, screen texts and the `/car` snapshot format.
- **The OBD deck** (`src/obd/`, env `obddeck`) runs the adapter on NimBLE 2.5.1 (its only BLE link) and keeps health, the trip log and the fuel book; it serves `GET /car` and `GET /fill`, signed.
- **`src/yellow/ledpanel.cpp`** polls `/car` and gains two screens (Check, Parked), a new trip band, warnings on the prompt line and a fill-up voice command it forwards.
- **Out of scope here:** upload (spec phase 3) and the PC checkup (phase 4) get their own plans. This plan fixes the trip-log row format they will consume.

**Tech Stack:**
- PlatformIO, Arduino-ESP32 (espressif32@6.9.0), NimBLE-Arduino 2.5.1, TFT_eSPI, LittleFS.
- Unity host tests via `tools/ntest.ps1` / `tools/run_tests.ps1`.
- Python 3 + `bless` for the bench adapter.

**Spec:** `docs/superpowers/specs/2026-10-28-car-health-check-design.md`

## Global Constraints

- **Read-only:**
  - Only Mode 01, `03`, `07` and the AT setup commands may ever be sent. Mode 04 (clear codes) and any other mode are refused by `obd::allowed()`.
  - Nothing writes to the car.
- **Libraries:** NimBLE-Arduino stays at 2.5.1 (the LED panel and `lib/xboxpad` need it). The OBD code is ported from NimBLE 1.4.3 APIs.
- **BLE connections:** the 4th connection needs `-D CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` in `[env:ledpanel]`. The default 3 is already taken by two lights + the pad.
- **Compile flags:** `src/` builds with `-Wall -Wextra -Wshadow -Wdouble-promotion -Wlogical-op -Wduplicated-cond -Wnull-dereference -Wformat=2 -Werror`.
- **Screen text:** English, all caps, VT323 faces SMALL/BODY/BIG (17/19/24/36 px), colours only through `termpal` roles. ALERT (red) is only for problems.
- **Defaults** (spec values, one table in `carhealth`):
  - battery running < 12.0 V for 60 s;
  - coolant > 105 °C;
  - LTFT beyond ±10 % for a whole trip = note, beyond ±20 % = problem;
  - MIL on or a new stored code = problem;
  - a new pending code = note.
- **Car constants:** tank 44 L, displacement 1.497 L, VE 0.85 (`feat/obd` values). Ethanol from PID 52 when it reads 0-100, else 30 %.
- **Screen D** shows after 5 s at 0 km/h on a trip of ≥ 0.5 km, and hides at > 3 km/h or on a tap.
- **Trip-log persistence:** every 30 s. Rows are one per minute. Data total ≤ 96 KB on flash, oldest trip dropped first.
- **Voice:** `"filled up at 6.49"` style. The price is accepted only within 3.00-15.00 R$/L.
- **No secrets, real SSIDs, car data or personal notes** in this public repo.
- **Commits:** conventional style `type(scope): subject`.
- **No merge to `main`** until the owner's car test passes (checklist in `docs/BACKLOG.md`).

## Review Focus

1. **No adapter at all** (not plugged, out of range, phone holding it): the deck must behave exactly as today (LEDs, pad, voice, KARAOKE). The OBD figures read `--`, nothing red, and screen A never blocks the boot.
2. **The link drops mid-trip:** distance and litres stop growing (no extrapolation), screen C shows `--` for stale figures, and the trip log keeps what it had.
3. **The tracker's traffic on the same bus:** a reply for a different PID or a different mode must be rejected, never stored as this request's value.
4. **Power cut while a row is being written:** at the next boot the log reads back every whole row and drops the cut one. The deck never shows nonsense or crashes on it.
5. **No fill-up recorded yet, or a misheard price** ("filled up at 649"): RANGE and R$ show `--` and the voice reply says what was heard. A price outside 3.00-15.00 is refused.

Tests pinning these: Task 1 (`test_reply_for_other_pid_is_error`), Task 2 (`test_stale_reading_adds_no_distance`), Task 5 (`test_cut_row_is_dropped`), Task 4 (`test_no_fill_up_means_no_range`, `test_price_out_of_range_refused`), Task 8 (bench run without adapter).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/util/car/obd.{h,cpp}` | ELM327 requests/replies, the read-only gate, Mode 01 status, Mode 03/07 codes (from `feat/obd`, extended) |
| `src/util/car/obdble.{h,cpp}` | GATT characteristic pick + notification inbox (from `feat/obd`, unchanged) |
| `src/util/car/obdplan.{h,cpp}` | request order, link health (from `feat/obd`, extended with trims, module volts, codes cadence) |
| `src/util/car/fueleco.{h,cpp}` | air → litres, km/l, fuel cut (from `feat/obd`, unchanged) |
| `src/util/car/carhealth.{h,cpp}` | instant warnings and the per-item verdict (OK / NOTE / PROBLEM) with hold timers |
| `src/util/car/tripsum.{h,cpp}` | per-minute aggregation, trip summary, after-start recharge profile, JSON-line encoding |
| `src/util/car/triplog.{h,cpp}` | trip files on a storage interface: append, recover after a cut, cap, list |
| `src/util/car/fuelbook.{h,cpp}` | fill-up state, price, range, NVS record; `fuelvoice` phrase parse |
| `src/util/car/healthview.{h,cpp}` | the text lines of screens A, B, C's band and D; the parked gate |
| `src/yellow/obdlink.{h,cpp}` | the adapter link on NimBLE 2: find, connect (from the link task), ask loop task, snapshot |
| `src/yellow/ledpanel.cpp` | wiring: Check and Parked screens, trip band, warnings, voice fill-up, log writes |
| `src/util/yellow/deckmenu.{h,cpp}` | `Screen::Check`, `Screen::Parked` with one full-screen tap rect each |
| `tools/obd_sim.py`, `tools/test_obd_sim.py` | bench ELM327 over BLE (FFF0/FFF1/FFF2) with canned City replies |
| `test/test_<module>/test_<module>.cpp` | one Unity suite per pure module |

---

### Task 1: Port the OBD core into `util/car` and extend it

**Files:**
- Create: `src/util/car/obd.{h,cpp}`, `src/util/car/obdble.{h,cpp}`, `src/util/car/obdplan.{h,cpp}`, `src/util/car/fueleco.{h,cpp}` (from `git show feat/obd:src/util/<name>`)
- Create: `test/test_obd/`, `test/test_obdble/`, `test/test_obdplan/`, `test/test_fueleco/` (from `feat/obd`, include paths fixed to `../../src/util/car/`)
- Modify: `src/util/car/obd.{h,cpp}`, `src/util/car/obdplan.{h,cpp}`

**Interfaces:**
- Produces:
  - `enum class obd::Pid` gains `TrimS` (0106), `TrimL` (0107), `ModuleV` (0142, `(256A+B)/1000` V) and `Status` (0101) before `COUNT`.
  - `obd::parse(Pid::Status, raw, &v)` sets `v` to the stored-code count, with the MIL flag added as +1000.
  - `bool obd::milOn(float statusValue)` and `int obd::codeCount(float statusValue)`.
  - Codes requests: `const char* obd::codesRequest(bool pending)` returns `"03\r"` or `"07\r"`.
  - `int obd::parseCodes(const char* raw, bool pending, char (*out)[6], int max)` writes codes like `"P0301"` and returns the count. It returns -1 on an error or a reply for the other mode.
  - `obd::allowed()` also accepts exactly `"03\r"` and `"07\r"`.
  - `obdplan::Plan` puts `TrimS, TrimL, ModuleV, Status` in its slow set.
  - New `bool obdplan::Plan::codesDue(uint32_t nowMs)` is true at the first call after `setSupported()`, then every 300000 ms. `void codesDone(uint32_t nowMs)` resets it.

- [ ] **Step 1: Copy the four modules and their suites from `feat/obd`, fix include paths, run them**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: every suite passes, including the four copied ones (unchanged behaviour).

- [ ] **Step 2: Write the failing tests for the extensions** (in `test/test_obd/test_obd.cpp` and `test/test_obdplan/test_obdplan.cpp`)

```cpp
void test_allowed_codes_requests_only() {
    TEST_ASSERT_TRUE(obd::allowed("03\r"));
    TEST_ASSERT_TRUE(obd::allowed("07\r"));
    TEST_ASSERT_FALSE(obd::allowed("04\r"));
    TEST_ASSERT_FALSE(obd::allowed("0304\r"));
    TEST_ASSERT_FALSE(obd::allowed("03\r04\r"));
}
void test_trims_and_module_volts() {
    float v;
    TEST_ASSERT_EQUAL(obd::Reply::Ok, obd::parse(obd::Pid::TrimL, "41 07 8C\r", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.4f, v);                 // (140-128)*100/128
    TEST_ASSERT_EQUAL(obd::Reply::Ok, obd::parse(obd::Pid::ModuleV, "41 42 36 B0\r", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.0f, v);
}
void test_status_mil_and_count() {
    float v;
    TEST_ASSERT_EQUAL(obd::Reply::Ok, obd::parse(obd::Pid::Status, "41 01 82 07 E5 00\r", &v));
    TEST_ASSERT_TRUE(obd::milOn(v));
    TEST_ASSERT_EQUAL(2, obd::codeCount(v));
}
void test_parse_stored_codes() {
    char codes[4][6];
    TEST_ASSERT_EQUAL(2, obd::parseCodes("43 02 03 01 01 71\r", false, codes, 4));
    TEST_ASSERT_EQUAL_STRING("P0301", codes[0]);
    TEST_ASSERT_EQUAL_STRING("P0171", codes[1]);
    TEST_ASSERT_EQUAL(0, obd::parseCodes("43 00\r", false, codes, 4));
}
void test_reply_for_other_pid_is_error() {        // Review Focus 3: the tracker's traffic
    float v = -1;
    TEST_ASSERT_EQUAL(obd::Reply::Error, obd::parse(obd::Pid::Speed, "41 0C 1A F8\r", &v));
    TEST_ASSERT_EQUAL_FLOAT(-1, v);
    char codes[2][6];
    TEST_ASSERT_EQUAL(-1, obd::parseCodes("47 01 03 01\r", false, codes, 2));   // a 07 reply to 03
}
// test_obdplan.cpp
void test_codes_due_at_start_then_every_five_minutes() {
    obdplan::Plan p;
    const uint32_t all[4] = {~0u, ~0u, ~0u, ~0u};
    p.setSupported(all);
    TEST_ASSERT_TRUE(p.codesDue(1000));
    p.codesDone(1000);
    TEST_ASSERT_FALSE(p.codesDue(1000 + 299999));
    TEST_ASSERT_TRUE(p.codesDue(1000 + 300000));
}
void test_slow_set_includes_trims_and_status() { /* drive next() over 20 SLOW_MS periods; assert TrimS, TrimL, ModuleV, Status each asked at least once */ }
```

- [ ] **Step 3: Run them and see them fail**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_obd\test_obd.cpp src\util\car\obd.cpp`
Expected: compile error (no `Pid::TrimL`, `parseCodes`).

- [ ] **Step 4: Implement the extensions in `obd.{h,cpp}` and `obdplan.{h,cpp}`**

Notes on the encoding:
- **Trims:** `(A-128)*100/128` %.
- **Status:** A bit 7 = MIL, A & 0x7F = count. Return `count + (mil ? 1000 : 0)`.
- **DTC bytes:** two per code, top 2 bits → P/C/B/U. Skip `0000` pairs. Accept a leading count byte (CAN) and several ECU lines.
- **Arrays:** extend the `DEFS` and `PID_BYTE` arrays in Pid order.

- [ ] **Step 5: Run all suites**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add src/util/car test/test_obd test/test_obdble test/test_obdplan test/test_fueleco
git commit -m "feat(car): OBD core in util/car, with trims, module volts, status and read-only codes"
```

---

### Task 2: `carhealth`: instant warnings and verdicts

**Files:**
- Create: `src/util/car/carhealth.{h,cpp}`, `test/test_carhealth/test_carhealth.cpp`

**Interfaces:**
- Produces:

```cpp
namespace carhealth {
enum class Level : uint8_t { Ok, Note, Problem };
enum class Item : uint8_t { Battery, Engine, COUNT };
struct Reading {            // NAN / -1 = unknown
    uint32_t nowMs; bool engineRunning;
    float volts;            // ATRV
    float coolantC, ltftPct;
    int storedCodes, pendingCodes; bool mil;
};
struct Verdict { Level level; char why[48]; };   // why: "" when Ok; plain words, caps, <= 47 chars
class Watch {
public:
    void feed(const Reading& r);            // ~1 Hz
    Verdict item(Item i) const;             // the worst live condition for the item
    bool critical() const;                  // coolant hot or MIL: may draw while moving
    void endTrip();                         // LTFT-for-a-whole-trip is judged here
};
constexpr float BATT_LOW_V = 12.0f;  constexpr uint32_t BATT_HOLD_MS = 60000;
constexpr float COOLANT_HOT_C = 105.0f;
constexpr float LTFT_NOTE = 10.0f, LTFT_PROBLEM = 20.0f;
}
```

- [ ] **Step 1: Write the failing tests**

```cpp
void test_battery_low_needs_sixty_seconds_running();   // 11.9 V for 59 s -> Ok; 60 s -> Problem "BATTERY LOW WHILE RUNNING"
void test_battery_low_ignored_with_engine_off();       // engineRunning=false, 11.5 V -> Ok (cranking, ignition only)
void test_smart_charging_idle_is_ok();                 // 12.3 V running for 10 min -> Ok
void test_coolant_hot_is_critical_problem();           // 106 C -> Engine Problem, critical() true
void test_mil_or_new_stored_code_is_problem();
void test_pending_code_is_note();                      // why == "1 PENDING CODE, NOT CONFIRMED"
void test_ltft_note_only_over_whole_trip();            // 12 % for the trip -> Note after endTrip(), Ok before
void test_ltft_problem_over_twenty();
void test_unknown_readings_never_warn();               // all NAN / -1 -> Ok for both items
```

- [ ] **Step 2: Run, see them fail**

Run: `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_carhealth\test_carhealth.cpp src\util\car\carhealth.cpp`
Expected: compile error.

- [ ] **Step 3: Implement `carhealth::Watch`.** For LTFT, keep a running mean while the engine runs and judge it in `endTrip()`; `item()` reports it until the next trip ends.

- [ ] **Step 4: Run, see them pass.** Same command. Expected: PASS.

- [ ] **Step 5: Commit**: `feat(car): instant health warnings with hold timers`

---

### Task 3: `tripsum`: minute rows, trip summary, recharge profile

**Files:**
- Create: `src/util/car/tripsum.{h,cpp}`, `test/test_tripsum/test_tripsum.cpp`

**Interfaces:**
- Consumes: `fueleco::Trip`'s litres (Task 1) through the per-second sample below.
- Produces:

```cpp
namespace tripsum {
struct Sample {            // one per second from the loop; NAN = unknown
    uint32_t nowMs; float kmh, rpm, litres /*this second*/, volts, coolantC, trimS, trimL;
    bool fresh;            // the link's readings are fresh (obdplan::staleAfterMs)
};
struct Row {               // one minute
    uint16_t minute; float km, avgKmh, maxKmh, avgRpm, litres;
    uint16_t idleS; float vMin, vAvg, vMax, coolMax, trimS, trimL;
    uint8_t flags;         // bit0 MIL, bit1 new stored code, bit2 new pending code
};
struct Summary { float km, litres, kmL /*NAN if no fuel counted*/, avgKmh; uint32_t seconds, idleS; };
struct Recharge { float peakV; uint16_t secondsOver13_8; bool done; };  // first 5 min after start
class Trip {
public:
    void add(const Sample& s);         // a non-fresh sample adds time only (no km, no litres)
    bool rowReady() const;  Row takeRow();   // a row each full minute
    Summary summary() const;
    Recharge recharge() const;
};
size_t jsonRow(const Row& r, const char* tripId, char* out, size_t n);  // one line + '\n', 0 if no fit
}
```

The JSON keys are fixed for the later upload/checkup plans:
`{"t":"<id>","m":3,"km":0.62,"kmh":37.1,"kmhMax":52,"rpm":1840,"l":0.051,"idle":4,"v":[13.9,14.1,14.4],"cool":91,"trim":[1.6,2.3],"f":0}`

- [ ] **Step 1: Write the failing tests**

```cpp
void test_sixty_seconds_at_sixty_kmh_is_one_km_row();      // row.km == 1.0 +-0.001, row.avgKmh == 60
void test_stale_reading_adds_no_distance();                 // Review Focus 2: fresh=false samples -> km stays
void test_idle_counts_engine_on_and_stopped();              // rpm 800, kmh 0 for 30 s -> idleS 30
void test_summary_kml_nan_without_fuel();
void test_recharge_profile_first_five_minutes();            // 14.4 V for 90 s then 13.4 -> peak 14.4, over 90, done after 300 s
void test_json_row_is_one_line_with_fixed_keys();           // exact string for a fixed Row
void test_json_row_reports_no_fit();                        // n=20 -> 0
```

- [ ] **Step 2: Run, see them fail.** `tools\ntest.ps1 test\test_tripsum\test_tripsum.cpp src\util\car\tripsum.cpp`
- [ ] **Step 3: Implement.** Use `%.3g`-style formatting with `snprintf`; NAN goes out as `null`.
- [ ] **Step 4: Run, see them pass.**
- [ ] **Step 5: Commit**: `feat(car): per-minute trip rows, summary and after-start recharge profile`

---

### Task 4: `fuelbook` and the fill-up phrase

**Files:**
- Create: `src/util/car/fuelbook.{h,cpp}`, `test/test_fuelbook/test_fuelbook.cpp`

**Interfaces:**
- Produces:

```cpp
namespace fuelbook {
constexpr float TANK_L = 44.0f, PRICE_MIN = 3.0f, PRICE_MAX = 15.0f;
struct Book { bool hasFull = false; float litresSinceFull = 0, pricePerL = NAN, recentKmL = NAN; };
void fillUp(Book* b, float pricePerL);               // pricePerL NAN = full tank, price unchanged
void burn(Book* b, float litres);
float rangeKm(const Book& b);                        // NAN unless hasFull and recentKmL known; >= 0
float costRs(const Book& b, float litres);           // NAN without a price
constexpr size_t RECORD_BYTES = 17;                  // NVS "fuel": flags u8, 4 floats LE
void encode(const Book& b, uint8_t out[RECORD_BYTES]);
bool decode(const uint8_t* in, size_t len, Book* b);
// "filled up at 6.49", "fill up 6,49", "full tank", "tanque cheio a 6,49": true if it is a
// fill-up phrase; *price = NAN when none was said; false *priceOk when said but out of range.
bool parsePhrase(const char* text, float* price, bool* priceOk);
}
```

- [ ] **Step 1: Write the failing tests**

```cpp
void test_no_fill_up_means_no_range();            // Review Focus 5
void test_range_after_fill_and_burn();            // fillUp(6.49), burn(10), recentKmL 12 -> (44-10)*12 = 408
void test_range_never_negative();                 // burn(50) -> 0
void test_cost_uses_price();                      // 2 L at 6.49 -> 12.98
void test_phrase_english_and_portuguese();        // "Filled up at 6.49." -> true, 6.49; "tanque cheio a 6,49" -> 6.49
void test_phrase_full_tank_without_price();       // "full tank" -> true, price NAN
void test_price_out_of_range_refused();           // "filled up at 649" -> true, priceOk false
void test_not_a_fill_phrase();                    // "purple inside" -> false
void test_record_roundtrip_and_bad_length();
```

- [ ] **Step 2: Run, see them fail.** `tools\ntest.ps1 test\test_fuelbook\test_fuelbook.cpp src\util\car\fuelbook.cpp`
- [ ] **Step 3: Implement.** Words for the phrase: `filled`/`fill`/`full`/`tank`/`abasteci`/`tanque`/`cheio`. Both `.` and `,` are decimal separators.
- [ ] **Step 4: Run, see them pass.**
- [ ] **Step 5: Commit**: `feat(car): fuel book, range and the fill-up phrase`

---

### Task 5: `triplog`: trip files that survive a power cut

**Files:**
- Create: `src/util/car/triplog.{h,cpp}`, `test/test_triplog/test_triplog.cpp`

**Interfaces:**
- Consumes: `tripsum::jsonRow` lines (Task 3).
- Produces:

```cpp
namespace triplog {
struct Store {                                   // implemented over LittleFS on the deck, in RAM in tests
    virtual bool append(const char* name, const char* data, size_t n) = 0;
    virtual long size(const char* name) = 0;     // -1 if missing
    virtual size_t read(const char* name, long at, char* out, size_t n) = 0;
    virtual bool truncate(const char* name, long len) = 0;
    virtual bool remove(const char* name) = 0;
    virtual int list(char (*names)[24], int max) = 0;   // "/t<id>.jsonl" files, oldest first
    virtual ~Store() = default;
};
constexpr long CAP_BYTES = 96 * 1024;
class Log {
public:
    explicit Log(Store& s) : s_(s) {}
    void open(const char* tripId);                     // "/t<id>.jsonl"
    void queue(const char* line);                      // RAM until flush()
    bool flush();                                      // every 30 s from the loop
    bool recover();                                    // at boot: cut a partial last line in every file
    void enforceCap();                                 // drop oldest whole files above CAP_BYTES
private: Store& s_; /* ... */
};
}
```

- [ ] **Step 1: Write the failing tests** (a `RamStore` in the test file)

```cpp
void test_flush_appends_queued_lines();
void test_cut_row_is_dropped();               // Review Focus 4: file ends "...}\n{\"t\":\"x\",\"m" -> recover() truncates to the last '\n'
void test_cap_drops_oldest_trip_first();
void test_flush_failure_keeps_queue();        // append returns false -> lines still queued, next flush writes them
```

- [ ] **Step 2: Run, see them fail.** `tools\ntest.ps1 test\test_triplog\test_triplog.cpp src\util\car\triplog.cpp`
- [ ] **Step 3: Implement.** Cap the RAM queue at 8 lines; the oldest line is dropped first, with a counter.
- [ ] **Step 4: Run, see them pass.**
- [ ] **Step 5: Commit**: `feat(car): trip log files with cut-row recovery and a size cap`

---

### Task 6: `healthview`: what screens A-D say, and the parked gate

**Files:**
- Create: `src/util/car/healthview.{h,cpp}`, `test/test_healthview/test_healthview.cpp`

**Interfaces:**
- Consumes: `carhealth::Verdict` (Task 2), `tripsum::Summary` (Task 3), `fuelbook` values (Task 4).
- Produces:

```cpp
namespace healthview {
enum class Role : uint8_t { Text, Mid, Dim, Alert };   // mapped to termpal by the deck
struct Line { char text[40]; Role role; };
struct Check { Line battery, batteryWhy, engine, engineWhy, lastTrip, range, status; bool problem; };
// Screen A/B. lastTrip from the NVS trip and fuel book; status ">ALL SYSTEMS NOMINAL" or ">1 ISSUE  [TAP FOR DETAILS]"
Check check(const carhealth::Verdict& batt, float volts, const carhealth::Verdict& eng,
            const tripsum::Summary* last, float lastCostRs, float rangeKm);
// Screen C's band: "12.4 KM", "11.8 KM/L", "296 KM LEFT" and "0:16  R$ 6,30  14.1V  90C"; "--" for NAN
struct Band { char trip[12], kmlNow[12], range[16], small[40]; };
Band band(float tripKm, float kmlNow, float rangeKm, uint32_t seconds, float costRs, float volts, float coolantC);
// Screen D
struct Parked { Line km, minutes, kml, cost, small, battery, engine, footer; };
Parked parked(const tripsum::Summary& s, float costRs, float rangeKm,
              const carhealth::Verdict& batt, const carhealth::Verdict& eng);
class ParkGate {           // show after 5 s at 0 km/h with >= 0.5 km; hide at > 3 km/h or dismiss()
public:
    bool update(float kmh, float tripKm, uint32_t nowMs);   // true while D should show
    void dismiss();                                         // a tap: hidden until the car moves again
};
}
```

R$ is written `R$ 9,40` (comma decimal). km/l and km use a dot, as in the mockups.

- [ ] **Step 1: Write the failing tests**

```cpp
void test_check_all_ok_lines();                 // status ">ALL SYSTEMS NOMINAL", problem false
void test_check_problem_marks_only_that_item(); // battery Alert, engine Text, status ">1 ISSUE  [TAP FOR DETAILS]"
void test_band_dashes_for_unknown();            // NAN range -> "-- KM LEFT"
void test_cost_uses_comma();                    // 9.4 -> "R$ 9,40"
void test_parkgate_needs_five_seconds_and_half_km();
void test_parkgate_hides_on_move_and_dismiss_holds_until_moving();
void test_lines_fit_forty_chars();              // longest inputs -> every text < 40, NUL-terminated
```

- [ ] **Step 2: Run, see them fail.** `tools\ntest.ps1 test\test_healthview\test_healthview.cpp src\util\car\healthview.cpp src\util\car\carhealth.cpp src\util\car\tripsum.cpp src\util\car\fuelbook.cpp`
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run, see them pass.**
- [ ] **Step 5: Commit**: `feat(car): screen texts for the health check and the parked gate`

---

### Task 7: Bench adapter `tools/obd_sim.py`

**Files:**
- Create: `tools/obd_sim.py`, `tools/test_obd_sim.py`; Modify: `tools/requirements.txt` (add `bless`)

**Interfaces:**
- Produces: `answer(cmd: str, state: dict) -> str`, a pure function returning the ELM327 text before `>`. The `state` dict holds `speed`, `rpm`, `volts`, `coolant`, `ltft`, `codes`, `pending`, `mil` and `silent`.
- A BLE peripheral named `IOS-Vlink` advertising service FFF0, with FFF1 notify and FFF2 write, answers each command with `answer(...) + "\r>"`.
- A small console lets the tester change the state live (`speed 60`, `volts 11.8`, `code P0301`, `silent on`).

- [ ] **Step 1: Write the failing pytest**

```python
def test_speed_reply():          assert answer("010D", {"speed": 60}) == "41 0D 3C"
def test_atrv():                 assert answer("ATRV", {"volts": 12.6}) == "12.6V"
def test_unsupported_is_no_data(): assert answer("012F", {}) == "NO DATA"
def test_clear_codes_refused():  assert answer("04", {}) == "?"
def test_supported_scan_matches_city():  # 0100 bits include 0C 0D 10 06 07; exclude 05 0F 2F
```

- [ ] **Step 2: Run, see it fail.** `python -m pytest tools/test_obd_sim.py -q`. Expected: ImportError.
- [ ] **Step 3: Implement.** The supported PIDs are the City's from the spec. The BLE part sits under `if __name__ == "__main__"`.
- [ ] **Step 4: Run, see it pass.** Same command. Expected: 5 passed.
- [ ] **Step 5: Check by hand** that the PC advertises it: `python tools/obd_sim.py`, then nRF Connect on the phone sees `IOS-Vlink` with FFF0. If Windows can't advertise a 16-bit FFF0 service with `bless`, write it down in `docs/BACKLOG.md` and use a spare ESP32 build instead (same `answer()` table ported).
- [ ] **Step 6: Commit**: `test(car): bench ELM327 over BLE with the City's replies`

---

### Architecture change (2026-10-29)

The first Task 8 showed the yellow deck can't safely hold a 4th BLE connection (the prebuilt
framework sizes the BT controller for 3; a 4-connection build crash-looped). The owner chose a third
deck, the **OBD deck** (spec "Architecture update"). Tasks 8-11 below replace the old 8-10:
- the OBD deck (env `obddeck`) reads the car and keeps health, the trip log and the fuel book;
- the yellow deck polls it over the hotspot (`GET /car`, signed) and shows screens A-D, the band and
  warnings, and forwards a spoken fill-up (`GET /fill`, signed).

The parked first attempt (yellow-deck `obdlink`, its `ledpanel.cpp` wiring and BACKLOG notes) is a
patch in the plan workspace (`wip-yellow-obd/`): Task 9 reuses its `obdlink.{h,cpp}` almost as is.

---

### Task 8: `carwire`: the `/car` snapshot format

**Files:**
- Create: `src/util/car/carwire.{h,cpp}`, `test/test_carwire/test_carwire.cpp`

**Interfaces:**
- Consumes: `carhealth::Verdict`/`Level` (Task 2), `tripsum::Summary` (Task 3).
- Produces:

```cpp
namespace carwire {
enum class Link : uint8_t { Off, Connecting, NoEngine, Live };   // obdplan::Link's values
struct Snapshot {
    Link link = Link::Off;
    bool checkReady = false;          // first codes read + a volts reading are in, or 20 s passed
    float kmh = NAN, rpm = NAN, volts = NAN, coolantC = NAN, kmlNow = NAN;
    carhealth::Verdict battery{}, engine{};          // level, known, why
    tripsum::Summary trip{};          // this power-on so far
    float tripCostRs = NAN;
    bool hasLast = false; tripsum::Summary last{}; float lastCostRs = NAN;
    float rangeKm = NAN, pricePerL = NAN;
    bool critical = false;            // carhealth::Watch::critical()
    uint32_t seq = 0;                 // +1 per snapshot: the yellow deck spots a stale reply
};
constexpr size_t MAX_JSON = 768;
size_t encode(const Snapshot& s, char* out, size_t n);   // 0 if it doesn't fit; NAN/inf -> null
bool decode(const char* json, Snapshot* s);              // false on junk or a missing "v":1; unknown keys ignored
}
```

- JSON is flat and short-keyed, with a version: `{"v":1,"seq":..,"link":"live","ready":1,"kmh":..,...}`. The `why` strings are escaped.
- Use `util/common/jsonflat` / `jsonpath` for decoding if they fit (read their headers), else a small parser here.

- [ ] **Step 1: Write the failing tests**: a full round trip (every field, NANs included); the `why` text with quotes and backslashes survives; `encode` reports no fit; `decode` rejects junk, a wrong version and a truncated body; unknown keys are ignored; the worst-case snapshot fits `MAX_JSON`.
- [ ] **Step 2: Run, see them fail.** `tools\ntest.ps1 test\test_carwire\test_carwire.cpp src\util\car\carwire.cpp`
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run, see them pass; then `tools\run_tests.ps1`.**
- [ ] **Step 5: Commit**: `feat(car): the /car snapshot format`

---

### Task 9: The OBD deck firmware (`obddeck`)

**Files:**
- Create: `src/obd/main.cpp` (setup/loop), `src/obd/obdlink.{h,cpp}` (from `wip-yellow-obd/`, any `ledpanel`-isms removed), `src/obd/carfs.{h,cpp}` (the LittleFS `triplog::Store`)
- Modify: `platformio.ini`: new `[env:obddeck]` (`board = esp32dev`, NimBLE-Arduino 2.5.1 with the default 3 connections, `min_spiffs.csv`, `build_src_filter = -<*> +<obd/> +<common/> +<util/car/> +<util/common/> +<util/wire/>` or whatever `common/` needs) and `[env:obddeck_ota]` (espota to `cardeck-obd.local`, `tools/ota_auth.py`), mirroring `ledpanel` / `ledpanel_ota`.
- Modify: `docs/ARCHITECTURE.md` (a short "OBD deck" section) and `docs/BACKLOG.md` ("Car-health: waiting on the adapter" checks, from the parked patch, plus the OBD deck's first flash).

**What it does:**
- **WiFi** like the yellow deck (`common/net/wifi`, the same networks, mDNS `cardeck-obd`), SNTP clock, OTA (`common/net/ota`), boot guard, watchdog (`common/core/watchdog.h`: loop + every task).
- **The OBD link** (`obdlink`) with every rule from the parked attempt and the branch reviews:
  - connect from one place, backoff 5→60 s;
  - every write through `obd::allowed()`, never ATH1;
  - `setSupported()` after the scan even on failure;
  - ATRV at least every ~5 s;
  - `fueleco` stepped only with fresh values;
  - `obdble` pairing within one service;
  - the address in NVS (`bleaddr`/`bletype`/`blename`).
- **Each second**: a `tripsum::Sample` from the fresh values (litres = NAN when fueleco counted nothing), `carhealth::Watch::feed()`, `fuelbook::burn()`; `Watch::codesRead()` after a good 03/07 read.
- **Each minute**: the closed row to `triplog`, `fuelbook::learn()` with it.
- **Every 30 s**: `triplog::flush()`; the last-trip NVS record (`{km, litres, seconds, idleS, avgKmh, costRs}` + the `carhealth::Memo`) and the fuel book (`fuelbook::encode`) when they changed.
- **Boot**:
  - boot sequence `tseq` +1;
  - `triplog` `recover()` + `enforceCap()`, then `open(seq)`;
  - restore the Memo and call `endTrip()`;
  - load the last trip and the fuel book.
- **`GET /car`**: `carwire::encode` of the latest snapshot. **`GET /fill`**: `p` optional → `fuelbook::fillUp`, NVS saved at once. Both require a valid signature (`common/net/reqauth`); no bearer key between decks.
- **The LittleFS `Store`**:
  - `list(names, max, skip)` in ascending name order (= oldest first);
  - `append` truncates back to the pre-append size on a short write;
  - `triplog::Log` is a static object.
- **Serial log lines**: `[obd] …`, `[car] …` (a one-line snapshot every 10 s), `[mem] …`.

- [ ] **Step 1: Build** `pio run -e obddeck` → SUCCESS, no `-Werror` findings; `pio run -e ledpanel` still builds.
- [ ] **Step 2: Host suites** `tools\run_tests.ps1` → all pass.
- [ ] **Step 3: On a board, if one is plugged in** (never the yellow deck's COM13 or the black deck's COM12; the owner names the port): flash it, check the boot log, `/car` answering a signed request, the OBD backoff to 60 s with no adapter, no watchdog reset. Otherwise write these as "first flash" checks in `docs/BACKLOG.md`.
- [ ] **Step 4: Commit**: `feat(obd): the OBD deck reads the car, keeps health, the trip log and the fuel book`

---

### Task 10: Yellow deck: the `/car` client and screens A/B and D

**Files:**
- Modify: `src/util/yellow/deckmenu.{h,cpp}` + `test/test_deckmenu`: `Screen::Check`, `Screen::Parked`, one full-screen tap rect each (`{0,0,320,240}`), every other id `{0,0,0,0}`.
- Modify: `src/yellow/ledpanel.cpp`: a car task (like the lyrics task: signed `GET /car` every 1 s while it answers, backing off to 10 s while it doesn't; `watchdog::watch()`/`feed()`), `drawCheck()`, `drawParked()`, `show()` for both, taps and Xbox A/B dismiss them.
- Modify: `platformio.ini` `[env:ledpanel]`: `+<util/car/>` in `build_src_filter` (healthview, carwire and what they include). No NimBLE change.
- Modify: `src/util/yellow/deckaddr.h` only if the OBD deck's address needs the same fixed-then-mDNS lookup the black deck has.

**Interfaces:**
- Consumes: `carwire::decode` (Task 8), `healthview::check/parked/ParkGate` (Task 6).
- **Screen A**:
  - shown once per power-on, when a snapshot first has `checkReady`;
  - after 5 s it goes back to the Menu, unless something is a problem: then it holds until a tap;
  - it never interrupts Lights/Karaoke/Presets.
- **Screen D**:
  - `ParkGate` fed from the snapshot's `kmh` and trip km, only from the Menu;
  - `dismiss()` only while D shows;
  - a stale snapshot (no new `seq` for 3 s) counts as "not stopped".
- **No OBD deck**: nothing changes from today's behaviour (Review Focus 1).

- [ ] **Step 1: Failing deckmenu test** `test_check_and_parked_are_one_full_screen_tap`; run, fail; implement; run, pass.
- [ ] **Step 2: Draw both screens** from the `healthview` lines (SMALL/BODY/BIG, `pal()` roles) following the mockups, with a `-D HEALTH_TEST=<a|b|d>` hook that draws each from a canned snapshot (add it to `docs/ARCHITECTURE.md` "Test hooks").
- [ ] **Step 3: On the yellow deck (COM13)**: flash with `HEALTH_TEST=a`, `b`, `d` in turn; read the serial log, and ask the owner to look at each screen (or photograph it) and compare with the mockups; fix any text past 316 px. Then flash the normal build and check that with no OBD deck nothing changes and the car task backs off.
- [ ] **Step 4: Commit**: `feat(leds): pre-flight check and trip-complete screens from the OBD deck`

---

### Task 11: Yellow deck: band, warnings and the voice fill-up

**Files:**
- Modify: `src/yellow/ledpanel.cpp`:
  - `drawTripBand()` draws `healthview::band` from the snapshot (`--` with no OBD deck);
  - the trip meter's distance comes from the snapshot when one is fresh;
  - warnings go to the prompt line: `setMessage(..., MESSAGE_MS)` with ALERT, only for `critical` while moving, any problem when stopped;
  - `handleSay()`: after `reqauth::pass`, `fuelbook::parsePhrase` before `ledvoice`. A fill-up phrase is forwarded as a signed `GET /fill` (with `p` when a valid price was heard) and answered with the replies below.
- Modify: `src/util/yellow/termlayout.h` only if the band needs new x positions (`test_termfont` must pass).
- Modify: `docs/BACKLOG.md` (car-test checklist) and `docs/yellow-deck.md` (the fill-up phrase).
- Wave 4 changed `ledvoice.h`, `lightstatus`, `termlayout.h` and `ledpanel.cpp`: re-read them first.

**Voice replies:**
- `"Full tank noted, 6.49 per litre."`
- `"Full tank noted. I heard 649, that's not a price."`
- `"Full tank noted."` (no price said)
- `"I can't reach the car module."` (the OBD deck didn't answer)

- [ ] **Step 1: Build** `pio run -e ledpanel` and `pio run -e obddeck` → SUCCESS; `tools\run_tests.ps1` → all pass.
- [ ] **Step 2: On the yellow deck (COM13)**:
  - the band shows `--` with no OBD deck;
  - `/say?text=filled%20up%20at%206.49&k=…` answers "I can't reach the car module.";
  - the lights, voice and KARAOKE behave as before.

  (If the owner can reach the deck from the PC; otherwise list it in BACKLOG.)
- [ ] **Step 3: Car-test checklist** in `docs/BACKLOG.md`:
  1. The OBD deck connects to the Vgate within 30 s of the start.
  2. Screen A appears with real volts.
  3. km matches the car's trip meter within 2 %.
  4. km/l is within 10 % of the car's display.
  5. Screen D appears at a stop.
  6. Voice fill-up at the pump works.
  7. The LEDs, the pad and KARAOKE keep working.
  8. A one-week log on the OBD deck shows no row gaps beyond link drops.
- [ ] **Step 4: Commit**: `feat(leds): trip band, warnings and the voice fill-up via the OBD deck`

---

## After this plan

- **First flash of the OBD deck** and the **owner's car test** (checklists in `docs/BACKLOG.md`), then the merge decision.
- **Plan 2:** upload from the OBD deck (it holds the log) to the private `city-telemetry` repo, using the `tripsum::jsonRow` format fixed here.
- **Plan 3:** `tools/car_checkup.py`, the daily routine, and `checkup.json` back to screens A/B.
- **Later:** always-on OBD deck from pin 16 = the battery logger (resting voltage, crank dip, parked drain).
