# Companion Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the CYD into a companion dashboard (clock, trip, weather, big lyrics, sound and trip buttons) fed by the S3 over the hotspot, with no Spotify client of its own.

**Architecture:** The S3's `GET /state` gains song timing, the current/next lyric line and a trip clock; `POST /trip/reset` resets it. A new `cyd_companion` build polls `/state` about once a second from a network task, fetches Open-Meteo every 15 min, and draws one dashboard on the UI loop with XPT2046 touch. All parsing, timing, layout and formatting are pure modules with host tests.

**Tech Stack:** PlatformIO espressif32@6.9.0, arduino-esp32 2.0.17, C++17, TFT_eSPI 2.5.43, Arduino `HTTPClient`/`WebServer`, PaulStoffregen XPT2046_Touchscreen (f956c5d, already used by `hwcheck`), Unity host tests via `tools/ntest.ps1` / `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-13-companion-screen-design.md` (core companion only; the "Optional extra: retro map" section gets its own plan later).

## Global Constraints

- Host tests use **made-up lyric lines only** (e.g. "first made-up line"), never real song text.
- No music controls on the companion (CarPlay does that). Buttons: `Howl`, `Vroom` (only when the S3 lists that clip), `Trip` (1 s press).
- Polling: about once a second; every 5 s after 10 misses in a row.
- Weather: Open-Meteo, home location from `src/config.h` (`HOME_LAT`, `HOME_LON`), every 15 min; keep the last value up to 1 h, then `--`.
- Every request first checks `netcfg::linkUp()`; no request without WiFi.
- `cyd` (deck) build keeps working unchanged; `tools/check_flash.py` 97 % limit applies to `cyd_companion` too.
- Backlight stays at full (decided 2026-10-07). No SD card.
- Conventional commits (`type(scope): subject`), ending with the `Co-Authored-By` line in use for this repo.
- `src/config.h` is the user's, git-ignored: add new options only to `include/config.example.h`, each with an `#ifndef` default in code so an older `config.h` still builds.
- Python edit scripts are written as files (the shell heredoc mangles `\n` and `\0`).

## Plan rulings (deviations from the spec, decided while planning)

- `/state` lyric fields are **flat top-level keys** (`lyricKind`, `line`, `next`, `lineStartMs`, `nextStartMs`) instead of a nested `lyric` object, so the CYD reads `/state` with one small flat-JSON reader (`util/jsonflat`) and no JSON library. Cost if wrong: a rename on both sides.
- The current lyric line is drawn with the existing font-2 text path **at 2x scale** (accents and Japanese/Chinese stay correct) and wraps over **2 rows** (`glyphrun::wrapTwo`), not 3: two 32 px rows plus the dim next line fill the lyric area. Cost if wrong: a 3-row wrap helper later.

## Review Focus

1. A song change between two polls: the CYD must never show the previous song's line over the new title (Task 3 test `test_song_change_drops_the_old_lines`).
2. `/state` truncated mid-string or mid-number (weak hotspot): parse must fail, keeping the last good state (Task 2 test `test_truncated_body_is_rejected`).
3. Lyric line containing quotes, backslashes or `ç`-style escapes: shown decoded, not raw (Task 2 test `test_string_escapes_are_decoded`).
4. Holding a finger on `Trip` for 5 s: one reset, not five; touching `Howl` and sliding onto `Trip`: no reset (Task 5 tests `test_trip_fires_once_per_hold`, `test_button_is_chosen_at_touch_down`).
5. Open-Meteo's `current_units` block repeats the key `temperature_2m` with a string value before `current`: the parser must read the number from `current` (Task 4 test `test_reads_current_not_units`).

---

## File structure

| File | Responsibility |
|---|---|
| `src/util/tripclock.h` (new) | Pure: seconds since start/reset, wrap-safe |
| `src/util/lyricbuf.{h,cpp}` (modify) | Pure: `pairAt()` current + next line indices |
| `src/util/statejson.{h,cpp}` (modify) | Pure: new `/state` fields; `lyricKindName()` |
| `src/core/shared.{h,cpp}` (modify) | `lyricPair()`: current/next line + start times under the lock |
| `src/net/remote.cpp` (modify) | `/state` fills the new fields; `POST /trip/reset` |
| `src/util/jsonflat.{h,cpp}` (new) | Pure: read string/number/bool/array-membership by key from flat JSON |
| `src/util/stateparse.{h,cpp}` (new) | Pure: `/state` body -> `stateparse::Deck` |
| `src/util/lyricclock.h` (new) | Pure: switch to the next line on the CYD's own clock |
| `src/util/pollpace.h` (new) | Pure: 1 s / 5 s poll pacing |
| `src/util/weather.{h,cpp}` (new) | Pure: Open-Meteo parse, WMO code -> icon |
| `src/util/dash.{h,cpp}` (new) | Pure: button rects, hit test, press logic, strip text |
| `src/ui/textdraw.{h,cpp}` (modify) | `drawRunScaled()`: font-2 runs at 2x |
| `src/companion/link.{h,cpp}` (new) | Network task: `/state` polls, actions, weather |
| `src/companion/touch.{h,cpp}` (new) | XPT2046 on VSPI, raw -> screen |
| `src/companion/dashboard.{h,cpp}` (new) | Drawing: top strip, lyric area, button bar |
| `src/companion/app.cpp` (new) | `setup()`/`loop()` of the companion build |
| `platformio.ini` (modify) | `[env:cyd_companion]` |
| `include/config.example.h`, `include/pins.h` (modify) | `COMPANION_DECK`, `HOME_LAT/LON`; touch calibration |
| `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/BACKLOG.md` (modify) | Docs |

---

### Task 1: S3 `/state` gains timing, lyric pair and a trip clock

**Files:**
- Create: `src/util/tripclock.h`, `test/test_tripclock/test_tripclock.cpp`
- Modify: `src/util/lyricbuf.h`, `src/util/lyricbuf.cpp`, `test/test_lyricbuf/test_lyricbuf.cpp`
- Modify: `src/util/statejson.h`, `src/util/statejson.cpp`, `test/test_statejson/test_statejson.cpp`
- Modify: `src/core/shared.h`, `src/core/shared.cpp`, `src/net/remote.cpp`

**Interfaces:**
- Produces: `tripclock::Clock{ explicit Clock(uint32_t nowMs = 0); void reset(uint32_t nowMs); uint32_t seconds(uint32_t nowMs) const; }`
- Produces: `lyricbuf::Pair{ int cur; int next; }` and `lyricbuf::Pair lyricbuf::pairAt(const Lyrics& l, uint32_t posMs)` (cur -1 before the first line; next -1 when none)
- Produces: `/state` keys `trackGen, progressMs, durationMs, playing, tripS, lyricKind, line, next, lineStartMs, nextStartMs` (plus the existing ones); `lyricKind` is one of `searching|synced|plain|instrumental|none`
- Produces: `POST /trip/reset?k=` -> 204 (403 wrong key)

- [ ] **Step 1: Write the failing tests**

`test/test_tripclock/test_tripclock.cpp`:
```cpp
#include <unity.h>
#include <cstdint>
#include "../../src/util/tripclock.h"

void setUp() {}
void tearDown() {}

void test_counts_seconds_since_start_and_reset() {
    tripclock::Clock c(1000);
    TEST_ASSERT_EQUAL_UINT32(0, c.seconds(1999));
    TEST_ASSERT_EQUAL_UINT32(37 * 60, c.seconds(1000 + 37 * 60 * 1000));
    c.reset(500000);
    TEST_ASSERT_EQUAL_UINT32(2, c.seconds(502500));
}

void test_survives_millis_wrap() {
    tripclock::Clock c(0xFFFFF000u);                       // 4.096 s before the wrap
    TEST_ASSERT_EQUAL_UINT32(10, c.seconds(0x00001710u));  // 10 s later, past the wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_counts_seconds_since_start_and_reset);
    RUN_TEST(test_survives_millis_wrap);
    return UNITY_END();
}
```

Append to `test/test_lyricbuf/test_lyricbuf.cpp` (before `int main`) and register with `RUN_TEST(test_pair_gives_current_and_next);`:
```cpp
void test_pair_gives_current_and_next() {
    static lyricbuf::Lyrics l;
    lyricbuf::parse("[00:05.00]first made-up line\n[00:09.00]second made-up line\n[00:14.00]third made-up line\n", l);
    lyricbuf::Pair p = lyricbuf::pairAt(l, 1000);           // intro
    TEST_ASSERT_EQUAL_INT(-1, p.cur);
    TEST_ASSERT_EQUAL_INT(0, p.next);
    p = lyricbuf::pairAt(l, 9500);
    TEST_ASSERT_EQUAL_INT(1, p.cur);
    TEST_ASSERT_EQUAL_INT(2, p.next);
    p = lyricbuf::pairAt(l, 20000);                         // last line: nothing next
    TEST_ASSERT_EQUAL_INT(2, p.cur);
    TEST_ASSERT_EQUAL_INT(-1, p.next);
    lyricbuf::reset(l);
    p = lyricbuf::pairAt(l, 0);                             // no lyrics
    TEST_ASSERT_EQUAL_INT(-1, p.cur);
    TEST_ASSERT_EQUAL_INT(-1, p.next);
}
```

Replace `sample()` and `test_writes_the_whole_state` in `test/test_statejson/test_statejson.cpp`, and add the kind-name test (register `RUN_TEST(test_lyric_kind_names);`):
```cpp
static statejson::State sample() {
    statejson::State s{};
    s.track = "Highway Song";
    s.artist = "The Band";
    s.title = "";
    s.detail = "";
    s.ssid = "iPhone";
    s.rssi = -52;
    s.uptimeS = 754;
    s.clips = CLIPS;
    s.clipCount = 1;
    s.trackGen = 7;
    s.progressMs = 61000;
    s.durationMs = 200000;
    s.playing = true;
    s.tripS = 2220;
    s.lyricKind = "synced";
    s.line = "first made-up line";
    s.next = "second made-up line";
    s.lineStartMs = 59000;
    s.nextStartMs = 64000;
    return s;
}

void test_writes_the_whole_state() {
    char out[768];
    size_t n = statejson::write(sample(), out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(
        "{\"track\":\"Highway Song\",\"artist\":\"The Band\",\"status\":\"\",\"detail\":\"\","
        "\"wifi\":\"iPhone\",\"rssi\":-52,\"uptime\":754,\"clips\":[\"howl\"],"
        "\"trackGen\":7,\"progressMs\":61000,\"durationMs\":200000,\"playing\":true,\"tripS\":2220,"
        "\"lyricKind\":\"synced\",\"line\":\"first made-up line\",\"next\":\"second made-up line\","
        "\"lineStartMs\":59000,\"nextStartMs\":64000}",
        out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
}

void test_lyric_kind_names() {
    using lyricstatus::Status;
    TEST_ASSERT_EQUAL_STRING("searching", statejson::lyricKindName(Status::Searching));
    TEST_ASSERT_EQUAL_STRING("searching", statejson::lyricKindName(Status::Retrying));
    TEST_ASSERT_EQUAL_STRING("synced", statejson::lyricKindName(Status::Synced));
    TEST_ASSERT_EQUAL_STRING("plain", statejson::lyricKindName(Status::Plain));
    TEST_ASSERT_EQUAL_STRING("instrumental", statejson::lyricKindName(Status::Instrumental));
    TEST_ASSERT_EQUAL_STRING("none", statejson::lyricKindName(Status::None));
}
```
Also change every other `char out[512]` in that file to `char out[768]`, and the small-buffer test's buffer stays 40.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: FAIL for `test_tripclock` (missing header), `test_lyricbuf` (`pairAt` not a member), `test_statejson` (no member `trackGen` / `lyricKindName`).

