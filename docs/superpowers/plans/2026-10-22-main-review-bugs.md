# Main review bugs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the code-review findings (2026-10-21) that are still real on `main` (e0a672d), and settle the doubtful ones with evidence instead of guesses.

**Architecture:** Each fix is small and local. Wherever the decision is pure it goes in a host-tested helper (`util/` or a pure header), following the repo's pattern; drawing and drivers only call it. One task is a device measurement that decides whether a fix is needed at all.

**Tech Stack:** C++ (Arduino-ESP32 2.0.17, PlatformIO, espressif32@6.9.0), Unity host tests through `tools/ntest.ps1` / `tools/run_tests.ps1`.

**Spec:** this document's "Verification summary" (the review is in the 2026-10-21 session; no separate spec).

## Verification summary (done 2026-10-22, against origin/main e0a672d)

The repo changed a lot after the review (#3 to #13: the CYD deck build retired, an own Spotify client, PSRAM buffers, OTA, the car LEDs). Each finding was re-checked against main's code, not against `feat/obd`.

| # | Finding | Verdict on main | Evidence |
|---|---|---|---|
| 1 | Plain boot-terminal lines get a dot leader | **Real, visible** | Probe test on main's `bios`: `value=""` -> `[Press DEL to enter SETUP ......]`. `bootterm.cpp:41` passes `r.value` ("" for plain lines, `bios.cpp:89`) to `bios::format`, which only treats `nullptr` as plain. |
| 2 | `/play` answers 404/503 before checking the key | **Real, low** (hotspot-only) | `remote.cpp:129-135`: clip and memory checks come before `s_gate.request`, against `remotegate.h` ("checked first"). |
| 3 | A failed album-art fetch is never retried | **Real, low** | `nettask.cpp:150` marks `Step::Art` done whatever `art::fetch` returned; `netplan` has no art retry. |
| 4 | Music box restarts every loop pass when the codec won't open | **Real, fault-only** (not seen) | `greeting.cpp:163`: the task ends if `audiosink::open` fails; `main.cpp:279` `musicBox()` backs off only on low memory, so it runs `playTunes()` again ~every 20 ms (I2S install + codec reset over the shared I2C bus). |
| 5 | Poké Ball smears the name on episodes of 100 min or more | **Real, cosmetic** | Font 2: digit 8 px, ':' 3, '/' 7, so "100:00/100:00" = 93 px -> ball x 194..206; the clear starts at 206 (`screen_now.cpp:96`), and the name / "No.xxxx" may reach x 204 (`screen_now.cpp:435`). Episodes do reach it (`client.cpp:23`, `additional_types=episode`). |
| 6 | Self-heal reboots every ~3 min on a 401 or outage | **Unproven on the S3** | The logic is unchanged (`nettask.cpp:195`: any failed allocation = "starved"). But the only log on disk with `allocfail` is a CYD log (2026-10-09, 16.7 KB TLS buffers: real starvation); every S3 log with `[mem]` lines shows 0. The "small WiFi failures on every request" came from the CYD, now retired. Measure first (Task 6). |
| 7 | WebServer reads any POST body before the handler | **Not worth fixing on the S3** | True (core `Parsing.cpp:201`), but on the S3 allocations over 4 KB go to the 8 MB PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 4096`), so a big body can't starve TLS; only someone on the owner's hotspot can send one. Note it for the CYD pages (no PSRAM): companion phone page, `ledpanel`. |
| 8 | Idle sprite wider than 120 px overflows `s_sceneBuf` | **Not a bug** | The only producer, `tools/gen_idle_sprite.py`, renders in a 64 x 64 grid (`SIZE = 64`), so 2w <= 128 < 240. Dropped. |
| - | "Nothing playing" on a 204 + failed /me/player | **Gone** | Main's own client asks `/me/player` directly (#4); `feat/obd`'s `d079b62` doesn't apply. |

## Global Constraints

- Base: `origin/main`. Branch `fix/main-review-bugs`, one PR; commits in the repo's style: `type(scope): subject`, ending with the Co-Authored-By line.
- PR #11 (`refactor/split-main`) moves `musicBox()` from `src/main.cpp` to `src/ui/statusview.cpp`. Task 4 lands after #11 merges (rebase onto it); Tasks 1, 2, 3, 5 touch no file #11 changes.
- `src/` builds with `-Werror`; envs to build: `s3`, `s3_ota`, `hwcheck_s3`, `cyd_companion`, `ledpanel`. Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` must end `fail=0`.
- `feat/obd` already has fixes for 1 and 2 (`6af77a8`, `d34c366`): cherry-pick them (`git cherry-pick -x`) rather than rewriting; resolve only what main changed around them.
- Don't flash anything without the owner; device checks are listed per task for the owner.

## Review Focus

1. A boot-terminal line whose live value is still empty (WI-FI before it joins) must stay plain, then gain its leader when the value arrives: test in Task 1.
2. The remote's `GET /state` and `POST /trip/reset` already check the key first; Task 2 must not change them: covered by reading the diff (one function).
3. A song whose art URL is empty (some podcasts) must not be retried: test in Task 3.
4. The art retry must reset on a track change, or the next song inherits a spent retry: test in Task 3.
5. The music box must come back by itself after a transient codec failure (once a minute), not stay off until reboot: Task 4 step.

---

### Task 1: Boot terminal: plain lines without a dot leader

**Files:**
- Modify: `src/util/bios.h`, `src/util/bios.cpp` (add `termFormat`)
- Modify: `src/ui/bootterm.cpp:41`
- Test: `test/test_bios/test_bios.cpp`

**Interfaces:**
- Produces: `int bios::termFormat(const TermLine& r, char* out)`; out holds `TERM_COLS + 1`; returns `format()`'s value column, `-1` when `r.value` is `""`.

- [ ] **Step 1: Cherry-pick** `git cherry-pick -x 6af77a8` (fix(ui): no dot leader on plain boot-terminal lines). It adds the helper, the test `test_term_format_plain_line_has_no_leader` and the `bootterm.cpp` call.
- [ ] **Step 2: Add the Review Focus 1 test** `test_term_format_live_line_gains_its_leader`: `termSet(term, "WI-FI", "", 0, 0, true)` -> `termFormat` returns -1 and out == "WI-FI"; then `termSet(term, "WI-FI", "HOTSPOT [OK]", 0, 100)` -> returns `TERM_COLS - 12` and `out + that == "HOTSPOT [OK]"`.
- [ ] **Step 3: Run** `powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_bios\test_bios.cpp src\util\bios.cpp` -> `0 Failures`.
- [ ] **Step 4: Commit** `test(ui): a live terminal line gains its leader when its value arrives`.

### Task 2: Phone remote: the key before anything else

**Files:**
- Modify: `src/net/remote.cpp:128-135` (`handlePlay`)

- [ ] **Step 1: Cherry-pick** `git cherry-pick -x d34c366` (fix(remote): check the key before answering a play): `handlePlay` reads `k`, answers 403 on `!s_gate.keyOk(key.c_str())`, then the 404 clip check, then the 503 memory check.
- [ ] **Step 2: Build** `pio run -e s3` -> SUCCESS. (No host test: the order lives in the handler; `remotegate` itself is tested.)
- [ ] **Step 3: Device check for the owner** (after flashing): `curl -i -X POST "http://<deck>/play?clip=nope"` -> `403`; with `&k=<token>` -> `404`.

### Task 3: Album art: one retry after the next poll

**Files:**
- Modify: `src/util/netplan.h` / `.cpp` (add `ArtRetry`)
- Modify: `src/core/nettask.cpp` (the `Step::Art` case and `onTrackChange`)
- Test: `test/test_netplan/test_netplan.cpp`

**Interfaces:**
- Produces: `class netplan::ArtRetry { public: static constexpr int MAX_TRIES = 2; void reset(); bool onResult(bool ok, bool hasUrl); };` `onResult` returns true when the step is finished (ok, no URL, or `MAX_TRIES` attempts), like `WalkRetry::onResult`.

- [ ] **Step 1: Write the failing tests** in `test_netplan.cpp`:
  - `test_art_retry_once_then_gives_up`: `r.onResult(false, true)` is false; `r.onResult(false, true)` is true.
  - `test_art_ok_or_no_url_is_final`: a fresh `ArtRetry`: `onResult(true, true)` is true; another fresh one: `onResult(false, false)` is true (Review Focus 3).
  - `test_art_retry_reset_on_track_change`: `onResult(false, true)`, `reset()`, then `onResult(false, true)` is false again (Review Focus 4).
- [ ] **Step 2: Run** `tools\ntest.ps1 test\test_netplan\test_netplan.cpp src\util\netplan.cpp src\util\text.cpp` -> fails to compile (`ArtRetry` undefined).
- [ ] **Step 3: Implement `ArtRetry`** in `netplan.h` (a counter, like `WalkRetry`).
- [ ] **Step 4: Wire it** in `nettask.cpp`: `static netplan::ArtRetry s_artRetry;` reset in `onTrackChange()` next to `s_lyricsRetry.reset()`; in `case Step::Art`, after `tock`: `if (!s_artRetry.onResult(ok, s_st.albumArtUrl[0] != '\0')) return false;` (the same "after the next poll" path the walker uses).
- [ ] **Step 5: Run** the netplan tests -> `0 Failures`; `pio run -e s3` -> SUCCESS.
- [ ] **Step 6: Commit** `fix(core): retry a failed album art once after the next poll`.

### Task 4: Music box: back off when the codec won't open (after PR #11)

**Files:**
- Modify: `src/audio/greeting.h` / `.cpp` (`tuneTask`, `playTunes`)
- Modify: `src/ui/statusview.cpp` (`musicBox`, as moved by #11)

**Interfaces:**
- Produces: `bool greeting::tunesFailed();` true from a `tuneTask` whose `audiosink::open()` failed until the next `playTunes()` (a `std::atomic<bool>` beside `s_tunes`).

- [ ] **Step 1: Rebase** the branch onto main once #11 is merged; confirm `musicBox` is in `src/ui/statusview.cpp`.
- [ ] **Step 2: Implement** `tunesFailed()`: set in `tuneTask` when `!s_tunesStop` and `open()` failed, cleared in `playTunes()`.
- [ ] **Step 3: Use it** in `musicBox()`: before `playTunes()`, `if (greeting::tunesFailed()) { gaveUp = true; gaveUpAt = millis(); Serial.println("[tunes] audio didn't start: retry in 1 min"); return; }` checked only when not playing (so it retries after `RETRY_MS`, 60 s; Review Focus 5).
- [ ] **Step 4: Build** `pio run -e s3 -e s3_ota` -> SUCCESS. (No host test: driver failure; the back-off reuses the tested-in-use `gaveUp` path.)
- [ ] **Step 5: Commit** `fix(s3): the music box waits a minute when the audio won't start`.

### Task 5: Poké Ball: hide it when the time leaves no room

**Files:**
- Modify: `src/ui/pokeball.h` (add `left`), `src/ui/pokeball.cpp`
- Modify: `src/ui/screen_now.cpp:98` (`drawProgressRegion`)
- Test: `test/test_pokeball/test_pokeball.cpp`

**Interfaces:**
- Produces: `constexpr int pokeball::AREA_X = 206;` (the region `drawProgressRegion` clears) and `int pokeball::left(int textRight, int textW);` -> `textRight - textW - 4 - SIZE`, or `-1` when that is below `AREA_X` (no ball drawn: `drawBall` already skips `s_ballX < 0`).

- [ ] **Step 1: Write the failing test** `test_ball_hides_when_the_time_is_wide`: `left(304, 77) == 210` ("12:34/12:34", today's case); `left(304, 80) == 207`; `left(304, 83) == -1` (would be 204); `left(304, 85) == -1` ("10:00/100:00", would be 202); `left(304, 93) == -1` ("100:00/100:00", would be 194).
- [ ] **Step 2: Run** `tools\ntest.ps1 test\test_pokeball\test_pokeball.cpp src\ui\pokeball.cpp` -> fails to compile.
- [ ] **Step 3: Implement** `left()`; in `screen_now.cpp` use `pokeball::AREA_X` in the `fillRect` and `s_ballX = pokeball::left(304, t.textWidth(tbuf, 2));`.
- [ ] **Step 4: Run** the test -> `0 Failures`; `pio run -e s3` -> SUCCESS.
- [ ] **Step 5: Commit** `fix(ui): no Poke Ball over the name when the time is 100 min or more`.

### Task 6: Self-heal: measure on the S3 before changing it

**Files:**
- Modify (only if the measurement says so): `src/core/mem.h` / `.cpp`, `src/util/netplan.h`, `src/core/nettask.cpp`, `test/test_netplan/test_netplan.cpp`
- Modify: `docs/BACKLOG.md` ("Dropped WiFi receive buffers")

- [ ] **Step 1: Capture (owner, on the iPhone hotspot, normal use with songs changing):** `python tools/capture_serial.py <S3 port> 1800 .superpowers/selfheal-s3.log --no-reset` (mind the HWCDC stall note: keep the port open the whole time).
- [ ] **Step 2: Read it:** `grep -c "allocfail" .superpowers/selfheal-s3.log` and the sizes in `(last N B caps=...)`.
- [ ] **Step 3: Decide:**
  - 0 `allocfail`, or only failures of 4096 B and up: **no code change.** Update the BACKLOG entry: re-measured on the S3 (date, 0 small failures), close the item.
  - Small failures (< 4096 B) between good polls: cherry-pick `feat/obd`'s `1d44c44` (fix(core): restart only on big failed allocations: `mem::bigFailTotal()`, `netplan::starved()`, test `test_starved_needs_memory_evidence`), resolve against main's `nettask.cpp:192` (it now also calls `ota::onGoodPoll()`), run the netplan tests and `pio run -e s3`, commit.

### Task 7: Wrap up

- [ ] **Step 1:** In `docs/BACKLOG.md`, under "Accepted as is", add one line: the WebServer reads a whole POST body before the handler; harmless on the S3 (PSRAM), watch on the CYD pages (companion, `ledpanel`).
- [ ] **Step 2:** `tools\run_tests.ps1` -> `fail=0`; `pio run -e s3 -e s3_ota -e hwcheck_s3 -e cyd_companion -e ledpanel` -> all SUCCESS.
- [ ] **Step 3:** Commit `docs(backlog): the review findings settled`, push, open the PR against main listing each finding's verdict (the table above).
