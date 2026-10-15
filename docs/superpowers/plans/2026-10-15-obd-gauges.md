# OBD-II Gauges Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The old deck (`cyd_companion`) reads live engine data from a Bluetooth Classic ELM327 and shows it as Pokémon HP-bar gauges while the adapter answers; otherwise it stays on Zero Two.

**Architecture:** A pure parser (`util/obd`) and a pure scheduler/health/colour module (`util/obdplan`), host-tested. An Arduino task (`companion/obdlink`) owns `BluetoothSerial`, discovers and connects to the adapter, asks PIDs in the scheduler's order and publishes a snapshot under a lock. A drawing module (`companion/gauges`) renders the snapshot; `companion/app.cpp` switches between her screens and the gauges.

**Tech Stack:** ESP32 Arduino core 2.0.17 (`BluetoothSerial`, FreeRTOS), TFT_eSPI 2.5.43, Unity host tests via `tools/ntest.ps1` / `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-15-obd-gauges-design.md`

## Global Constraints

- Build `cyd_companion` only; `s3` and `cyd` must not change behaviour or size (Bluetooth code lives in `src/companion/`, which they exclude).
- Flash under 97 % for `cyd_companion` (`python tools/check_flash.py .pio/build/cyd_companion/firmware.bin 0x300000 97`), and `cyd` stays under 97 %.
- `src/util/` is pure: no Arduino, ESP or `ui/` includes; every util module gets a Unity suite in `test/test_<name>/test_<name>.cpp`.
- No new library dependency: `BluetoothSerial` ships with the core.
- Read-only: send only `AT...` commands and mode `01` requests. Never mode `03`/`04` (fault codes / clear).
- `src/config.h` is git-ignored and holds secrets: never print or commit it. New options get defaults via `#ifndef` and a commented line in `include/config.example.h`.
- Conventional commits ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Values: RPM `010C` (256A+B)/4; speed `010D` A; coolant `0105` A−40; load `0104` A·100/255; intake `010F` A−40; throttle `0111` A·100/255; battery `ATRV`.
- Timing: slow PIDs every 2000 ms; unsupported (`NO DATA`) retried every 30000 ms; 3 failed requests in a row = adapter gone; reconnect every 30000 ms; `NoEngine` asks again every 5000 ms; gauges leave 5000 ms after the link leaves `Live`; a value older than 3000 ms is drawn dimmed.
- Bands: RPM green < 70 % of redline ≤ yellow < 90 % ≤ red (redline default 6500); coolant blue < 60 °C, green ≤ 105 °C, red above; battery red < 11.8 V, yellow < 12.2 V, else green.

## Review Focus

1. A CAN adapter that keeps headers on (`7E8 03 41 0D 32`) or two ECUs answering (two `41 0C` lines): the value still parses from the first matching line — tested in Task 1.
2. A reply cut off before its data bytes (`41 0C 1A`): an error, never a wrong number — tested in Task 1.
3. Ignition turned off while live (`UNABLE TO CONNECT` / `NO DATA` everywhere): the link goes `NoEngine`, gauges leave after 5 s, Zero Two returns — tested in Task 2 (Health + Hold).
4. `millis()` wrapping during a 30 s back-off or the 5 s hold — tested in Task 2.
5. Every PID unsupported at once (only `ATRV` answers): the scheduler still returns a request and never spins — tested in Task 2.

---

### Task 1: ELM327 request/reply parser (`util/obd`)

**Files:**
- Create: `src/util/obd.h`, `src/util/obd.cpp`
- Test: `test/test_obd/test_obd.cpp`

**Interfaces:**
- Produces:
  - `enum class obd::Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, COUNT };`
  - `enum class obd::Reply : uint8_t { Ok, NoData, NoEngine, Error };`
  - `const char* obd::request(Pid p);` — the command with its trailing `\r`
  - `obd::Reply obd::parse(Pid p, const char* raw, float* value);` — `raw` is everything read up to (not including) `>`
  - `bool obd::looksLikeAdapter(const char* btName);`

- [ ] **Step 1: Write the failing test**

`test/test_obd/test_obd.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/obd.h"

using obd::Pid;
using obd::Reply;

void setUp() {}
void tearDown() {}

static Reply parse(Pid p, const char* raw, float* v) { *v = -999.0f; return obd::parse(p, raw, v); }

void test_requests_end_with_cr_and_are_read_only() {
    TEST_ASSERT_EQUAL_STRING("010C\r", obd::request(Pid::Rpm));
    TEST_ASSERT_EQUAL_STRING("ATRV\r", obd::request(Pid::Battery));
    for (int i = 0; i < (int)Pid::COUNT; ++i) {          // mode 01 or AT only: never 03 / 04
        const char* r = obd::request((Pid)i);
        TEST_ASSERT_TRUE(strncmp(r, "01", 2) == 0 || strncmp(r, "AT", 2) == 0);
    }
}

void test_formulas_at_their_ends() {
    struct { Pid p; const char* raw; float want; } rows[] = {
        {Pid::Rpm, "41 0C 00 00", 0.0f},          {Pid::Rpm, "41 0C FF FF", 16383.75f},
        {Pid::Rpm, "41 0C 0B B8", 750.0f},        {Pid::Speed, "41 0D FF", 255.0f},
        {Pid::Coolant, "41 05 00", -40.0f},       {Pid::Coolant, "41 05 FF", 215.0f},
        {Pid::Load, "41 04 FF", 100.0f},          {Pid::Intake, "41 0F 3C", 20.0f},
        {Pid::Throttle, "41 11 00", 0.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)parse(r.p, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.want, v, r.raw);
    }
}

void test_what_clones_wrap_around_a_reply() {
    const char* forms[] = {
        "410C0BB8",                                  // ATS0: no spaces
        "010C\n41 0C 0B B8",                         // echo still on
        "SEARCHING...\n41 0C 0B B8",                 // first request after ATSP0
        "BUS INIT: ...OK\n41 0C 0B B8\n",            // K-line init
        "7E8 04 41 0C 0B B8",                        // CAN headers on
        "41 0C 0B B8\n41 0C 0B B9",                  // two ECUs: the first wins
        "\n\n41 0c 0b b8  \n",                       // blank lines, lower case
    };
    for (const char* raw : forms) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)parse(Pid::Rpm, raw, &v), raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 750.0f, v, raw);
    }
}

void test_errors_never_give_a_number() {
    struct { const char* raw; Reply want; } rows[] = {
        {"NO DATA", Reply::NoData},
        {"SEARCHING...\nUNABLE TO CONNECT", Reply::NoEngine},
        {"CAN ERROR", Reply::NoEngine},
        {"BUS INIT: ...ERROR", Reply::NoEngine},
        {"STOPPED", Reply::Error},
        {"?", Reply::Error},
        {"", Reply::Error},
        {"41 0C 1A", Reply::Error},                  // cut off before its second byte
        {"41 0D 32", Reply::Error},                  // a reply for another PID
        {"ELM327 v1.5", Reply::Error},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)parse(Pid::Rpm, r.raw, &v), r.raw);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-999.0f, v, r.raw);   // untouched
    }
}

void test_battery_voltage_forms() {
    struct { const char* raw; Reply want; float v; } rows[] = {
        {"12.6V", Reply::Ok, 12.6f}, {"ATRV\n14.1V", Reply::Ok, 14.1f}, {" 9V ", Reply::Ok, 9.0f},
        {"V", Reply::Error, -999.0f}, {"?", Reply::Error, -999.0f}, {"", Reply::Error, -999.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)parse(Pid::Battery, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.v, v, r.raw);
    }
}

void test_adapter_names() {
    const char* yes[] = {"OBDII", "OBD2", "obd-ii", "V-LINK", "ELM327", "Android-Vlink", "IOS-Vlink"};
    const char* no[] = {"JBL Flip 5", "iPhone", "", nullptr};
    for (const char* n : yes) TEST_ASSERT_TRUE_MESSAGE(obd::looksLikeAdapter(n), n);
    for (const char* n : no) TEST_ASSERT_FALSE(obd::looksLikeAdapter(n));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_requests_end_with_cr_and_are_read_only);
    RUN_TEST(test_formulas_at_their_ends);
    RUN_TEST(test_what_clones_wrap_around_a_reply);
    RUN_TEST(test_errors_never_give_a_number);
    RUN_TEST(test_battery_voltage_forms);
    RUN_TEST(test_adapter_names);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_obd/test_obd.cpp src/util/obd.cpp`