- [ ] **Step 3: Implement**

`src/util/tripclock.h`:
```cpp
#pragma once
#include <cstdint>
// Trip time: seconds since power-on (the car started) or the last reset from the companion's
// Trip button. millis() wrap-safe. PURE, host-tested.
namespace tripclock {
class Clock {
public:
    explicit Clock(uint32_t nowMs = 0) : start_(nowMs) {}
    void reset(uint32_t nowMs) { start_ = nowMs; }
    uint32_t seconds(uint32_t nowMs) const { return (nowMs - start_) / 1000; }
private:
    uint32_t start_;
};
}
```

`src/util/lyricbuf.h` (after `lineText`):
```cpp
// Current line for posMs (-1 before the first) and the one after it (-1 when none): what the
// companion screen shows as the big line and the dim next line.
struct Pair { int cur; int next; };
Pair pairAt(const Lyrics& l, uint32_t posMs);
```
`src/util/lyricbuf.cpp` (inside the namespace, at the end):
```cpp
Pair pairAt(const Lyrics& l, uint32_t posMs) {
    Pair p{-1, -1};
    if (l.n <= 0) return p;
    p.cur = currentIndex(l, posMs);
    p.next = p.cur + 1 < l.n ? p.cur + 1 : -1;
    return p;
}
```

`src/util/statejson.h`: add `#include "lyricstatus.h"`, extend `State` after `clipCount`:
```cpp
    uint32_t trackGen;      // changes on every song
    uint32_t progressMs, durationMs;
    bool playing;
    uint32_t tripS;         // trip clock (util/tripclock)
    const char* lyricKind;  // lyricKindName()
    const char* line;       // current lyric line ("" during the intro or without lyrics)
    const char* next;       // the line after it
    uint32_t lineStartMs, nextStartMs;   // song position where they start (0 = unknown)
```
and declare:
```cpp
// "searching" (also while retrying), "synced", "plain", "instrumental", "none".
const char* lyricKindName(lyricstatus::Status s);
```
`src/util/statejson.cpp`: after the `clips` array (before the closing `o.put('}')`):
```cpp
    o.key("trackGen");    o.num((long)s.trackGen);
    o.key("progressMs");  o.num((long)s.progressMs);
    o.key("durationMs");  o.num((long)s.durationMs);
    o.key("playing");     o.raw(s.playing ? "true" : "false");
    o.key("tripS");       o.num((long)s.tripS);
    o.key("lyricKind");   o.str(s.lyricKind);
    o.key("line");        o.str(s.line);
    o.key("next");        o.str(s.next);
    o.key("lineStartMs"); o.num((long)s.lineStartMs);
    o.key("nextStartMs"); o.num((long)s.nextStartMs);
```
and add:
```cpp
const char* lyricKindName(lyricstatus::Status s) {
    switch (s) {
    case lyricstatus::Status::Synced:       return "synced";
    case lyricstatus::Status::Plain:        return "plain";
    case lyricstatus::Status::Instrumental: return "instrumental";
    case lyricstatus::Status::None:         return "none";
    case lyricstatus::Status::Searching:
    case lyricstatus::Status::Retrying:     break;
    }
    return "searching";
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=41 fail=0` (40 before + `test_tripclock`).

- [ ] **Step 5: Device side**

`src/core/shared.h` (after `lyricView`):
```cpp
// Current and next lyric lines with their start times, for GET /state (net/remote).
struct LyricPair {
    lyricstatus::Status status;
    char line[160], next[160];
    uint32_t lineStartMs, nextStartMs;
};
void lyricPair(uint32_t gen, uint32_t posMs, LyricPair& out);
```
`src/core/shared.cpp` (next to `lyricView`):
```cpp
void lyricPair(uint32_t gen, uint32_t posMs, LyricPair& out) {
    Guard g;
    out.status = (gen != 0 && s_statusGen == gen) ? s_status : lyricstatus::Status::Searching;
    out.line[0] = out.next[0] = '\0';
    out.lineStartMs = out.nextStartMs = 0;
    if (gen == 0 || s_lyricsGen != gen) return;
    const lyricbuf::Lyrics& l = lyricsvc::arena();
    lyricbuf::Pair p = lyricbuf::pairAt(l, posMs);
    if (p.cur >= 0) {
        txt::copy(out.line, lyricbuf::lineText(l, p.cur), sizeof(out.line));
        out.lineStartMs = l.lines[p.cur].tMs;
    }
    if (p.next >= 0) {
        txt::copy(out.next, lyricbuf::lineText(l, p.next), sizeof(out.next));
        out.nextStartMs = l.lines[p.next].tMs;
    }
}
```
`src/net/remote.cpp`: add `#include "../util/tripclock.h"` and `#include "../util/interp.h"`; next to `s_mux` add `static tripclock::Clock s_trip(0);` (power-on = millis 0). In `handleState()` replace the `statejson::State s{...}` line with:
```cpp
    static shared::LyricPair lp;   // static: two 160-byte lines, off this task's stack
    const uint32_t now = millis();
    const uint32_t pos = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying,
                                                   now - st.lastPollMs);
    shared::lyricPair(st.trackGen, pos, lp);
    statejson::State s{};
    s.track = st.trackName; s.artist = st.artist; s.title = title; s.detail = detail;
    s.ssid = ssid.c_str(); s.rssi = (int)WiFi.RSSI(); s.uptimeS = now / 1000;
    s.clips = CLIPS; s.clipCount = (int)(sizeof CLIPS / sizeof CLIPS[0]);
    s.trackGen = st.trackGen; s.progressMs = pos; s.durationMs = st.durationMs;
    s.playing = st.isPlaying; s.tripS = s_trip.seconds(now);
    s.lyricKind = statejson::lyricKindName(lp.status);
    s.line = lp.line; s.next = lp.next; s.lineStartMs = lp.lineStartMs; s.nextStartMs = lp.nextStartMs;
```
and change `static char out[640];` to `static char out[768];`. In `begin()` register:
```cpp
    s_server->on("/trip/reset", HTTP_POST, [] {
        if (!s_gate.keyOk(s_server->arg("k").c_str())) { s_server->send(403, "text/plain", "wrong key"); return; }
        s_trip.reset(millis());
        Serial.println("[remote] trip reset");
        s_server->send(204);
    });
```
(`s_trip` is touched only on this task: no lock needed.)

- [ ] **Step 6: Build and check on the S3**

Run: `pio run -e s3 -t upload --upload-port COM12`, then with the S3 and a phone on the hotspot open `http://172.20.10.10/state?k=<REMOTE_TOKEN>` in Safari.
Expected: build SUCCESS; JSON with `trackGen`, `progressMs`, `lyricKind`, `line`, `next`, `tripS` while a song plays.

- [ ] **Step 7: Commit**

```bash
git add src/util/tripclock.h src/util/lyricbuf.* src/util/statejson.* src/core/shared.* src/net/remote.cpp test/test_tripclock test/test_lyricbuf test/test_statejson
git commit -m "feat(remote): /state carries song timing, the lyric pair and a trip clock"
```

---

### Task 2: Flat JSON reader and `/state` parser (CYD side, pure)

**Files:**
- Create: `src/util/jsonflat.{h,cpp}`, `src/util/stateparse.{h,cpp}`
- Test: `test/test_jsonflat/test_jsonflat.cpp`, `test/test_stateparse/test_stateparse.cpp`

**Interfaces:**
- Consumes: the `/state` keys from Task 1.
- Produces: `bool jsonflat::str(const char* body, const char* key, char* out, size_t n)`, `bool jsonflat::num(const char* body, const char* key, long* out)`, `bool jsonflat::real(const char* body, const char* key, double* out)`, `bool jsonflat::boolean(const char* body, const char* key, bool* out)`, `bool jsonflat::arrayHas(const char* body, const char* key, const char* item)`
- Produces: `struct stateparse::Deck { uint32_t trackGen, progressMs, durationMs, tripS, lineStartMs, nextStartMs; bool playing, hasVroom; char track[96], artist[96], status[24], detail[48], lyricKind[16], line[160], next[160]; }` and `bool stateparse::parse(const char* body, Deck& out)` (false: keep the last state)

- [ ] **Step 1: Write the failing tests**

`test/test_jsonflat/test_jsonflat.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/jsonflat.h"

void setUp() {}
void tearDown() {}

static const char* BODY =
    "{\"track\":\"A \\\"quoted\\\" title\",\"n\":-42,\"big\":64000,\"t\":28.5,\"on\":true,"
    "\"off\":false,\"clips\":[\"howl\",\"vroom\"],\"esc\":\"Can\\u00e7\\u00e3o \\\\ ok\\n\","
    "\"inner\":\"has \\\"n\\\": 5 inside\"}";

void test_reads_strings_numbers_and_bools() {
    char s[64];
    long n = 0;
    double d = 0;
    bool b = false;
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "track", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("A \"quoted\" title", s);
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "n", &n));
    TEST_ASSERT_EQUAL_INT32(-42, n);
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "big", &n));
    TEST_ASSERT_EQUAL_INT32(64000, n);
    TEST_ASSERT_TRUE(jsonflat::real(BODY, "t", &d));
    TEST_ASSERT_TRUE(d > 28.49 && d < 28.51);
    TEST_ASSERT_TRUE(jsonflat::boolean(BODY, "on", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_TRUE(jsonflat::boolean(BODY, "off", &b));
    TEST_ASSERT_FALSE(b);
}

void test_string_escapes_are_decoded() {
    char s[64];
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "esc", s, sizeof s));
    TEST_ASSERT_EQUAL_STRING("Canção \\ ok\n", s);   // ç ã -> UTF-8
}

void test_keys_inside_strings_are_not_keys() {
    long n = 0;
    TEST_ASSERT_FALSE(jsonflat::num(BODY, "n_missing", &n));
    // "n" appears inside the "inner" string value as text; the real key "n" is -42.
    TEST_ASSERT_TRUE(jsonflat::num(BODY, "n", &n));
    TEST_ASSERT_EQUAL_INT32(-42, n);
}

void test_array_membership() {
    TEST_ASSERT_TRUE(jsonflat::arrayHas(BODY, "clips", "vroom"));
    TEST_ASSERT_TRUE(jsonflat::arrayHas(BODY, "clips", "howl"));
    TEST_ASSERT_FALSE(jsonflat::arrayHas(BODY, "clips", "honk"));
    TEST_ASSERT_FALSE(jsonflat::arrayHas(BODY, "track", "howl"));   // not an array
}

void test_wrong_types_and_small_buffers() {
    char s[6];
    long n = 0;
    TEST_ASSERT_FALSE(jsonflat::num(BODY, "track", &n));            // string, not a number
    TEST_ASSERT_TRUE(jsonflat::str(BODY, "track", s, sizeof s));    // cut, still terminated
    TEST_ASSERT_EQUAL_size_t(5, strlen(s));
    TEST_ASSERT_FALSE(jsonflat::str(nullptr, "track", s, sizeof s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_strings_numbers_and_bools);
    RUN_TEST(test_string_escapes_are_decoded);
    RUN_TEST(test_keys_inside_strings_are_not_keys);
    RUN_TEST(test_array_membership);
    RUN_TEST(test_wrong_types_and_small_buffers);
    return UNITY_END();
}
```

`test/test_stateparse/test_stateparse.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/stateparse.h"

void setUp() {}
void tearDown() {}

static const char* FULL =
    "{\"track\":\"Highway Song\",\"artist\":\"The Band\",\"status\":\"\",\"detail\":\"\","
    "\"wifi\":\"iPhone\",\"rssi\":-52,\"uptime\":754,\"clips\":[\"howl\",\"vroom\"],"
    "\"trackGen\":7,\"progressMs\":61000,\"durationMs\":200000,\"playing\":true,\"tripS\":2220,"
    "\"lyricKind\":\"synced\",\"line\":\"first made-up line\",\"next\":\"second made-up line\","
    "\"lineStartMs\":59000,\"nextStartMs\":64000}";

void test_full_state() {
    stateparse::Deck d{};
    TEST_ASSERT_TRUE(stateparse::parse(FULL, d));
    TEST_ASSERT_EQUAL_UINT32(7, d.trackGen);
    TEST_ASSERT_EQUAL_UINT32(61000, d.progressMs);
    TEST_ASSERT_EQUAL_UINT32(2220, d.tripS);
    TEST_ASSERT_TRUE(d.playing);
    TEST_ASSERT_TRUE(d.hasVroom);
    TEST_ASSERT_EQUAL_STRING("Highway Song", d.track);
    TEST_ASSERT_EQUAL_STRING("synced", d.lyricKind);
    TEST_ASSERT_EQUAL_STRING("first made-up line", d.line);
    TEST_ASSERT_EQUAL_STRING("second made-up line", d.next);
    TEST_ASSERT_EQUAL_UINT32(64000, d.nextStartMs);
}

void test_older_deck_without_the_new_fields() {
    // A deck from before Task 1: no timing or lyric keys. Parsed, with neutral defaults.
    const char* old = "{\"track\":\"T\",\"artist\":\"A\",\"status\":\"\",\"detail\":\"\",\"wifi\":\"x\","
                      "\"rssi\":-60,\"uptime\":5,\"clips\":[\"howl\"]}";
    stateparse::Deck d{};
    TEST_ASSERT_TRUE(stateparse::parse(old, d));
    TEST_ASSERT_EQUAL_UINT32(0, d.trackGen);
    TEST_ASSERT_EQUAL_STRING("none", d.lyricKind);
    TEST_ASSERT_EQUAL_STRING("", d.line);
    TEST_ASSERT_FALSE(d.hasVroom);
}

void test_truncated_body_is_rejected() {
    stateparse::Deck d{};
    d.trackGen = 99;
    std::string cut(FULL, strlen(FULL) - 20);   // lost the tail on a weak link
    TEST_ASSERT_FALSE(stateparse::parse(cut.c_str(), d));
    TEST_ASSERT_EQUAL_UINT32(99, d.trackGen);    // untouched: the screen keeps the last state
    TEST_ASSERT_FALSE(stateparse::parse("", d));
    TEST_ASSERT_FALSE(stateparse::parse("<html>captive portal</html>", d));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_state);
    RUN_TEST(test_older_deck_without_the_new_fields);
    RUN_TEST(test_truncated_body_is_rejected);
    return UNITY_END();
}
```
Add `#include <string>` at the top of `test_stateparse.cpp` (for `std::string`).

- [ ] **Step 2: Run to verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: FAIL for both new suites (missing headers).

- [ ] **Step 3: Implement**

`src/util/jsonflat.h`:
```cpp
#pragma once
#include <cstddef>
// Reads values by key from a small JSON object whose values are strings, numbers, booleans or
// arrays of strings (the deck's GET /state). Keys are found by scanning tokens, so a key's
// name appearing inside a string value never matches. No allocation. PURE, host-tested.
namespace jsonflat {
// Decodes \" \\ \/ \b \f \n \r \t and \uXXXX (BMP, to UTF-8). Cut to n-1 bytes (whole UTF-8
// characters), always terminated. False when the key is missing or not a string.
bool str(const char* body, const char* key, char* out, size_t n);
bool num(const char* body, const char* key, long* out);      // integer; false if not a number
bool real(const char* body, const char* key, double* out);
bool boolean(const char* body, const char* key, bool* out);
bool arrayHas(const char* body, const char* key, const char* item);   // array of strings
}
```
`src/util/jsonflat.cpp`:
```cpp
#include "jsonflat.h"
#include <cstdlib>
#include <cstring>

namespace jsonflat {

// Skips a JSON string starting at p (p points at the opening quote); returns the char after the
// closing quote, or nullptr if unterminated.
static const char* skipString(const char* p) {
    for (++p; *p; ++p) {
        if (*p == '\\') { if (!*++p) return nullptr; continue; }
        if (*p == '"') return p + 1;
    }
    return nullptr;
}

// True when the string token at p (opening quote) equals key exactly (no escapes in keys).
static bool keyIs(const char* p, const char* key) {
    size_t k = strlen(key);
    return strncmp(p + 1, key, k) == 0 && p[1 + k] == '"';
}

static const char* skipSpace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

// Start of the value for `key`, or nullptr.
static const char* find(const char* body, const char* key) {
    if (!body || !key) return nullptr;
    for (const char* p = body; *p;) {
        if (*p == '"') {
            const char* end = skipString(p);
            if (!end) return nullptr;
            const char* after = skipSpace(end);
            if (*after == ':' && keyIs(p, key)) return skipSpace(after + 1);
            p = end;
        } else {
            ++p;
        }
    }
    return nullptr;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Appends UTF-8 for cp if all of it fits (keeps whole characters).
static void putUtf8(char* out, size_t n, size_t* len, unsigned cp) {
    char b[3];
    size_t k;
    if (cp < 0x80) { b[0] = (char)cp; k = 1; }
    else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
           b[2] = (char)(0x80 | (cp & 0x3F)); k = 3; }
    if (*len + k >= n) return;
    memcpy(out + *len, b, k);
    *len += k;
}

bool str(const char* body, const char* key, char* out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = '\0';
    const char* p = find(body, key);
    if (!p || *p != '"') return false;
    size_t len = 0;
    bool full = false;
    for (++p; *p && *p != '"'; ++p) {
        unsigned cp;
        if (*p == '\\') {
            ++p;
            switch (*p) {
            case 'n': cp = '\n'; break;
            case 't': cp = '\t'; break;
            case 'r': cp = '\r'; break;
            case 'b': cp = '\b'; break;
            case 'f': cp = '\f'; break;
            case 'u': {
                int v = 0;
                for (int i = 1; i <= 4; ++i) {
                    int h = hexval(p[i]);
                    if (h < 0) return false;
                    v = v * 16 + h;
                }
                p += 4;
                cp = (unsigned)v;
                break;
            }
            case '\0': return false;
            default: cp = (unsigned char)*p; break;   // \" \\ \/
            }
            if (!full) putUtf8(out, n, &len, cp);
        } else {
            // raw UTF-8 byte: keep whole characters only
            if (!full) {
                if (len + 1 >= n) full = true;
                else out[len++] = *p;
            }
        }
        if (len + 1 >= n) full = true;
    }
    if (*p != '"') { out[0] = '\0'; return false; }   // unterminated: truncated body
    while (len > 0 && ((unsigned char)out[len - 1] & 0xC0) == 0x80) {
        // drop a cut multi-byte tail
        size_t s = len - 1;
        while (s > 0 && ((unsigned char)out[s] & 0xC0) == 0x80) --s;
        unsigned char lead = (unsigned char)out[s];
        size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (len - s == need) break;
        len = s;
    }
    out[len] = '\0';
    return true;
}

bool num(const char* body, const char* key, long* out) {
    const char* p = find(body, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-')) return false;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p || *end == '.' || *end == 'e' || *end == 'E') return false;
    *out = v;
    return true;
}

bool real(const char* body, const char* key, double* out) {
    const char* p = find(body, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-')) return false;
    char* end = nullptr;
    double v = strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

bool boolean(const char* body, const char* key, bool* out) {
    const char* p = find(body, key);
    if (!p) return false;
    if (strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

bool arrayHas(const char* body, const char* key, const char* item) {
    const char* p = find(body, key);
    if (!p || *p != '[' || !item) return false;
    for (p = skipSpace(p + 1); *p && *p != ']';) {
        if (*p == '"') {
            const char* end = skipString(p);
            if (!end) return false;
            if (keyIs(p, item)) return true;
            p = skipSpace(end);
        } else {
            ++p;
        }
    }
    return false;
}

}
```
`src/util/stateparse.h`:
```cpp
#pragma once
#include <cstdint>
// The new deck's GET /state, as the companion screen needs it. PURE, host-tested.
namespace stateparse {
struct Deck {
    uint32_t trackGen, progressMs, durationMs, tripS, lineStartMs, nextStartMs;
    bool playing, hasVroom;
    char track[96], artist[96], status[24], detail[48], lyricKind[16], line[160], next[160];
};
// False (out untouched) for anything that isn't a complete JSON object: a truncated body on a
// weak link, an HTML captive portal, empty. Missing optional keys get neutral defaults (an
// older deck): numbers 0, strings "", lyricKind "none".
bool parse(const char* body, Deck& out);
}
```
`src/util/stateparse.cpp`:
```cpp
#include "stateparse.h"
#include <cstring>
#include "jsonflat.h"

namespace stateparse {

static bool complete(const char* b) {
    if (!b) return false;
    while (*b == ' ' || *b == '\r' || *b == '\n' || *b == '\t') ++b;
    if (*b != '{') return false;
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\r' || b[n - 1] == '\n' || b[n - 1] == '\t')) --n;
    return n > 1 && b[n - 1] == '}';
}

static uint32_t u32(const char* b, const char* k) {
    long v = 0;
    return jsonflat::num(b, k, &v) && v > 0 ? (uint32_t)v : 0;
}

bool parse(const char* body, Deck& out) {
    if (!complete(body)) return false;
    Deck d{};
    d.trackGen = u32(body, "trackGen");
    d.progressMs = u32(body, "progressMs");
    d.durationMs = u32(body, "durationMs");
    d.tripS = u32(body, "tripS");
    d.lineStartMs = u32(body, "lineStartMs");
    d.nextStartMs = u32(body, "nextStartMs");
    jsonflat::boolean(body, "playing", &d.playing);
    d.hasVroom = jsonflat::arrayHas(body, "clips", "vroom");
    jsonflat::str(body, "track", d.track, sizeof d.track);
    jsonflat::str(body, "artist", d.artist, sizeof d.artist);
    jsonflat::str(body, "status", d.status, sizeof d.status);
    jsonflat::str(body, "detail", d.detail, sizeof d.detail);
    if (!jsonflat::str(body, "lyricKind", d.lyricKind, sizeof d.lyricKind))
        strcpy(d.lyricKind, "none");
    jsonflat::str(body, "line", d.line, sizeof d.line);
    jsonflat::str(body, "next", d.next, sizeof d.next);
    out = d;
    return true;
}

}
```