Expected: COMPILE FAILED (`obd.h` not found)

- [ ] **Step 3: Write the implementation**

`src/util/obd.h`:

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
// ELM327 requests and replies for the old deck's live gauges (companion/obdlink): mode 01 PIDs
// and the adapter's own battery reading. Read-only by design: nothing here can send mode 03/04.
// Tolerates what cheap clones wrap around a reply (echo, SEARCHING..., CAN headers, several
// ECUs, spaces or none). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obd {
enum class Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, COUNT };
// Ok: *value set. NoData: the car doesn't support this PID. NoEngine: the adapter is up but the
// car doesn't answer (ignition off). Error: anything else (timeout, junk, cut-off reply).
enum class Reply : uint8_t { Ok, NoData, NoEngine, Error };
const char* request(Pid p);          // the command, ending in '\r'
// raw: everything the adapter sent before its '>' prompt. *value is touched only on Ok.
Reply parse(Pid p, const char* raw, float* value);
// A Bluetooth name that is probably an ELM327 (OBD, ELM, V-LINK / VLINK, any case).
bool looksLikeAdapter(const char* btName);
}
```

`src/util/obd.cpp`:

```cpp
#include "obd.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace obd {
namespace {
struct Def { const char* cmd; uint8_t pid; uint8_t bytes; };
// Order matches Pid. pid/bytes are unused for Battery (ATRV).
constexpr Def DEFS[(int)Pid::COUNT] = {
    {"010C\r", 0x0C, 2}, {"010D\r", 0x0D, 1}, {"0105\r", 0x05, 1}, {"0104\r", 0x04, 1},
    {"010F\r", 0x0F, 1}, {"0111\r", 0x11, 1}, {"ATRV\r", 0, 0},
};

bool contains(const char* s, const char* word) { return strstr(s, word) != nullptr; }

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)toupper((unsigned char)c);
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

float value(Pid p, const uint8_t* b) {
    switch (p) {
    case Pid::Rpm: return (256.0f * b[0] + b[1]) / 4.0f;
    case Pid::Speed: return b[0];
    case Pid::Coolant:
    case Pid::Intake: return (float)b[0] - 40.0f;
    case Pid::Load:
    case Pid::Throttle: return b[0] * 100.0f / 255.0f;
    default: return 0.0f;
    }
}

// One line, hex digits only (spaces dropped, upper case): finds "41"+PID and reads the bytes
// after it. False when the line has no such reply or it is cut off.
bool fromLine(const char* line, size_t len, Pid p, float* out) {
    char hex[96];
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < sizeof hex; ++i) {
        if (line[i] == ' ') continue;
        if (hexVal(line[i]) < 0) return false;            // not a data line (SEARCHING..., ELM327)
        hex[n++] = (char)toupper((unsigned char)line[i]);
    }
    hex[n] = '\0';
    char want[5];
    static const char* digits = "0123456789ABCDEF";
    const uint8_t pid = DEFS[(int)p].pid;
    want[0] = '4'; want[1] = '1'; want[2] = digits[pid >> 4]; want[3] = digits[pid & 15]; want[4] = 0;
    const char* at = strstr(hex, want);
    if (!at) return false;
    at += 4;
    uint8_t b[2];
    for (int k = 0; k < DEFS[(int)p].bytes; ++k) {
        const int hi = hexVal(at[2 * k]), lo = hi < 0 ? -1 : hexVal(at[2 * k + 1]);
        if (hi < 0 || lo < 0) return false;               // cut off
        b[k] = (uint8_t)(hi << 4 | lo);
    }
    *out = value(p, b);
    return true;
}

Reply battery(const char* raw, float* out) {
    for (const char* s = raw; *s; ++s) {
        if (!isdigit((unsigned char)*s)) continue;
        char* end = nullptr;
        const float v = strtof(s, &end);
        while (end && *end == ' ') ++end;
        if (end && (*end == 'V' || *end == 'v')) { *out = v; return Reply::Ok; }
        s = end ? end - 1 : s;
    }
    return Reply::Error;
}
}

const char* request(Pid p) { return (int)p < (int)Pid::COUNT ? DEFS[(int)p].cmd : "ATRV\r"; }

Reply parse(Pid p, const char* raw, float* value) {
    if (!raw) return Reply::Error;
    if (p == Pid::Battery) return battery(raw, value);
    if (contains(raw, "UNABLE TO CONNECT") || contains(raw, "CAN ERROR") || contains(raw, "BUS ERROR") ||
        (contains(raw, "BUS INIT") && contains(raw, "ERROR")))
        return Reply::NoEngine;
    if (contains(raw, "NO DATA")) return Reply::NoData;
    for (const char* line = raw; *line;) {
        const char* end = strchr(line, '\n');
        const size_t len = end ? (size_t)(end - line) : strlen(line);
        float v;
        if (fromLine(line, len, p, &v)) { *value = v; return Reply::Ok; }
        if (!end) break;
        line = end + 1;
    }
    return Reply::Error;
}