- [ ] **Step 4: Run to verify they pass**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=43 fail=0`.

- [ ] **Step 5: Commit**

```bash
git add src/util/jsonflat.* src/util/stateparse.* test/test_jsonflat test/test_stateparse
git commit -m "feat(companion): flat JSON reader and /state parser"
```

---

### Task 3: Lyric clock and poll pacing (pure)

**Files:**
- Create: `src/util/lyricclock.h`, `src/util/pollpace.h`
- Test: `test/test_lyricclock/test_lyricclock.cpp`

**Interfaces:**
- Consumes: `stateparse::Deck` (Task 2).
- Produces: `lyricclock::View{ const char* big; const char* small; }` and `lyricclock::View lyricclock::pick(const stateparse::Deck& d, uint32_t sincePollMs)`
- Produces: `pollpace::Pace{ void onResult(bool ok); uint32_t intervalMs() const; int misses() const; }` with `FAST_MS = 1000`, `SLOW_MS = 5000`, `MISSES_FOR_SLOW = 10`

- [ ] **Step 1: Write the failing test**

`test/test_lyricclock/test_lyricclock.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/lyricclock.h"
#include "../../src/util/pollpace.h"

void setUp() {}
void tearDown() {}

static stateparse::Deck deck() {
    stateparse::Deck d{};
    d.trackGen = 3;
    d.playing = true;
    d.progressMs = 61000;
    d.lineStartMs = 59000;
    d.nextStartMs = 64000;
    strcpy(d.lyricKind, "synced");
    strcpy(d.track, "Made-up Song");
    strcpy(d.artist, "Made-up Artist");
    strcpy(d.line, "first made-up line");
    strcpy(d.next, "second made-up line");
    return d;
}

void test_shows_the_line_until_the_next_one_starts() {
    stateparse::Deck d = deck();
    lyricclock::View v = lyricclock::pick(d, 2000);           // 63.0 s
    TEST_ASSERT_EQUAL_STRING("first made-up line", v.big);
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.small);
    v = lyricclock::pick(d, 3000);                             // 64.0 s: next starts
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.big);
    TEST_ASSERT_EQUAL_STRING("", v.small);                     // unknown until the next poll
}

void test_paused_song_doesnt_advance() {
    stateparse::Deck d = deck();
    d.playing = false;
    TEST_ASSERT_EQUAL_STRING("first made-up line", lyricclock::pick(d, 9000).big);
}

void test_intro_shows_the_upcoming_first_line_small() {
    stateparse::Deck d = deck();
    d.line[0] = '\0';
    d.lineStartMs = 0;
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Made-up Song", v.big);           // title while nothing is sung
    TEST_ASSERT_EQUAL_STRING("second made-up line", v.small);
}

void test_no_lyrics_shows_title_and_artist() {
    stateparse::Deck d = deck();
    strcpy(d.lyricKind, "none");
    d.line[0] = d.next[0] = '\0';
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Made-up Song", v.big);
    TEST_ASSERT_EQUAL_STRING("Made-up Artist", v.small);
}

void test_song_change_drops_the_old_lines() {
    // The deck already moved on (new trackGen) but still searches lyrics: no old lines.
    stateparse::Deck d = deck();
    d.trackGen = 4;
    strcpy(d.lyricKind, "searching");
    d.line[0] = d.next[0] = '\0';
    strcpy(d.track, "Next Made-up Song");
    lyricclock::View v = lyricclock::pick(d, 0);
    TEST_ASSERT_EQUAL_STRING("Next Made-up Song", v.big);
    TEST_ASSERT_TRUE(strcmp(v.small, "second made-up line") != 0);
}

void test_poll_pace_slows_after_ten_misses_and_recovers() {
    pollpace::Pace p;
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    for (int i = 0; i < 9; ++i) p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    p.onResult(false);
    TEST_ASSERT_EQUAL_UINT32(pollpace::SLOW_MS, p.intervalMs());
    p.onResult(true);
    TEST_ASSERT_EQUAL_UINT32(pollpace::FAST_MS, p.intervalMs());
    TEST_ASSERT_EQUAL_INT(0, p.misses());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_shows_the_line_until_the_next_one_starts);
    RUN_TEST(test_paused_song_doesnt_advance);
    RUN_TEST(test_intro_shows_the_upcoming_first_line_small);
    RUN_TEST(test_no_lyrics_shows_title_and_artist);
    RUN_TEST(test_song_change_drops_the_old_lines);
    RUN_TEST(test_poll_pace_slows_after_ten_misses_and_recovers);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: FAIL `test_lyricclock` (missing headers).

- [ ] **Step 3: Implement**

`src/util/lyricclock.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstring>
#include "stateparse.h"
// What the companion's lyric area shows between two polls: the big line and the small dim one.
// The CYD switches to the next line on its own clock (progress + time since the poll), so line
// changes land on time although it only polls once a second. PURE, host-tested.
namespace lyricclock {
struct View { const char* big; const char* small; };

inline View pick(const stateparse::Deck& d, uint32_t sincePollMs) {
    const bool lyrics = strcmp(d.lyricKind, "synced") == 0 || strcmp(d.lyricKind, "plain") == 0;
    if (lyrics) {
        const uint32_t pos = d.progressMs + (d.playing ? sincePollMs : 0);
        if (d.next[0] && d.nextStartMs && pos >= d.nextStartMs) return {d.next, ""};
        if (d.line[0]) return {d.line, d.next};
        return {d.track, d.next};                 // intro: title big, the first line coming up
    }
    return {d.track, d.artist};                   // searching / none / instrumental
}
}
```
`src/util/pollpace.h`:
```cpp
#pragma once
#include <cstdint>
// How often the companion asks the new deck for /state: every second, every 5 s after ten
// failures in a row (the deck is off or out of range), back to every second on the first
// answer. PURE, host-tested (test_lyricclock).
namespace pollpace {
constexpr uint32_t FAST_MS = 1000, SLOW_MS = 5000;
constexpr int MISSES_FOR_SLOW = 10;
class Pace {
public:
    void onResult(bool ok) { misses_ = ok ? 0 : misses_ + 1; }
    uint32_t intervalMs() const { return misses_ >= MISSES_FOR_SLOW ? SLOW_MS : FAST_MS; }
    int misses() const { return misses_; }
private:
    int misses_ = 0;
};
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=44 fail=0`.

- [ ] **Step 5: Commit**

```bash
git add src/util/lyricclock.h src/util/pollpace.h test/test_lyricclock
git commit -m "feat(companion): lyric line clock and poll pacing"
```

---

### Task 4: Weather parser and icon mapping (pure)

**Files:**
- Create: `src/util/weather.{h,cpp}`
- Test: `test/test_weather/test_weather.cpp`

**Interfaces:**
- Consumes: `jsonflat::real/num` (Task 2).
- Produces: `enum class weather::Icon { Sun, Moon, Cloud, Rain, Storm }`, `struct weather::Now { double tempC; int code; bool day; }`, `bool weather::parse(const char* body, Now* out)`, `weather::Icon weather::iconFor(int code, bool day)`, `void weather::url(const char* lat, const char* lon, char* out, size_t n)`

- [ ] **Step 1: Write the failing test**

`test/test_weather/test_weather.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/weather.h"

void setUp() {}
void tearDown() {}

// Shape of an Open-Meteo answer for ?current=temperature_2m,weather_code,is_day
static const char* ANSWER =
    "{\"latitude\":-23.5,\"longitude\":-46.625,\"current_units\":{\"time\":\"iso8601\","
    "\"interval\":\"seconds\",\"temperature_2m\":\"°C\",\"weather_code\":\"wmo code\",\"is_day\":\"\"},"
    "\"current\":{\"time\":\"2026-10-13T13:45\",\"interval\":900,\"temperature_2m\":28.3,"
    "\"weather_code\":3,\"is_day\":1}}";

void test_reads_current_not_units() {
    weather::Now n{};
    TEST_ASSERT_TRUE(weather::parse(ANSWER, &n));
    TEST_ASSERT_TRUE(n.tempC > 28.29 && n.tempC < 28.31);
    TEST_ASSERT_EQUAL_INT(3, n.code);
    TEST_ASSERT_TRUE(n.day);
}

void test_rejects_incomplete_answers() {
    weather::Now n{};
    TEST_ASSERT_FALSE(weather::parse("{\"error\":true,\"reason\":\"bad lat\"}", &n));
    TEST_ASSERT_FALSE(weather::parse("", &n));
    TEST_ASSERT_FALSE(weather::parse(nullptr, &n));
}

void test_icons_for_wmo_codes() {
    using weather::Icon;
    TEST_ASSERT_EQUAL_INT((int)Icon::Sun, (int)weather::iconFor(0, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Moon, (int)weather::iconFor(1, false));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(3, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(45, true));    // fog
    TEST_ASSERT_EQUAL_INT((int)Icon::Rain, (int)weather::iconFor(61, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Rain, (int)weather::iconFor(81, false));
    TEST_ASSERT_EQUAL_INT((int)Icon::Storm, (int)weather::iconFor(95, true));
    TEST_ASSERT_EQUAL_INT((int)Icon::Cloud, (int)weather::iconFor(73, true));    // snow: rare here
}

void test_builds_the_request_url() {
    char u[200];
    weather::url("-23.55", "-46.63", u, sizeof u);
    TEST_ASSERT_EQUAL_STRING("https://api.open-meteo.com/v1/forecast?latitude=-23.55&longitude=-46.63"
                             "&current=temperature_2m,weather_code,is_day", u);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_current_not_units);
    RUN_TEST(test_rejects_incomplete_answers);
    RUN_TEST(test_icons_for_wmo_codes);
    RUN_TEST(test_builds_the_request_url);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: FAIL `test_weather` (missing header).

- [ ] **Step 3: Implement**

`src/util/weather.h`:
```cpp
#pragma once
#include <cstddef>
// Outside weather for the companion's top strip, from Open-Meteo (free, no key). PURE.
namespace weather {
enum class Icon { Sun, Moon, Cloud, Rain, Storm };
struct Now { double tempC; int code; bool day; };
// Reads the "current" block (current_units repeats the keys with string values first).
bool parse(const char* body, Now* out);
Icon iconFor(int wmoCode, bool day);
void url(const char* lat, const char* lon, char* out, size_t n);
}
```
`src/util/weather.cpp`:
```cpp
#include "weather.h"
#include <cstdio>
#include <cstring>
#include "jsonflat.h"