bool looksLikeAdapter(const char* btName) {
    if (!btName || !btName[0]) return false;
    char up[40];
    size_t n = 0;
    for (; btName[n] && n + 1 < sizeof up; ++n) up[n] = (char)toupper((unsigned char)btName[n]);
    up[n] = '\0';
    return contains(up, "OBD") || contains(up, "ELM") || contains(up, "VLINK") || contains(up, "V-LINK");
}
}
```

Note on `fromLine`: a line ending in `\r` would contain a non-hex `\r`; the link (Task 4) turns `\r` into `\n` before parsing, so lines never carry `\r`.

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_obd/test_obd.cpp src/util/obd.cpp`
Expected: `6 Tests 0 Failures 0 Ignored`, `OK`

- [ ] **Step 5: Commit**

```bash
git add src/util/obd.h src/util/obd.cpp test/test_obd/test_obd.cpp
git commit -m "feat(obd): ELM327 request and reply parser

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Scheduler, link health, screen hold and colour bands (`util/obdplan`)

**Files:**
- Create: `src/util/obdplan.h`, `src/util/obdplan.cpp`
- Test: `test/test_obdplan/test_obdplan.cpp`

**Interfaces:**
- Consumes: `obd::Pid`, `obd::Reply` (Task 1)
- Produces:
  - `enum class obdplan::Link : uint8_t { Off, Connecting, NoEngine, Live };`
  - `class obdplan::Plan { public: obd::Pid next(uint32_t nowMs); void done(obd::Pid p, obd::Reply r, uint32_t nowMs); };`
  - `class obdplan::Health { public: Link reply(obd::Reply r); bool lost() const; Link state() const; };`
  - `class obdplan::Hold { public: bool update(bool live, uint32_t nowMs); };`
  - `enum class obdplan::Band : uint8_t { Cold, Ok, Warn, Hot };`
  - `Band obdplan::rpmBand(float rpm, float redline); Band obdplan::coolantBand(float c); Band obdplan::batteryBand(float v);`
  - `float obdplan::frac(float v, float lo, float hi);`
  - constants `SLOW_MS = 2000`, `RETRY_MS = 30000`, `HOLD_MS = 5000`, `LOST_AFTER = 3`

- [ ] **Step 1: Write the failing test**

`test/test_obdplan/test_obdplan.cpp`:

```cpp
#include <unity.h>
#include <cstdint>
#include "../../src/util/obdplan.h"

using obd::Pid;
using obd::Reply;
using obdplan::Band;
using obdplan::Link;

void setUp() {}
void tearDown() {}

static bool isFast(Pid p) { return p == Pid::Rpm || p == Pid::Speed; }

void test_fast_pair_every_cycle_slow_ones_in_turn() {
    obdplan::Plan plan;
    uint32_t t = 1000;
    int slow = 0, fast = 0;
    bool seen[(int)Pid::COUNT] = {};
    for (int i = 0; i < 200; ++i, t += 100) {            // 20 s of 100 ms requests
        const Pid p = plan.next(t);
        seen[(int)p] = true;
        (isFast(p) ? fast : slow)++;
        plan.done(p, Reply::Ok, t);
    }
    TEST_ASSERT_INT_WITHIN(2, 10, slow);                 // one slow request per SLOW_MS
    TEST_ASSERT_EQUAL_INT(200 - slow, fast);
    for (int i = 0; i < (int)Pid::COUNT; ++i) TEST_ASSERT_TRUE(seen[i]);   // all of them, in turn
}

void test_unsupported_pid_backs_off_then_is_asked_again() {
    obdplan::Plan plan;
    uint32_t t = 0xFFFFF000u;                             // the back-off crosses the millis() wrap
    bool marked = false;
    uint32_t noDataAt = 0;
    for (int i = 0; i < 600; ++i, t += 100) {            // 60 s of 100 ms requests
        const Pid p = plan.next(t);
        if (p == Pid::Speed) {
            if (!marked) {                               // the car says NO DATA once
                marked = true;
                noDataAt = t;
                plan.done(p, Reply::NoData, t);
                continue;
            }
            // asked again only once RETRY_MS has passed, at the next fast turn after it
            TEST_ASSERT_UINT32_WITHIN(500, obdplan::RETRY_MS + 400, t - noDataAt);
            return;
        }
        plan.done(p, Reply::Ok, t);
    }
    TEST_FAIL_MESSAGE("speed was never asked again");
}

void test_every_pid_unsupported_still_gives_a_request() {
    obdplan::Plan plan;
    uint32_t t = 0;
    for (int i = 0; i < 100; ++i, t += 50) {
        const Pid p = plan.next(t);
        TEST_ASSERT_TRUE((int)p < (int)Pid::COUNT);
        plan.done(p, p == Pid::Battery ? Reply::Ok : Reply::NoData, t);
    }
}

void test_health_states() {
    obdplan::Health h;
    TEST_ASSERT_EQUAL_INT((int)Link::Connecting, (int)h.state());
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::Ok));
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::NoData));   // the car answered
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)h.reply(Reply::NoEngine));
    h.reply(Reply::Error);
    h.reply(Reply::Error);
    TEST_ASSERT_FALSE(h.lost());
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)h.reply(Reply::Ok));       // errors reset
    for (int i = 0; i < obdplan::LOST_AFTER; ++i) h.reply(Reply::Error);
    TEST_ASSERT_TRUE(h.lost());
}

void test_hold_shows_at_once_and_leaves_after_5_s_off() {
    obdplan::Hold hold;
    uint32_t t = 0xFFFFFF00u;                             // across the millis() wrap
    TEST_ASSERT_FALSE(hold.update(false, t));
    TEST_ASSERT_TRUE(hold.update(true, t += 10));
    TEST_ASSERT_TRUE(hold.update(false, t += 10));
    TEST_ASSERT_TRUE(hold.update(true, t += 4000));       // a blip: still shown, timer resets
    const uint32_t live = t;
    TEST_ASSERT_TRUE(hold.update(false, live + 10));
    TEST_ASSERT_TRUE(hold.update(false, live + obdplan::HOLD_MS));
    TEST_ASSERT_FALSE(hold.update(false, live + obdplan::HOLD_MS + 1));
}