namespace weather {

bool parse(const char* body, Now* out) {
    if (!body || !out) return false;
    const char* cur = strstr(body, "\"current\":{");
    if (!cur) return false;
    cur += strlen("\"current\":");
    double t = 0;
    long code = 0, day = 1;
    if (!jsonflat::real(cur, "temperature_2m", &t) || !jsonflat::num(cur, "weather_code", &code))
        return false;
    jsonflat::num(cur, "is_day", &day);
    *out = {t, (int)code, day != 0};
    return true;
}

Icon iconFor(int c, bool day) {
    if (c >= 95) return Icon::Storm;
    if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return Icon::Rain;
    if (c <= 1) return day ? Icon::Sun : Icon::Moon;
    return Icon::Cloud;   // 2-3 cloudy, 45/48 fog, 71-77/85-86 snow
}

void url(const char* lat, const char* lon, char* out, size_t n) {
    snprintf(out, n,
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,weather_code,is_day",
             lat, lon);
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=45 fail=0`.

- [ ] **Step 5: Commit**

```bash
git add src/util/weather.* test/test_weather
git commit -m "feat(companion): Open-Meteo parser and weather icons"
```

---

### Task 5: Dashboard layout, press logic and strip text (pure)

**Files:**
- Create: `src/util/dash.{h,cpp}`
- Test: `test/test_dash/test_dash.cpp`

**Interfaces:**
- Produces: `struct dash::Rect { int x, y, w, h; }`, `enum class dash::Button { None, Howl, Vroom, Trip }`, constants `dash::STRIP_H = 28`, `dash::BAR_Y = 192`, `dash::BAR_H = 48`, `dash::LONG_PRESS_MS = 1000`
- Produces: `int dash::buttons(bool hasVroom, Button* ids, Rect* rects)` (returns 2 or 3), `dash::Button dash::hit(int x, int y, bool hasVroom)`
- Produces: `class dash::Press { Button update(bool touching, int x, int y, uint32_t nowMs, bool hasVroom); Button down() const; }` — returns the button to act on this frame (Howl/Vroom on touch-down, Trip once after 1 s held), else None
- Produces: `void dash::clockText(int minuteOfDay, char*, size_t)`, `void dash::tripText(uint32_t s, char*, size_t)`, `void dash::tempText(bool valid, double c, char*, size_t)`

- [ ] **Step 1: Write the failing test**

`test/test_dash/test_dash.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/dash.h"

using dash::Button;

void setUp() {}
void tearDown() {}

void test_buttons_fill_the_bar() {
    Button ids[3];
    dash::Rect r[3];
    TEST_ASSERT_EQUAL_INT(2, dash::buttons(false, ids, r));
    TEST_ASSERT_EQUAL_INT((int)Button::Howl, (int)ids[0]);
    TEST_ASSERT_EQUAL_INT((int)Button::Trip, (int)ids[1]);
    TEST_ASSERT_EQUAL_INT(3, dash::buttons(true, ids, r));
    TEST_ASSERT_EQUAL_INT((int)Button::Vroom, (int)ids[1]);
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(r[i].x >= 0 && r[i].x + r[i].w <= 320);
        TEST_ASSERT_TRUE(r[i].y >= dash::BAR_Y && r[i].y + r[i].h <= 240);
        TEST_ASSERT_TRUE(r[i].w >= 80);   // a thumb on a bumpy road
    }
    TEST_ASSERT_TRUE(r[0].x + r[0].w < r[1].x);   // gaps between buttons
}

void test_hit_test() {
    Button ids[3];
    dash::Rect r[3];
    dash::buttons(true, ids, r);
    TEST_ASSERT_EQUAL_INT((int)Button::Howl, (int)dash::hit(r[0].x + 5, r[0].y + 5, true));
    TEST_ASSERT_EQUAL_INT((int)Button::Trip, (int)dash::hit(r[2].x + r[2].w - 1, r[2].y + r[2].h - 1, true));
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)dash::hit(160, 100, true));   // lyric area
}

void test_sound_fires_once_at_touch_down() {
    Button ids[3];
    dash::Rect r[3];
    dash::buttons(false, ids, r);
    dash::Press p;
    const int x = r[0].x + 10, y = r[0].y + 10;
    TEST_ASSERT_EQUAL_INT((int)Button::Howl, (int)p.update(true, x, y, 0, false));
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(true, x, y, 50, false));    // held
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(false, 0, 0, 100, false));  // released
    TEST_ASSERT_EQUAL_INT((int)Button::Howl, (int)p.update(true, x, y, 200, false));   // new press
}

void test_trip_fires_once_per_hold() {
    Button ids[3];
    dash::Rect r[3];
    dash::buttons(false, ids, r);
    dash::Press p;
    const int x = r[1].x + 10, y = r[1].y + 10;
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(true, x, y, 0, false));
    TEST_ASSERT_EQUAL_INT((int)Button::Trip, (int)p.down());                            // highlighted
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(true, x, y, 999, false));
    TEST_ASSERT_EQUAL_INT((int)Button::Trip, (int)p.update(true, x, y, 1000, false));
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(true, x, y, 5000, false));   // still held
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(false, 0, 0, 5100, false));
    // a short tap never resets
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(true, x, y, 6000, false));
    TEST_ASSERT_EQUAL_INT((int)Button::None, (int)p.update(false, 0, 0, 6300, false));
}

void test_button_is_chosen_at_touch_down() {
    Button ids[3];
    dash::Rect r[3];
    dash::buttons(false, ids, r);
    dash::Press p;
    p.update(true, r[0].x + 10, r[0].y + 10, 0, false);                 // on Howl (fires)
    TEST_ASSERT_EQUAL_INT((int)Button::None,
                          (int)p.update(true, r[1].x + 10, r[1].y + 10, 1500, false));   // slid onto Trip
}