void test_colour_bands_at_their_edges() {
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::rpmBand(4500, 6500));     // 69 %
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::rpmBand(4600, 6500));   // 71 %
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::rpmBand(5800, 6500));   // 89 %
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::rpmBand(5900, 6500));    // 91 %
    TEST_ASSERT_EQUAL_INT((int)Band::Cold, (int)obdplan::coolantBand(59.9f));
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::coolantBand(105.0f));
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::coolantBand(105.1f));
    TEST_ASSERT_EQUAL_INT((int)Band::Hot, (int)obdplan::batteryBand(11.79f));
    TEST_ASSERT_EQUAL_INT((int)Band::Warn, (int)obdplan::batteryBand(12.19f));
    TEST_ASSERT_EQUAL_INT((int)Band::Ok, (int)obdplan::batteryBand(12.2f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(-5, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, obdplan::frac(150, 0, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, obdplan::frac(5, 10, 10));   // empty range: no division by 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fast_pair_every_cycle_slow_ones_in_turn);
    RUN_TEST(test_unsupported_pid_backs_off_then_is_asked_again);
    RUN_TEST(test_every_pid_unsupported_still_gives_a_request);
    RUN_TEST(test_health_states);
    RUN_TEST(test_hold_shows_at_once_and_leaves_after_5_s_off);
    RUN_TEST(test_colour_bands_at_their_edges);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_obdplan/test_obdplan.cpp src/util/obdplan.cpp src/util/obd.cpp`
Expected: COMPILE FAILED (`obdplan.h` not found)

- [ ] **Step 3: Write the implementation**

`src/util/obdplan.h`:

```cpp
#pragma once
#include <cstdint>
#include "obd.h"
// The old deck's OBD gauges: which request goes next, whether the adapter is still there, when
// the gauges show, and the bars' colours. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdplan {
constexpr uint32_t SLOW_MS = 2000;     // one slow value per this long; RPM and speed otherwise
constexpr uint32_t RETRY_MS = 30000;   // a PID the car doesn't support is asked again after this
constexpr uint32_t HOLD_MS = 5000;     // the gauges stay this long after the link leaves Live
constexpr int LOST_AFTER = 3;          // failed requests in a row: the adapter is gone

enum class Link : uint8_t { Off, Connecting, NoEngine, Live };

class Plan {
public:
    obd::Pid next(uint32_t nowMs);
    void done(obd::Pid p, obd::Reply r, uint32_t nowMs);
private:
    bool blocked(obd::Pid p, uint32_t nowMs) const;
    bool started_ = false, fastSpeed_ = false;
    uint8_t slowNext_ = 0;
    uint32_t lastSlowMs_ = 0;
    bool noData_[(int)obd::Pid::COUNT] = {};
    uint32_t noDataAt_[(int)obd::Pid::COUNT] = {};
};

// Ok / NoData: Live (the car answered). NoEngine: NoEngine. Error: counted; LOST_AFTER in a row
// and lost() is true (the state stays as it was).
class Health {
public:
    Link reply(obd::Reply r);
    bool lost() const { return errors_ >= LOST_AFTER; }
    Link state() const { return state_; }
private:
    Link state_ = Link::Connecting;
    int errors_ = 0;
};

// Gauges on screen: at once when live, until live has been false for HOLD_MS.
class Hold {
public:
    bool update(bool live, uint32_t nowMs);
private:
    bool shown_ = false;
    uint32_t lastLiveMs_ = 0;
};

enum class Band : uint8_t { Cold, Ok, Warn, Hot };
Band rpmBand(float rpm, float redline);   // Ok < 70 % <= Warn < 90 % <= Hot
Band coolantBand(float c);                // Cold < 60 C, Ok <= 105 C, else Hot
Band batteryBand(float v);                // Hot < 11.8 V, Warn < 12.2 V, else Ok
float frac(float v, float lo, float hi);  // clamped to 0..1; 0 for an empty range
}
```

`src/util/obdplan.cpp`:

```cpp
#include "obdplan.h"

namespace obdplan {
namespace {
constexpr obd::Pid SLOW[] = {obd::Pid::Coolant, obd::Pid::Load, obd::Pid::Intake, obd::Pid::Throttle,
                             obd::Pid::Battery};
constexpr int SLOW_N = (int)(sizeof SLOW / sizeof SLOW[0]);
}

bool Plan::blocked(obd::Pid p, uint32_t nowMs) const {
    return noData_[(int)p] && nowMs - noDataAt_[(int)p] < RETRY_MS;
}

obd::Pid Plan::next(uint32_t nowMs) {
    if (!started_ || nowMs - lastSlowMs_ >= SLOW_MS) {   // a slow value's turn
        started_ = true;
        lastSlowMs_ = nowMs;
        for (int k = 0; k < SLOW_N; ++k) {
            const obd::Pid p = SLOW[(slowNext_ + k) % SLOW_N];
            if (!blocked(p, nowMs)) {
                slowNext_ = (uint8_t)((slowNext_ + k + 1) % SLOW_N);
                return p;
            }
        }
    }
    for (int k = 0; k < 2; ++k) {                        // RPM and speed take turns
        const obd::Pid p = fastSpeed_ ? obd::Pid::Speed : obd::Pid::Rpm;
        fastSpeed_ = !fastSpeed_;
        if (!blocked(p, nowMs)) return p;
    }
    return obd::Pid::Battery;   // everything else unsupported: the adapter's own reading
}

void Plan::done(obd::Pid p, obd::Reply r, uint32_t nowMs) {
    if (r == obd::Reply::NoData) {
        noData_[(int)p] = true;
        noDataAt_[(int)p] = nowMs;
    } else if (r == obd::Reply::Ok) {
        noData_[(int)p] = false;
    }
}

Link Health::reply(obd::Reply r) {
    if (r == obd::Reply::Error) {
        ++errors_;
        return state_;
    }
    errors_ = 0;
    state_ = r == obd::Reply::NoEngine ? Link::NoEngine : Link::Live;
    return state_;
}

bool Hold::update(bool live, uint32_t nowMs) {
    if (live) {
        shown_ = true;
        lastLiveMs_ = nowMs;
    } else if (shown_ && nowMs - lastLiveMs_ > HOLD_MS) {
        shown_ = false;
    }
    return shown_;
}

Band rpmBand(float rpm, float redline) {
    const float f = frac(rpm, 0.0f, redline);
    return f < 0.7f ? Band::Ok : f < 0.9f ? Band::Warn : Band::Hot;
}
Band coolantBand(float c) { return c < 60.0f ? Band::Cold : c <= 105.0f ? Band::Ok : Band::Hot; }
Band batteryBand(float v) { return v < 11.8f ? Band::Hot : v < 12.2f ? Band::Warn : Band::Ok; }

float frac(float v, float lo, float hi) {
    if (!(hi > lo)) return 0.0f;
    const float f = (v - lo) / (hi - lo);
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_obdplan/test_obdplan.cpp src/util/obdplan.cpp src/util/obd.cpp`
Expected: `6 Tests 0 Failures 0 Ignored`, `OK`. If `test_unsupported_pid_backs_off_then_is_asked_again`'s last assertion is off by one cycle, fix the test's search loop, not the 30 s rule (the rule is the spec).

- [ ] **Step 5: Commit**

```bash
git add src/util/obdplan.h src/util/obdplan.cpp test/test_obdplan/test_obdplan.cpp
git commit -m "feat(obd): request schedule, adapter health, gauge hold and colour bands

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Her lines for the car (`util/avatarlines`)

**Files:**
- Modify: `src/util/avatarlines.h` (enum `Event`, `REACT_N`), `src/util/avatarlines.cpp` (`REACT`)
- Test: `test/test_avatarlines/test_avatarlines.cpp` (the reactions test)

**Interfaces:**
- Produces: `avatarlines::Event::CarOn`, `avatarlines::Event::CarOff` (reactions: always speak, fixed line)

- [ ] **Step 1: Extend the reactions test (failing)**

In `test/test_avatarlines/test_avatarlines.cpp`, the reactions test builds `const Event evs[] = {Event::Howl, Event::Vroom, Event::TripReset, Event::Busy, Event::Unreachable};` and checks each against `REACT[i]`. Change that array to:

```cpp
    const Event evs[] = {Event::Howl, Event::Vroom, Event::TripReset, Event::Busy, Event::Unreachable,
                         Event::CarOn, Event::CarOff};
```

and, after the existing `TEST_ASSERT_EQUAL_STRING(... REACT[3].text)` line, add:

```cpp
    TEST_ASSERT_EQUAL_STRING("Car connected, darling! Let's see what she can do~", avatarlines::REACT[5].text);
    TEST_ASSERT_EQUAL_STRING("The car's asleep~", avatarlines::REACT[6].text);
```

Make sure the loop bound over `evs` uses `sizeof evs / sizeof evs[0]` (not a literal 5).

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_avatarlines/test_avatarlines.cpp src/util/avatarlines.cpp`
Expected: COMPILE FAILED (`CarOn` is not a member of `Event`)

- [ ] **Step 3: Implement**

`src/util/avatarlines.h`: change the enum and count:

```cpp
enum class Event : uint8_t { Boot, Track, Howl, Vroom, TripReset, Busy, Unreachable, CarOn, CarOff };
```
```cpp
constexpr int BOOT_N = 3, TRACK_N = 12, REACT_N = 7;
```
and the comment on `REACT`: `// Howl, Vroom, TripReset, Busy, Unreachable, CarOn, CarOff`.

`src/util/avatarlines.cpp`: append to `REACT` (moods as used by the others):

```cpp
    {Happy, "Car connected, darling! Let's see what she can do~"},
    {Talk, "The car's asleep~"},
```

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_avatarlines/test_avatarlines.cpp src/util/avatarlines.cpp`
Expected: all tests pass, `OK`

- [ ] **Step 5: Commit**

```bash
git add src/util/avatarlines.h src/util/avatarlines.cpp test/test_avatarlines/test_avatarlines.cpp
git commit -m "feat(avatar): her lines when the car connects and sleeps

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The Bluetooth link (`companion/obdlink`) and its config

**Files:**
- Create: `src/companion/obdlink.h`, `src/companion/obdlink.cpp`
- Modify: `include/config.example.h` (OBD options, after `COMPANION_DECK`)
- Modify: `src/companion/app.cpp` (`setup()`: start the link)

**Interfaces:**
- Consumes: `obd::request`, `obd::parse`, `obd::looksLikeAdapter`, `obdplan::Plan`, `obdplan::Health`, `obdplan::Link`
- Produces:
  ```cpp
  namespace obdlink {
  struct View {
      obdplan::Link link;
      float value[(int)obd::Pid::COUNT];
      bool have[(int)obd::Pid::COUNT];        // a reading exists
      uint32_t atMs[(int)obd::Pid::COUNT];    // millis() of the last good reading
      char adapter[32];                       // the adapter's Bluetooth name ("" before one)
  };
  void start();                // the OBD task on core 0 (no-op if Bluetooth can't start)
  void snapshot(View& out);    // copy under the lock (UI loop)
  }
  ```

- [ ] **Step 1: Write `src/companion/obdlink.h`**

```cpp
#pragma once
#include <cstdint>
#include "../util/obd.h"
#include "../util/obdplan.h"
// The old deck's link to an ELM327 over Bluetooth Classic (BluetoothSerial, master): finds the
// adapter, sets it up, asks the gauges' values in obdplan's order and publishes them. Its own
// task (a Bluetooth connect blocks for seconds). Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdlink {
struct View {
    obdplan::Link link;
    float value[(int)obd::Pid::COUNT];
    bool have[(int)obd::Pid::COUNT];        // a reading exists
    uint32_t atMs[(int)obd::Pid::COUNT];    // millis() of the last good reading
    char adapter[32];                       // the adapter's Bluetooth name ("" before one)
};
void start();                // the OBD task on core 0 (no-op if Bluetooth can't start)
void snapshot(View& out);    // copy under the lock (UI loop)
}
```

- [ ] **Step 2: Write `src/companion/obdlink.cpp`**

```cpp
#include "obdlink.h"
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <cstring>
#include "../config.h"
#include "../core/mem.h"

#ifndef OBD_PIN
#define OBD_PIN "1234"          // the clones ship with 1234, 0000 or 8888
#endif
// OBD_NAME (optional): the adapter's exact Bluetooth name; unset = the first device whose name
// looks like one (obd::looksLikeAdapter).

namespace obdlink {
static SemaphoreHandle_t s_lock = nullptr;
static View s_view{};
static BluetoothSerial s_bt;
constexpr uint32_t RECONNECT_MS = 30000, NO_ENGINE_MS = 5000, DISCOVER_MS = 10000;

static void setLink(obdplan::Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    xSemaphoreGive(s_lock);
}

static void publish(obd::Pid p, obd::Reply r, float v, obdplan::Link l) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.link = l;
    if (r == obd::Reply::Ok) {
        s_view.value[(int)p] = v;
        s_view.have[(int)p] = true;
        s_view.atMs[(int)p] = millis();
    }
    xSemaphoreGive(s_lock);
}

// Sends cmd and reads up to the '>' prompt (or the timeout) into out, '\r' as '\n'. False on
// timeout with nothing read.
static bool ask(const char* cmd, char* out, size_t n, uint32_t timeoutMs) {
    while (s_bt.available()) s_bt.read();                 // drop anything left over
    s_bt.print(cmd);
    size_t len = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (!s_bt.available()) { delay(2); continue; }
        const int c = s_bt.read();
        if (c == '>') { out[len] = '\0'; return true; }
        if (len + 1 < n) out[len++] = c == '\r' ? '\n' : (char)c;
    }
    out[len] = '\0';
    return len > 0;
}

// Discovers nearby Bluetooth devices and picks the adapter; logs every name seen.
static bool findAdapter(BTAddress* addr, char* name, size_t n) {
    BTScanResults* found = s_bt.discover(DISCOVER_MS);
    if (!found) return false;
    for (int i = 0; i < found->getCount(); ++i) {
        BTAdvertisedDevice* d = found->getDevice(i);
        const std::string nm = d->getName();
        Serial.printf("[obd] seen \"%s\"\n", nm.c_str());
#ifdef OBD_NAME
        const bool match = nm == OBD_NAME;
#else
        const bool match = obd::looksLikeAdapter(nm.c_str());
#endif
        if (match) {
            *addr = d->getAddress();
            strlcpy(name, nm.c_str(), n);
            return true;
        }
    }
    return false;
}

static bool init() {
    static const char* const CMDS[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATSP0\r"};
    char raw[64];
    for (const char* c : CMDS) {
        if (!ask(c, raw, sizeof raw, 2000)) {
            Serial.printf("[obd] no answer to %.4s\n", c);
            return false;
        }
    }
    Serial.println("[obd] adapter ready");
    return true;
}

static void task(void*) {
    for (;;) {
        setLink(obdplan::Link::Off);
        BTAddress addr;
        char name[32] = "";
        if (!findAdapter(&addr, name, sizeof name)) { delay(RECONNECT_MS); continue; }
        setLink(obdplan::Link::Connecting);
        Serial.printf("[obd] connecting to %s\n", name);
        if (!s_bt.connect(addr) || !init()) {
            Serial.println("[obd] connect failed");
            s_bt.disconnect();
            delay(RECONNECT_MS);
            continue;
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strlcpy(s_view.adapter, name, sizeof s_view.adapter);
        xSemaphoreGive(s_lock);
        mem::log("obd connected");
        obdplan::Plan plan;
        obdplan::Health health;
        bool first = true;                                // the first PID may say SEARCHING... for seconds
        while (s_bt.connected()) {
            const obd::Pid p = plan.next(millis());
            char raw[128];
            const bool got = ask(obd::request(p), raw, sizeof raw, first ? 5000 : 1000);
            first = false;
            float v = 0.0f;
            const obd::Reply r = got ? obd::parse(p, raw, &v) : obd::Reply::Error;
            plan.done(p, r, millis());
            const obdplan::Link l = health.reply(r);
            publish(p, r, v, l);
            if (health.lost()) { Serial.println("[obd] adapter stopped answering"); break; }
            if (l == obdplan::Link::NoEngine) delay(NO_ENGINE_MS);
        }
        s_bt.disconnect();
        Serial.println("[obd] disconnected");
        delay(2000);
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.link = obdplan::Link::Off;
    if (!s_bt.begin("cardeck-obd", true) || !s_bt.setPin(OBD_PIN)) {
        Serial.println("[obd] Bluetooth didn't start: no gauges");
        return;
    }
    mem::log("bt on");
    xTaskCreatePinnedToCore(task, "obd", 6144, nullptr, 1, nullptr, 0);
}

void snapshot(View& out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
}
}
```

If `s_lock` is null (start() not called) `snapshot` must not be used; the app only calls it after `start()`.

- [ ] **Step 3: Document the options in `include/config.example.h`** (after the `COMPANION_DECK` line):

```cpp

// OBD gauges on the companion (ELM327 over Bluetooth Classic, read-only). All optional.
// #define OBD_NAME           "OBDII"   // the adapter's exact Bluetooth name (default: any OBD/ELM/V-LINK)
// #define OBD_PIN            "1234"    // its pairing PIN (1234, 0000 or 8888 on most clones)
// #define OBD_REDLINE        6500      // the RPM bar's full scale
```

- [ ] **Step 4: Start the link in `src/companion/app.cpp` `setup()`**, after `decklink::start();`:

```cpp
    obdlink::start();           // Bluetooth to the ELM327 (its own task); gauges when it answers
```
and add `#include "obdlink.h"` with the other companion includes.

- [ ] **Step 5: Build and check the budgets**

Run: `pio run -e cyd_companion` then `python tools/check_flash.py .pio/build/cyd_companion/firmware.bin 0x300000 97`
Expected: SUCCESS; flash under 97 % (expected ~70–80 %). Also run `pio run -e cyd -e s3` — both SUCCESS and unchanged in size (they don't build `companion/`).

- [ ] **Step 6: Commit**

```bash
git add src/companion/obdlink.h src/companion/obdlink.cpp src/companion/app.cpp include/config.example.h
git commit -m "feat(companion): Bluetooth link to an ELM327 (discover, connect, ask, publish)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: The gauge screen (`companion/gauges`) and switching in the app

**Files:**
- Create: `src/companion/gauges.h`, `src/companion/gauges.cpp`
- Modify: `src/companion/app.cpp` (`loop()`)

**Interfaces:**
- Consumes: `obdlink::View`, `obdlink::snapshot`, `obdplan::Hold`, `obdplan::rpmBand / coolantBand / batteryBand / frac`, `avatarlines::Event::CarOn / CarOff`, `avatarview::say`, `screens::bar`, `ui::battleBox`, `ui::drawText`, `theme::pal()`, theme colours `theme::HP_GREEN`, `theme::HP_YELLOW`, `theme::HP_RED`, `theme::EXP_BLUE`, `theme::HP_EMPTY`
- Produces:
  ```cpp
  namespace gauges {
  void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);   // redraws what changed
  void invalidate();                                                 // repaint all next draw
  }
  ```

- [ ] **Step 1: Write `src/companion/gauges.h`**

```cpp
#pragma once
#include <TFT_eSPI.h>
#include "obdlink.h"
// The old deck's OBD gauges in the battle-UI look: RPM as a wide HP-style bar, speed as the
// biggest number, coolant / battery / load / throttle in four small boxes, her message bar kept
// at the bottom. Redraws only boxes whose shown text changed.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md ("Screen")
namespace gauges {
void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t nowMs);
void invalidate();
}
```

- [ ] **Step 2: Write `src/companion/gauges.cpp`**

```cpp
#include "gauges.h"
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "../ui/battle.h"
#include "../ui/textdraw.h"
#include "../ui/theme.h"
#include "../util/menunav.h"
#include "../util/obdplan.h"

#ifndef OBD_REDLINE
#define OBD_REDLINE 6500        // the Honda City 1.5 L's; the RPM bar's full scale
#endif

namespace gauges {
namespace {
using obd::Pid;
using obdplan::Band;
constexpr uint32_t STALE_MS = 3000;   // older readings are drawn dimmed
constexpr int W = 320;

struct Box { int x, y, w, h; const char* name; };
constexpr Box RPM{6, 4, 308, 58, "RPM"};
constexpr Box SPEED{6, 66, 140, 134, "SPEED"};
constexpr Box SMALL[4] = {{150, 66, 80, 65, "COOLANT"}, {234, 66, 80, 65, "BATTERY"},
                          {150, 135, 80, 65, "LOAD"}, {234, 135, 80, 65, "THROTTLE"}};
constexpr Pid SMALL_PID[4] = {Pid::Coolant, Pid::Battery, Pid::Load, Pid::Throttle};

char s_shown[6][24];       // RPM, speed, then the four small boxes: the text last drawn
bool s_full = true;

uint16_t bandColor(Band b) {
    switch (b) {
    case Band::Cold: return theme::EXP_BLUE;
    case Band::Warn: return theme::HP_YELLOW;
    case Band::Hot: return theme::HP_RED;
    default: return theme::HP_GREEN;
    }
}

void bar(TFT_eSPI& t, int x, int y, int w, int h, float f, uint16_t c) {
    const int fill = (int)(f * (w - 2) + 0.5f);
    t.drawRect(x, y, w, h, theme::pal().boxBorder);
    t.fillRect(x + 1, y + 1, fill, h - 2, c);
    t.fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, theme::HP_EMPTY);
}

bool fresh(const obdlink::View& v, Pid p, uint32_t now) {
    return v.have[(int)p] && now - v.atMs[(int)p] < STALE_MS;
}

// The text a box shows for p ("--" before a reading); the trailing '~' marks it stale.
void text(const obdlink::View& v, Pid p, uint32_t now, char* out, size_t n) {
    if (!v.have[(int)p]) { snprintf(out, n, "--"); return; }
    const float x = v.value[(int)p];
    switch (p) {
    case Pid::Battery: snprintf(out, n, "%.1fV", (double)x); break;
    case Pid::Coolant: snprintf(out, n, "%dC", (int)x); break;
    case Pid::Load:
    case Pid::Throttle: snprintf(out, n, "%d%%", (int)(x + 0.5f)); break;
    default: snprintf(out, n, "%d", (int)(x + 0.5f)); break;
    }
    if (!fresh(v, p, now)) strncat(out, "~", n - strlen(out) - 1);
}

void drawSmall(TFT_eSPI& t, int i, const obdlink::View& v, uint32_t now, const char* s) {
    const Box& b = SMALL[i];
    const Pid p = SMALL_PID[i];
    const theme::Palette& pal = theme::pal();
    t.fillRect(b.x + 4, b.y + 18, b.w - 8, 26, pal.boxFill);
    const uint16_t fg = fresh(v, p, now) ? pal.text : pal.boxShadow;
    ui::drawText(t, s, b.x + b.w / 2, b.y + 31, fg, pal.textShadow, MC_DATUM, b.w - 8);
    const float x = v.have[(int)p] ? v.value[(int)p] : 0.0f;
    float f = 0.0f;
    Band band = Band::Ok;
    if (p == Pid::Coolant) { f = obdplan::frac(x, 40.0f, 120.0f); band = obdplan::coolantBand(x); }
    else if (p == Pid::Battery) { f = obdplan::frac(x, 11.0f, 15.0f); band = obdplan::batteryBand(x); }
    else f = obdplan::frac(x, 0.0f, 100.0f);
    bar(t, b.x + 6, b.y + b.h - 16, b.w - 12, 8, v.have[(int)p] ? f : 0.0f, bandColor(band));
}
}

void invalidate() { s_full = true; }

void draw(TFT_eSPI& t, const obdlink::View& v, uint32_t now) {
    const theme::Palette& pal = theme::pal();
    if (s_full) {
        t.fillRect(0, 0, W, menunav::BAR.y - 2, pal.top);
        ui::battleBox(t, RPM.x, RPM.y, RPM.w, RPM.h, ui::Tab::None);
        ui::battleBox(t, SPEED.x, SPEED.y, SPEED.w, SPEED.h, ui::Tab::None);
        for (const Box& b : SMALL) ui::battleBox(t, b.x, b.y, b.w, b.h, ui::Tab::None);
        ui::drawText(t, RPM.name, RPM.x + 10, RPM.y + 6, pal.text, pal.textShadow, TL_DATUM, 80);
        ui::drawText(t, "km/h", SPEED.x + SPEED.w / 2, SPEED.y + SPEED.h - 18, pal.text,
                     pal.textShadow, MC_DATUM, SPEED.w);
        for (const Box& b : SMALL)
            ui::drawText(t, b.name, b.x + b.w / 2, b.y + 9, pal.text, pal.textShadow, MC_DATUM, b.w - 6);
        memset(s_shown, 0, sizeof s_shown);
        s_full = false;
    }
    char s[24];
    text(v, Pid::Rpm, now, s, sizeof s);
    if (strcmp(s, s_shown[0]) != 0) {
        strlcpy(s_shown[0], s, sizeof s_shown[0]);
        t.fillRect(RPM.x + 200, RPM.y + 4, 100, 20, pal.boxFill);
        ui::drawText(t, s, RPM.x + RPM.w - 10, RPM.y + 6, fresh(v, Pid::Rpm, now) ? pal.text : pal.boxShadow,
                     pal.textShadow, TR_DATUM, 100);
        const float rpm = v.have[(int)Pid::Rpm] ? v.value[(int)Pid::Rpm] : 0.0f;
        bar(t, RPM.x + 10, RPM.y + 30, RPM.w - 20, 16, obdplan::frac(rpm, 0.0f, OBD_REDLINE),
            bandColor(obdplan::rpmBand(rpm, OBD_REDLINE)));
    }
    text(v, Pid::Speed, now, s, sizeof s);
    if (strcmp(s, s_shown[1]) != 0) {
        strlcpy(s_shown[1], s, sizeof s_shown[1]);
        t.fillRect(SPEED.x + 6, SPEED.y + 20, SPEED.w - 12, 70, pal.boxFill);
        t.setTextColor(fresh(v, Pid::Speed, now) ? pal.text : pal.boxShadow, pal.boxFill);
        t.setTextDatum(MC_DATUM);
        t.setTextSize(2);
        t.drawString(s, SPEED.x + SPEED.w / 2, SPEED.y + 55, 4);   // font 4 doubled: ~52 px
        t.setTextSize(1);
    }
    for (int i = 0; i < 4; ++i) {
        text(v, SMALL_PID[i], now, s, sizeof s);
        if (strcmp(s, s_shown[2 + i]) == 0) continue;
        strlcpy(s_shown[2 + i], s, sizeof s_shown[2 + i]);
        drawSmall(t, i, v, now, s);
    }
}
}
```

- [ ] **Step 3: Switch screens in `src/companion/app.cpp`**

Add includes: `#include "gauges.h"` and `#include "../util/obdplan.h"`. Add two statics after `s_press`:

```cpp
static obdplan::Hold s_obdHold;   // gauges on screen (util/obdplan: shown at once, gone 5 s after)
static bool s_gauges = false;     // as drawn
```

In `loop()`, right after `const uint32_t now = millis();` and the `if (!avatarview::on()) {...}` block (gauges need her message bar, so they only run with the avatar, like the menu), insert:

```cpp
    obdlink::View car;
    obdlink::snapshot(car);
    const bool showGauges = s_obdHold.update(car.link == obdplan::Link::Live, now);
    if (showGauges != s_gauges) {                 // the car came or went
        s_gauges = showGauges;
        if (s_gauges) {
            s_tft.fillRect(0, 0, 320, menunav::BAR.y - 2, theme::pal().top);
            avatarview::place(0, 0, 0);           // she leaves the screen, her bar stays
            gauges::invalidate();
            screens::invalidate();
            avatarview::say(avatarlines::Event::CarOn, v, now);
        } else {
            enter(s_screen);                      // back to the screen it left
            avatarview::say(avatarlines::Event::CarOff, v, now);
        }
    }
    if (s_gauges) {
        fire(phone::take());                      // the phone still works; screens apply on return
        handleResult(v, now);
        const char* line = avatarview::update(s_tft, v, now);
        gauges::draw(s_tft, car, now);
        screens::bar(s_tft, line ? line : "Live from the car");
        delay(20);
        return;
    }
```

Note: `fire()` may call `enter()` (e.g. LYRICS from the phone) while gauges are up; that repaints her screen. Guard it: at the top of `enter()`, add

```cpp
    if (s_gauges) { s_screen = s; return; }       // the gauges own the screen; s shows on return
```

- [ ] **Step 4: Build and check the budget**

Run: `pio run -e cyd_companion` and `python tools/check_flash.py .pio/build/cyd_companion/firmware.bin 0x300000 97`
Expected: SUCCESS, under 97 %. Then the full host suite: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_tests.ps1` → `suites pass=64 fail=0`.

- [ ] **Step 5: Flash and check without the car**

Run: `pio run -e cyd_companion -t upload --upload-port COM14`, then `python tools/capture_serial.py COM14 60 obd-nocar.log`
Expected in the log: `[obd] Bluetooth` does not fail; `[mem] bt on free=... largest=...`; `[obd] seen "..."` lines for nearby devices (or none); no crash; Zero Two's home as before; `[companion] deck found` still appears (WiFi and Bluetooth together).

- [ ] **Step 6: Commit**

```bash
git add src/companion/gauges.h src/companion/gauges.cpp src/companion/app.cpp
git commit -m "feat(companion): OBD gauge screen while the car answers, Zero Two otherwise

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Docs

**Files:**
- Modify: `README.md` (features list: an "OBD gauges (companion)" bullet; "Make it yours" or config notes for `OBD_NAME` / `OBD_PIN` / `OBD_REDLINE`)
- Modify: `CHANGELOG.md` (`[Unreleased]` → Added)
- Modify: `docs/ARCHITECTURE.md` (companion section: `obdlink`, `gauges`; pure modules: `obd`, `obdplan`)
- Modify: `docs/BACKLOG.md` (the "OBD2 car info" idea: built as a showcase; later: fault codes, phone CAR button)
- Modify: `docs/superpowers/README.md` (index row for this spec and plan, status "implemented")

- [ ] **Step 1: Write the entries** — each one or two sentences, in the file's own style, naming: Bluetooth Classic ELM327 only (Android-style clones, PIN 1234/0000/8888), read-only, the values shown, gauges only while the car answers, Zero Two otherwise, `cyd_companion` only.

- [ ] **Step 2: Check links and commit**

Run: `grep -n "obd" README.md CHANGELOG.md docs/ARCHITECTURE.md docs/BACKLOG.md docs/superpowers/README.md`
Expected: each file mentions it once in the right section.

```bash
git add README.md CHANGELOG.md docs/ARCHITECTURE.md docs/BACKLOG.md docs/superpowers/README.md
git commit -m "docs: OBD gauges on the old deck

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: On the car (needs the owner)

Not code; the acceptance check. The owner plugs the ELM327 into the City's OBD port, turns the ignition on, powers the old deck in the car, and connects it by USB to a laptop (or leaves it and reports what the screen shows).

- [ ] **Step 1:** `python tools/capture_serial.py <port> 300 obd-car.log` while: ignition on (engine off) → start the engine → idle 30 s → a gentle rev to ~3000 rpm → ignition off → unplug the adapter.
- [ ] **Step 2:** Expected: `[obd] seen "<name>"`, `[obd] connecting to <name>`, `[obd] adapter ready`, `[mem] obd connected ...`; gauges appear with "Car connected, darling!"; idle ~600–800 rpm; the rev shows on the bar (yellow from 70 % of the redline: ~4550 rpm by default); coolant climbing; battery 13.5–14.5 V with the engine running; after ignition off, `NoEngine` and Zero Two back within ~5 s with "The car's asleep~"; after unplugging, `[obd] adapter stopped answering` / `disconnected`.
- [ ] **Step 3:** If the adapter isn't found: read the `[obd] seen` names and set `OBD_NAME` (and `OBD_PIN` 0000 / 8888) in `src/config.h`, reflash. Record any fix as a commit with its own test where it changes `util/`.