void test_strip_texts() {
    char s[16];
    dash::clockText(13 * 60 + 42, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("13:42", s);
    dash::clockText(-1, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("--:--", s);
    dash::tripText(37 * 60 + 59, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("0:37", s);
    dash::tripText(12 * 3600 + 5 * 60, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("12:05", s);
    dash::tempText(true, 28.3, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("28", s);
    dash::tempText(true, -0.6, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("-1", s);
    dash::tempText(false, 0, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("--", s);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_buttons_fill_the_bar);
    RUN_TEST(test_hit_test);
    RUN_TEST(test_sound_fires_once_at_touch_down);
    RUN_TEST(test_trip_fires_once_per_hold);
    RUN_TEST(test_button_is_chosen_at_touch_down);
    RUN_TEST(test_strip_texts);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: FAIL `test_dash` (missing header).

- [ ] **Step 3: Implement**

`src/util/dash.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
// Companion dashboard geometry, touch handling and top-strip text. PURE, host-tested.
namespace dash {
struct Rect { int x, y, w, h; };
enum class Button { None, Howl, Vroom, Trip };
constexpr int W = 320, H = 240, STRIP_H = 28, BAR_Y = 192, BAR_H = 48, GAP = 6;
constexpr uint32_t LONG_PRESS_MS = 1000;   // Trip: a bump on the road must not reset it

// Buttons left to right: Howl, [Vroom], Trip. Returns how many.
int buttons(bool hasVroom, Button* ids, Rect* rects);
Button hit(int x, int y, bool hasVroom);

// One touch = at most one action. The button is chosen at touch-down; sounds fire at once,
// Trip fires once after LONG_PRESS_MS of holding. Sliding onto another button does nothing.
class Press {
public:
    Button update(bool touching, int x, int y, uint32_t nowMs, bool hasVroom);
    Button down() const { return down_; }   // highlighted while touched
private:
    Button down_ = Button::None;
    uint32_t sinceMs_ = 0;
    bool fired_ = false;
};

void clockText(int minuteOfDay, char* out, size_t n);   // "13:42"; < 0 -> "--:--"
void tripText(uint32_t seconds, char* out, size_t n);   // h:mm
void tempText(bool valid, double c, char* out, size_t n);   // rounded; "--" when invalid
}
```
`src/util/dash.cpp`:
```cpp
#include "dash.h"
#include <cmath>
#include <cstdio>

namespace dash {

int buttons(bool hasVroom, Button* ids, Rect* rects) {
    const int n = hasVroom ? 3 : 2;
    const Button order3[3] = {Button::Howl, Button::Vroom, Button::Trip};
    const Button order2[2] = {Button::Howl, Button::Trip};
    const int w = (W - GAP * (n + 1)) / n;
    for (int i = 0; i < n; ++i) {
        ids[i] = hasVroom ? order3[i] : order2[i];
        rects[i] = {GAP + i * (w + GAP), BAR_Y + 2, w, BAR_H - 6};
    }
    return n;
}

Button hit(int x, int y, bool hasVroom) {
    Button ids[3];
    Rect r[3];
    const int n = buttons(hasVroom, ids, r);
    for (int i = 0; i < n; ++i)
        if (x >= r[i].x && x < r[i].x + r[i].w && y >= r[i].y && y < r[i].y + r[i].h) return ids[i];
    return Button::None;
}

Button Press::update(bool touching, int x, int y, uint32_t nowMs, bool hasVroom) {
    if (!touching) {
        down_ = Button::None;
        fired_ = false;
        return Button::None;
    }
    if (down_ == Button::None && !fired_) {   // touch-down
        down_ = hit(x, y, hasVroom);
        sinceMs_ = nowMs;
        if (down_ == Button::None) { fired_ = true; return Button::None; }   // outside: ignore this touch
        if (down_ != Button::Trip) { fired_ = true; return down_; }
        return Button::None;
    }
    if (down_ == Button::Trip && !fired_ && nowMs - sinceMs_ >= LONG_PRESS_MS) {
        fired_ = true;
        return Button::Trip;
    }
    return Button::None;
}

void clockText(int m, char* out, size_t n) {
    if (m < 0) snprintf(out, n, "--:--");
    else snprintf(out, n, "%02d:%02d", (m / 60) % 24, m % 60);
}

void tripText(uint32_t s, char* out, size_t n) {
    snprintf(out, n, "%u:%02u", (unsigned)(s / 3600), (unsigned)(s % 3600 / 60));
}

void tempText(bool valid, double c, char* out, size_t n) {
    if (!valid) snprintf(out, n, "--");
    else snprintf(out, n, "%d", (int)lround(c));
}

}
```
Note for `test_button_is_chosen_at_touch_down`: after Howl fires, `fired_` is true and `down_` stays Howl, so sliding onto Trip returns None until release. For `test_trip_fires_once_per_hold`'s short tap: touch at 6000 then release at 6300 never reaches 1000 ms.

- [ ] **Step 4: Run to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=46 fail=0`.

- [ ] **Step 5: Commit**

```bash
git add src/util/dash.* test/test_dash
git commit -m "feat(companion): dashboard layout, press logic and strip text"
```

---

### Task 6: Font-2 text at 2x scale

**Files:**
- Modify: `src/ui/textdraw.h`, `src/ui/textdraw.cpp`

**Interfaces:**
- Produces: `int ui::drawRunScaled(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg, uint16_t shadow, uint8_t datum, int scale)`; `drawRun(...)` becomes `drawRunScaled(..., 1)`. Item widths stay scale-1 (wrap with `maxW / scale`).

- [ ] **Step 1: Implement** (device-only drawing; verified on the panel in Task 9)

`src/ui/textdraw.h`, after `drawRun`:
```cpp
// drawRun at an integer scale (2 = the companion's big lyric line): font 2 via setTextSize,
// accent marks and CJK glyphs pixel-doubled. Items keep their scale-1 widths.
int drawRunScaled(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
                  uint16_t shadow, uint8_t datum, int scale);
```
`src/ui/textdraw.cpp`: give `drawMark` and `drawWide` an `int s` parameter and draw `t.fillRect(px, py, s, s, col)` at `(cx - accents::W / 2 * s + x * s, r0 + y * s)` / `(x + c * s, top + (CJK_DY + r) * s)` where `r0 = top + accents::topRow(m, base) * s`. Replace `drawRun` with:
```cpp
int drawRunScaled(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
                  uint16_t shadow, uint8_t datum, int scale) {
    const int s = scale < 1 ? 1 : scale;
    int w = glyphrun::width(it, n) * s, h = t.fontHeight(2) * s;
    int left = x, top = y;
    switch (datum) {
        case TC_DATUM: left = x - w / 2; break;
        case TR_DATUM: left = x - w; break;
        case ML_DATUM: top = y - h / 2; break;
        case MC_DATUM: left = x - w / 2; top = y - h / 2; break;
        default: break;   // TL_DATUM
    }
    int px = left;
    char one[2] = {0, 0};
    t.setTextSize(s);
    for (size_t i = 0; i < n; ++i) {
        const glyphrun::Item& g = it[i];
        if (g.kind == glyphrun::Kind::Wide) {
            drawWide(t, g.cp, px + s, top + s, shadow, s);   // shadow first, like shadowText
            drawWide(t, g.cp, px, top, fg, s);
        } else {
            one[0] = g.ch;
            t.setTextDatum(TL_DATUM);
            t.setTextColor(shadow);
            t.drawString(one, px + s, top + s, 2);
            t.setTextColor(fg);
            t.drawString(one, px, top, 2);
            if (g.mark != txt::Mark::None) {
                int cx = px + (g.w * s - 1) / 2;
                drawMark(t, g.mark, g.ch, cx + s, top + s, shadow, s);
                drawMark(t, g.mark, g.ch, cx, top, fg, s);
            }
        }
        px += g.w * s;
    }
    t.setTextSize(1);
    return w;
}

int drawRun(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
            uint16_t shadow, uint8_t datum) {
    return drawRunScaled(t, it, n, x, y, fg, shadow, datum, 1);
}
```
(At scale 1 this draws exactly what the old `drawRun` did: `shadowText` is shadow at +1,+1 then the colour, transparent background.)

- [ ] **Step 2: Build both deck targets (the deck must look unchanged)**

Run: `pio run -e s3` and `pio run -e cyd`
Expected: both SUCCESS; `cyd` flash still below 97 %.

- [ ] **Step 3: Commit**

```bash
git add src/ui/textdraw.h src/ui/textdraw.cpp
git commit -m "feat(ui): draw font-2 runs at an integer scale"
```

---

### Task 7: `cyd_companion` build, link task and touch

**Files:**
- Modify: `platformio.ini`, `include/config.example.h`, `include/pins.h`
- Create: `src/companion/link.h`, `src/companion/link.cpp`, `src/companion/touch.h`, `src/companion/touch.cpp`

**Interfaces:**
- Consumes: `stateparse::parse`, `pollpace::Pace`, `weather::parse/url`, `netcfg::linkUp/secure/streamed`, `net::connectAny/isOnline/loop`.
- Produces (`link.h`):
```cpp
namespace link {
struct View {
    bool deckOk;               // last /state poll answered (green dot)
    bool wrongKey;             // 403 from the deck
    uint32_t polledAtMs;       // millis() of that answer
    stateparse::Deck deck;     // last good state
    bool weatherOk; double tempC; int code; bool day; uint32_t weatherAtMs;
    const char* lastAction;    // "Playing!", "Busy", ... shown under the buttons; "" none
    uint32_t actionAtMs;
};
void start();                  // network task on core 0
void snapshot(View& out);      // copy under the lock (UI loop)
enum class Action { None, Howl, Vroom, TripReset };
void request(Action a);        // from the UI loop; done by the network task
}
```
- Produces (`touch.h`): `namespace touch { void begin(); bool read(int* x, int* y); }` (screen coordinates, rotation 1)

- [ ] **Step 1: Config and pins**

`include/config.example.h`, after `REMOTE_TOKEN`:
```cpp
// Companion screen (env cyd_companion): the new deck it reads, and the home location for the
// weather (decimal degrees, as text). The deck's fixed hotspot address is 172.20.10.10.
#define COMPANION_DECK        "172.20.10.10"
#define HOME_LAT              "-23.55"
#define HOME_LON              "-46.63"
```
`include/pins.h`, in the CYD block next to the touch pins:
```cpp
// Touch calibration (XPT2046 raw -> 320x240, rotation 1), measured with env:hwcheck.
static const int TOUCH_RAW_X0 = 200, TOUCH_RAW_X1 = 3900;
static const int TOUCH_RAW_Y0 = 200, TOUCH_RAW_Y1 = 3900;
```

`platformio.ini`, after `[env:hwcheck]`:
```ini
; The CYD as a companion screen for the S3 (docs/superpowers/specs/2026-10-13-companion-screen-design.md):
; no Spotify, no Pokemon; reads the S3's /state over the hotspot.
;   pio run -e cyd_companion -t upload --upload-port COM14
[env:cyd_companion]
extends = env:cyd
board_build.embed_files = data/cjk16.bin
lib_deps =
    ${common.lib_deps}
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git#f956c5d
build_flags =
    ${env:cyd.build_flags}
    -D CARDECK_COMPANION=1
build_src_filter = -<*> +<companion/> +<util/> +<ui/battle.cpp> +<ui/theme.cpp> +<ui/textdraw.cpp>
    +<ui/accents.cpp> +<ui/cjkdata.cpp> +<ui/cjkfont.cpp> +<ui/backlight.cpp> +<net/wifi.cpp>
    +<net/clock.cpp> +<net/http_config.cpp> +<board/cyd/board_cyd.cpp> +<core/mem.cpp>
```
(If the linker names a missing symbol from another `src/ui/` or `src/util/` file, add that one file to the filter and note it in the ledger as a ruling; do not add `spotify/`, `pokemon/`, `images/`, `lyrics/`, `genre/`, `core/nettask.cpp`, `core/shared.cpp` or `main.cpp`.)

- [ ] **Step 2: Touch driver**

`src/companion/touch.h`:
```cpp
#pragma once
// XPT2046 resistive touch of the CYD on the SPI host the display doesn't use (VSPI).
namespace touch {
void begin();
bool read(int* x, int* y);   // true while pressed; screen coordinates (rotation 1)
}
```
`src/companion/touch.cpp`:
```cpp
#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"

namespace touch {
static SPIClass s_spi(VSPI);   // the display is on HSPI (GPIO 12-15)
static XPT2046_Touchscreen s_ts(TOUCH_CS, TOUCH_IRQ);

void begin() {
    s_spi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    s_ts.begin(s_spi);
    s_ts.setRotation(1);
}

bool read(int* x, int* y) {
    if (!s_ts.touched()) return false;
    TS_Point p = s_ts.getPoint();
    if (p.z < 200) return false;                      // too light: noise
    *x = constrain(map(p.x, TOUCH_RAW_X0, TOUCH_RAW_X1, 0, 320), 0, 319);
    *y = constrain(map(p.y, TOUCH_RAW_Y0, TOUCH_RAW_Y1, 0, 240), 0, 239);
    return true;
}
}
```

- [ ] **Step 3: Link task**

`src/companion/link.cpp`:
```cpp
#include "link.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "../config.h"
#include "../net/wifi.h"
#include "../net/http_config.h"
#include "../util/pollpace.h"
#include "../util/readguard.h"
#include "../util/weather.h"

#ifndef COMPANION_DECK
#define COMPANION_DECK "172.20.10.10"
#endif
#ifndef HOME_LAT
#define HOME_LAT ""   // no location: no weather
#define HOME_LON ""
#endif
#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""
#endif

namespace link {
static SemaphoreHandle_t s_lock = nullptr;
static View s_view{};
static volatile Action s_action = Action::None;
static char s_body[1536];   // /state is < 1 KB; static: off the task stack
constexpr uint32_t WEATHER_MS = 15UL * 60 * 1000;

void request(Action a) { s_action = a; }

void snapshot(View& out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out = s_view;
    xSemaphoreGive(s_lock);
}

// Plain-HTTP GET into s_body; returns the status (negative: no answer).
static int get(const char* path) {
    if (!netcfg::linkUp()) return -1;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(1500);
    h.setConnectTimeout(1500);
    char url[160];
    snprintf(url, sizeof url, "http://%s%s", COMPANION_DECK, path);
    if (!h.begin(c, url)) return -1;
    int rc = h.GET();
    size_t len = 0;
    if (rc == 200) {
        WiFiClient* s = h.getStreamPtr();
        readguard::Guard g(millis(), 1500, 3000);
        while (len < sizeof s_body - 1 && (h.connected() || s->available())) {
            int a = s->available();
            if (a <= 0) { if (g.check(millis()) != readguard::State::Ok) break; delay(1); continue; }
            size_t want = sizeof s_body - 1 - len;
            if ((size_t)a < want) want = (size_t)a;
            len += s->readBytes(s_body + len, want);
            g.onData(millis());
        }
    }
    s_body[len] = '\0';
    h.end();
    return rc;
}

static int post(const char* path) {
    if (!netcfg::linkUp()) return -1;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(1500);
    h.setConnectTimeout(1500);
    char url[160];
    snprintf(url, sizeof url, "http://%s%s", COMPANION_DECK, path);
    if (!h.begin(c, url)) return -1;
    int rc = h.POST("");
    h.end();
    return rc;
}

static void setAction(const char* text) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_view.lastAction = text;
    s_view.actionAtMs = millis();
    xSemaphoreGive(s_lock);
}

static void doAction(Action a) {
    char path[96];
    if (a == Action::TripReset) snprintf(path, sizeof path, "/trip/reset?k=%s", REMOTE_TOKEN);
    else snprintf(path, sizeof path, "/play?clip=%s&k=%s", a == Action::Vroom ? "vroom" : "howl", REMOTE_TOKEN);
    const int rc = post(path);
    switch (rc) {
    case 202: setAction("Playing!"); break;
    case 204: setAction("Trip reset"); break;
    case 403: setAction("Wrong key"); break;
    case 409: setAction("Busy"); break;
    case 429: setAction("Wait a moment"); break;
    case 503: setAction("Deck low on memory"); break;
    default:  setAction("Deck not reachable"); break;
    }
    Serial.printf("[companion] action %d -> %d\n", (int)a, rc);
}

static bool fetchWeather(weather::Now* out) {
    if (!HOME_LAT[0] || !netcfg::linkUp()) return false;
    char url[200];
    weather::url(HOME_LAT, HOME_LON, url, sizeof url);
    WiFiClientSecure c;
    netcfg::secure(c);   // public data: no certificate check (SECURITY.md)
    HTTPClient h;
    netcfg::streamed(h);
    if (!h.begin(c, url)) return false;
    bool ok = false;
    if (h.GET() == 200) {
        String body = h.getString();   // ~400 B
        ok = weather::parse(body.c_str(), out);
    }
    h.end();
    return ok;
}

static void run(void*) {
    pollpace::Pace pace;
    uint32_t lastPoll = 0, lastWeather = 0;
    bool firstWeather = true;
    for (;;) {
        net::loop();
        const Action a = s_action;
        if (a != Action::None) { s_action = Action::None; doAction(a); }
        const uint32_t now = millis();
        if (now - lastPoll >= pace.intervalMs()) {
            lastPoll = now;
            char path[64];
            snprintf(path, sizeof path, "/state?k=%s", REMOTE_TOKEN);
            const int rc = get(path);
            static stateparse::Deck d;   // static: ~600 B
            const bool ok = rc == 200 && stateparse::parse(s_body, d);
            pace.onResult(ok);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_view.wrongKey = rc == 403;
            if (ok) { s_view.deck = d; s_view.polledAtMs = millis(); }
            s_view.deckOk = ok;
            xSemaphoreGive(s_lock);
        }
        if (firstWeather || now - lastWeather >= WEATHER_MS) {
            weather::Now w{};
            if (fetchWeather(&w)) {
                firstWeather = false;
                lastWeather = now;
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_view.weatherOk = true;
                s_view.tempC = w.tempC; s_view.code = w.code; s_view.day = w.day;
                s_view.weatherAtMs = millis();
                xSemaphoreGive(s_lock);
                Serial.printf("[weather] %.1f C code %d\n", w.tempC, w.code);
            } else {
                lastWeather = now - WEATHER_MS + 60000;   // failed: try again in a minute
                firstWeather = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void start() {
    s_lock = xSemaphoreCreateMutex();
    s_view.lastAction = "";
    xTaskCreatePinnedToCore(run, "link", 8192, nullptr, 1, nullptr, 0);
}
}
```
`src/companion/link.h`: the `Produces` block above, with `#pragma once`, `#include <cstdint>` and `#include "../util/stateparse.h"`.

- [ ] **Step 4: Build**

Run: `pio run -e cyd_companion`
Expected: link error only for the missing `setup`/`loop` (Task 8 adds `app.cpp`). Any other undefined symbol: add its module to `build_src_filter` (ruling in the ledger).

- [ ] **Step 5: Commit**

```bash
git add platformio.ini include/config.example.h include/pins.h src/companion/link.* src/companion/touch.*
git commit -m "feat(companion): build target, /state and weather link task, touch driver"
```

---

### Task 8: Dashboard drawing and the companion app

**Files:**
- Create: `src/companion/dashboard.h`, `src/companion/dashboard.cpp`, `src/companion/app.cpp`

**Interfaces:**
- Consumes: `link::View/snapshot/request/start` (Task 7), `touch::begin/read` (Task 7), `lyricclock::pick` (Task 3), `dash::*` (Task 5), `ui::drawRunScaled` (Task 6), `glyphrun::decode/wrapTwo`, `ui::asciiWidth2/wideWidth`, `ui::drawText`, `ui::topStrip`, `ui::battleBox`, `theme::pal()`, `weather::iconFor`, `netclock::begin/minuteOfDay`, `daynight::isNight`, `backlight::begin/set`.
- Produces: `namespace dashboard { void drawAll(TFT_eSPI&, const link::View&, int minuteOfDay, dash::Button down, uint32_t nowMs); }` — repaints only the parts whose text changed since the last call.

- [ ] **Step 1: `dashboard.h`**
```cpp
#pragma once
#include <TFT_eSPI.h>
#include "link.h"
#include "../util/dash.h"
// The companion's single screen: top strip, lyric area, button bar. Each part is repainted
// only when what it shows changed (no flicker, little SPI time).
namespace dashboard {
void drawAll(TFT_eSPI& t, const link::View& v, int minuteOfDay, dash::Button down, uint32_t nowMs);
void invalidate();   // next drawAll repaints everything (night mode switch)
}
```

- [ ] **Step 2: `dashboard.cpp`**
```cpp
#include "dashboard.h"
#include <cstring>
#include "../ui/battle.h"
#include "../ui/textdraw.h"
#include "../ui/theme.h"
#include "../util/glyphrun.h"
#include "../util/lyricclock.h"
#include "../util/weather.h"

namespace dashboard {
static char s_strip[64] = "\x01", s_big[160] = "\x01", s_small[160] = "\x01", s_bar[64] = "\x01";

void invalidate() { s_strip[0] = s_big[0] = s_small[0] = s_bar[0] = '\x01'; }

static void weatherIcon(TFT_eSPI& t, weather::Icon ic, int x, int y) {   // 16x16, drawn in code
    const uint16_t sun = TFT_YELLOW, cloud = TFT_LIGHTGREY, rain = TFT_CYAN;
    switch (ic) {
    case weather::Icon::Sun:
        t.fillCircle(x + 8, y + 8, 5, sun);
        for (int i = 0; i < 8; ++i) {
            const float a = i * 0.785f;
            t.drawLine(x + 8 + (int)(7 * cosf(a)), y + 8 + (int)(7 * sinf(a)),
                       x + 8 + (int)(8 * cosf(a)), y + 8 + (int)(8 * sinf(a)), sun);
        }
        break;
    case weather::Icon::Moon:
        t.fillCircle(x + 8, y + 8, 6, sun);
        t.fillCircle(x + 11, y + 6, 5, theme::pal().top);
        break;
    case weather::Icon::Cloud:
    case weather::Icon::Rain:
    case weather::Icon::Storm:
        t.fillCircle(x + 6, y + 8, 4, cloud);
        t.fillCircle(x + 10, y + 6, 5, cloud);
        t.fillRect(x + 3, y + 8, 12, 4, cloud);
        if (ic != weather::Icon::Cloud)
            for (int i = 0; i < 3; ++i) t.drawLine(x + 4 + i * 4, y + 13, x + 3 + i * 4, y + 15, rain);
        if (ic == weather::Icon::Storm) t.drawLine(x + 9, y + 12, x + 7, y + 15, sun);
        break;
    }
}

static void strip(TFT_eSPI& t, const link::View& v, int minuteOfDay, uint32_t nowMs) {
    char clock[8], trip[12], temp[8], key[64];
    dash::clockText(minuteOfDay, clock, sizeof clock);
    dash::tripText(v.deckOk || v.polledAtMs ? v.deck.tripS : nowMs / 1000, trip, sizeof trip);
    const bool fresh = v.weatherOk && nowMs - v.weatherAtMs < 3600UL * 1000;   // 1 h, then "--"
    dash::tempText(fresh, v.tempC, temp, sizeof temp);
    const int bars = WiFi.status() == WL_CONNECTED ? (WiFi.RSSI() > -60 ? 3 : WiFi.RSSI() > -72 ? 2 : 1) : 0;
    snprintf(key, sizeof key, "%s|%s|%s|%d|%d|%d|%d", clock, trip, temp, fresh ? v.code : -1, v.day,
             bars, v.deckOk);
    if (strcmp(key, s_strip) == 0) return;
    strncpy(s_strip, key, sizeof s_strip);
    t.fillRect(0, 0, dash::W, dash::STRIP_H, theme::pal().top);
    t.setTextColor(theme::pal().topText);
    t.setTextDatum(ML_DATUM);
    t.drawString(clock, 8, 14, 4);
    char tripLine[20];
    snprintf(tripLine, sizeof tripLine, "trip %s", trip);
    t.drawString(tripLine, 92, 14, 2);
    if (fresh) weatherIcon(t, weather::iconFor(v.code, v.day), 178, 6);
    t.drawString(temp, 198, 14, 4);
    const int tx = 198 + t.textWidth(temp, 4) + 2;
    t.drawCircle(tx + 2, 7, 2, theme::pal().topText);   // degree sign (font 4 has none)
    t.drawString("C", tx + 6, 14, 2);
    for (int i = 0; i < 3; ++i)   // WiFi bars
        t.fillRect(276 + i * 6, 20 - (i + 1) * 4, 4, (i + 1) * 4,
                   i < bars ? theme::pal().topText : theme::pal().iconOff);
    if (!bars) t.drawString("No WiFi", 240, 14, 2);
    t.fillCircle(308, 14, 5, v.deckOk ? TFT_GREEN : theme::pal().iconOff);   // link to the deck
}

static void lyricArea(TFT_eSPI& t, const char* big, const char* small) {
    if (strcmp(big, s_big) == 0 && strcmp(small, s_small) == 0) return;
    strncpy(s_big, big, sizeof s_big - 1);
    strncpy(s_small, small, sizeof s_small - 1);
    const int y0 = dash::STRIP_H, h = dash::BAR_Y - dash::STRIP_H;
    ui::battleBox(t, 4, y0 + 4, dash::W - 8, h - 8, ui::Tab::None);
    constexpr size_t MAX = 160;
    static glyphrun::Item items[MAX + 3];
    size_t n = glyphrun::decode(big, items, MAX, ui::asciiWidth2, ui::wideWidth, &t);
    const int maxW = (dash::W - 32) / 2;                       // 2x scale
    glyphrun::Wrap w = glyphrun::wrapTwo(items, n, maxW, 3 * ui::asciiWidth2('.', &t));
    const uint16_t fg = theme::pal().text, sh = theme::pal().textShadow;
    const int cx = dash::W / 2;
    if (w.bStart >= w.bEnd) {
        ui::drawRunScaled(t, items, w.aEnd, cx, y0 + 52, fg, sh, MC_DATUM, 2);
    } else {
        ui::drawRunScaled(t, items, w.aEnd, cx, y0 + 34, fg, sh, MC_DATUM, 2);
        size_t bn = w.bEnd - w.bStart;
        if (w.bEllipsis)
            for (int k = 0; k < 3; ++k)
                items[w.bEnd + k] = {glyphrun::Kind::Ascii, '.', txt::Mark::None, '.',
                                     (uint8_t)ui::asciiWidth2('.', &t)};
        if (w.bEllipsis) bn += 3;
        ui::drawRunScaled(t, items + w.bStart, bn, cx, y0 + 70, fg, sh, MC_DATUM, 2);
    }
    ui::drawText(t, small, cx, y0 + h - 30, theme::pal().iconOff, sh, MC_DATUM, dash::W - 32);
}

static void bar(TFT_eSPI& t, const link::View& v, dash::Button down, uint32_t nowMs) {
    const bool showAction = v.lastAction && v.lastAction[0] && nowMs - v.actionAtMs < 2500;
    char key[64];
    snprintf(key, sizeof key, "%d|%d|%d|%s", v.deckOk, v.deck.hasVroom, (int)down,
             showAction ? v.lastAction : "");
    if (strcmp(key, s_bar) == 0) return;
    strncpy(s_bar, key, sizeof s_bar - 1);
    t.fillRect(0, dash::BAR_Y, dash::W, dash::BAR_H, theme::pal().grass);
    dash::Button ids[3];
    dash::Rect r[3];
    const int n = dash::buttons(v.deck.hasVroom, ids, r);
    for (int i = 0; i < n; ++i) {
        const bool on = v.deckOk;
        const uint16_t fill = !on ? theme::pal().iconOff : ids[i] == down ? theme::pal().horizon : theme::pal().boxFill;
        t.fillRoundRect(r[i].x, r[i].y, r[i].w, r[i].h, 8, fill);
        t.drawRoundRect(r[i].x, r[i].y, r[i].w, r[i].h, 8, theme::pal().boxBorder);
        const char* label = ids[i] == dash::Button::Howl ? "Howl" : ids[i] == dash::Button::Vroom ? "Vroom" : "Trip (hold)";
        t.setTextDatum(MC_DATUM);
        t.setTextColor(theme::pal().text);
        t.drawString(showAction && ids[i] == down ? v.lastAction : label, r[i].x + r[i].w / 2,
                     r[i].y + r[i].h / 2, 2);
    }
    if (showAction && down == dash::Button::None) {   // answer after release: over the bar centre
        t.setTextDatum(MC_DATUM);
        t.setTextColor(theme::pal().text);
        t.drawString(v.lastAction, dash::W / 2, dash::BAR_Y - 10, 2);
    }
}

void drawAll(TFT_eSPI& t, const link::View& v, int minuteOfDay, dash::Button down, uint32_t nowMs) {
    strip(t, v, minuteOfDay, nowMs);
    if (v.wrongKey) lyricArea(t, "Wrong key", "Check REMOTE_TOKEN in config.h on both decks");
    else if (!v.deckOk && !v.polledAtMs) lyricArea(t, "Looking for the deck...", COMPANION_HINT);
    else if (!v.deckOk) lyricArea(t, "Looking for the deck...", v.deck.track);
    else {
        lyricclock::View lv = lyricclock::pick(v.deck, nowMs - v.polledAtMs);
        lyricArea(t, lv.big, lv.small);
    }
    bar(t, v, down, nowMs);
}
}
```
At the top of `dashboard.cpp` add `#include <WiFi.h>`, `#include <cmath>` and:
```cpp
#ifndef COMPANION_DECK
#define COMPANION_DECK "172.20.10.10"
#endif
#define COMPANION_HINT "Is the new deck on, on the same hotspot?"
```
and `#include "../config.h"` before it.

- [ ] **Step 3: `app.cpp`**
```cpp
// Companion screen (env cyd_companion): the CYD beside the S3, fed by its /state.
// Spec: docs/superpowers/specs/2026-10-13-companion-screen-design.md
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../board.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../ui/backlight.h"
#include "../ui/theme.h"
#include "../util/daynight.h"
#include "../util/version.h"
#include "dashboard.h"
#include "link.h"
#include "touch.h"

static TFT_eSPI s_tft;
static dash::Press s_press;

void setup() {
    Serial.begin(115200);
    board::begin();
    Serial.printf("[boot] cardeck companion v%s\n", CARDECK_VERSION);
    s_tft.init(); s_tft.invertDisplay(true); s_tft.setRotation(1); s_tft.fillScreen(TFT_BLACK);
    backlight::begin();
    touch::begin();
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    s_tft.drawString("Connecting...", 10, 10, 2);
    net::connectAny();          // keeps retrying later from the link task (net::loop)
    netclock::begin();
    link::start();
    dashboard::invalidate();
}

void loop() {
    static uint32_t lastNightCheck = 0;
    int minute = -1;
    const bool synced = netclock::minuteOfDay(&minute);
    if (millis() - lastNightCheck > 60000 || lastNightCheck == 0) {
        lastNightCheck = millis();
        const bool night = synced && daynight::isNight(minute);
        if (night != theme::isNightActive()) {
            theme::setNight(night);
            backlight::set(night);
            dashboard::invalidate();
        }
    }
    link::View v;
    link::snapshot(v);
    int x = 0, y = 0;
    const bool touching = touch::read(&x, &y);
    const dash::Button fire = s_press.update(touching, x, y, millis(), v.deck.hasVroom);
    if (v.deckOk) {
        if (fire == dash::Button::Howl) link::request(link::Action::Howl);
        else if (fire == dash::Button::Vroom) link::request(link::Action::Vroom);
        else if (fire == dash::Button::Trip) link::request(link::Action::TripReset);
    }
    dashboard::drawAll(s_tft, v, synced ? minute : -1, s_press.down(), millis());
    delay(20);
}
```
Check the version macro name: `grep -n "define" src/util/version.h`; use whatever it defines (the deck's `main.cpp` prints `[boot] cardeck v%s` with it). If `theme::pal()` lacks `iconOff`, `topText`, `boxBorder` or `horizon`, use the closest `theme::Palette` field and ledger the ruling.

- [ ] **Step 4: Build and flash**

Run: `pio run -e cyd_companion -t upload --upload-port COM14` (old deck plugged in)
Expected: SUCCESS, flash below 97 %. Serial: `[boot] cardeck companion v...`, `[wifi] connected to iPhone ip=172.20.10.11` (or `fixed hotspot address 172.20.10.11`), `[weather] ...`.

- [ ] **Step 5: Commit**

```bash
git add src/companion/dashboard.* src/companion/app.cpp
git commit -m "feat(companion): dashboard screen and app"
```

---

### Task 9: On-device check, docs

**Files:**
- Modify: `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/BACKLOG.md`, `SECURITY.md`

- [ ] **Step 1: Touch calibration**

Flash `env:hwcheck` to the CYD, touch the four corners, read `[touch] raw x=.. y=..`, and put the min/max into `TOUCH_RAW_*` in `include/pins.h` if they differ from 200/3900 by more than ~100. Reflash `cyd_companion`.

- [ ] **Step 2: Check on the boards (both on the hotspot)**

| Check | Expected |
|---|---|
| Song playing on the S3 | CYD lyric area shows the line big, next line dim; line changes within ~0.3 s of the S3 |
| Unplug the S3 | grey dot, "Looking for the deck..." within ~2 s; clock/trip/weather still shown; plug back: green within ~5 s |
| Howl button | S3 howls; "Playing!" on the CYD |
| Trip: short tap | nothing; hold 1 s: trip shows 0:00 on the CYD and in `/state` |
| `[mem]` / free heap on the CYD | stable over 10 min |

- [ ] **Step 3: Docs**

- `README.md` Features: "**Companion screen:** a second (CYD) deck beside the first shows the clock, trip time, outside weather and the lyrics big, with touch buttons for sounds and a trip reset; it reads the first deck over the hotspot (env `cyd_companion`)." Quick start: how to flash it (`pio run -e cyd_companion -t upload`), the three new `config.h` options.
- `CHANGELOG.md` Unreleased / Added: the companion screen; `/state` timing, lyric pair and trip; `POST /trip/reset`.
- `docs/ARCHITECTURE.md`: a "Companion screen" bullet (builds, data flow, modules) and the new pure modules in the host-tested list (`util/{tripclock,jsonflat,stateparse,lyricclock,pollpace,weather,dash}`).
- `docs/BACKLOG.md`: Verification items left from Step 2; the map extra as its own plan; OBD next.
- `SECURITY.md`: the companion sends `REMOTE_TOKEN` in plain HTTP on the hotspot (same as the phone page).

- [ ] **Step 4: Full suite and both deck builds**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`, `pio run -e s3`, `pio run -e cyd`, `pio run -e cyd_companion`
Expected: `suites pass=46 fail=0`; three builds SUCCESS; both CYD builds below 97 %.

- [ ] **Step 5: Commit**

```bash
git add README.md CHANGELOG.md docs/ARCHITECTURE.md docs/BACKLOG.md SECURITY.md include/pins.h
git commit -m "docs: companion screen"
```
