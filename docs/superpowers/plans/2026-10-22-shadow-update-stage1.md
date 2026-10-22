# Shadow Update Screen, Stage 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The S3's firmware-update screen becomes the Star Light Zone night with a red progress rail, a 100% flash and an "Update failed" state; Shadow's frames come in stage 2.

**Architecture:** A stdlib-only Python tool rebuilds the Star Light Zone backdrop from s1disasm into a git-ignored media file. Pure, host-tested C++ helpers own the decisions (the backdrop file, the update's state over time, the rail geometry); `net/ota` keeps the state and reboots itself, `ui/otascreen` draws, `main.cpp` picks the screen. Without the media file the deck keeps today's battle-box screen.

**Tech Stack:** Python 3 (stdlib, unittest), C++ (Arduino-ESP32 2.0.17, TFT_eSPI, ArduinoOTA), Unity host tests (`tools/ntest.ps1`, `tools/run_tests.ps1`).

**Spec:** `docs/superpowers/specs/2026-10-22-shadow-update-screen-design.md`

## Global Constraints

- SEGA's art is never committed: the tool fetches into the git-ignored `sonic-sprites/`, writes the git-ignored `data/slzbg.bin`; CREDITS.md says so.
- Source pinned: `sonicretro/s1disasm` @ `15dfa3a6ee4c53aeb482233e493755c84b4f3479` (same as `tools/gen_idle_sprite.py`); files `artnem/8x8 - SLZ.nem`, `map16/SLZ.eni`, `map256/SLZ.kos`, `levels/slzbg.bin`, `palette/Star Light Zone.bin`, `palette/Sonic.bin`.
- The backdrop window: x 256..575, y 660..899 of the rendered layout (320 x 240, 1x).
- Layout (spec "Layout"): strip y 0..19, ~80% black, `FIRMWARE UPDATE` at x 8 and the percent right-aligned at x 312, font 2, percent #FF5A2A, other text white with a 1 px black shadow; rail x 16..303, y 200..209, 2 px border #E8E8F0, track #101018, fill #C01818 to #FF5A2A; `Keep the deck powered` at x 16, y 216, font 2; failed: border and fill #808080, strip text `Update failed`.
- Timings (spec "Decisions"): reboot 600 ms after Done; `Update failed` held 3000 ms; failed is not "active" (the network task resumes at once); a new upload during the hold takes over.
- Tools are stdlib-only (CI runs `python -m unittest discover -s tools -p "test_*.py"` with no extra packages); the PNG preview, if any, is optional and skipped without Pillow.
- `src/` builds with `-Werror`. Builds to pass: `pio run -e s3 -e s3_ota -e hwcheck_s3`. Host suites: `tools\run_tests.ps1` ends `fail=0`.
- Commits: `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- If PR #11 (`refactor/split-main`) merges first, rebase: Task 5's `main.cpp` hunk moves with the update call.

## Review Focus

1. CI and fresh checkouts have only the 8-byte stand-in `slzbg.bin` (header + zeros): the deck must keep the old screen, not draw garbage. Test in Task 2 (`test_stand_in_reads_as_none`).
2. The percent arrives out of range or jumps (0 then 100, 101, -1): the fill and the jets must stay inside the rail. Test in Task 3 (`test_fill_and_rider_clamp`).
3. ArduinoOTA's `handle()` returning after a successful end must not count as a failure. Test in Task 3 (`test_end_then_return_is_done`).
4. A second upload starting during the failed hold: the screen shows it at once. Test in Task 3 (`test_new_start_during_failed_hold`).
5. The hold across the `millis()` wrap. Test in Task 3 (`test_hold_survives_millis_wrap`).

---

### Task 1: The backdrop generator

**Files:**
- Create: `tools/gen_slz_backdrop.py` (from `tools/slz_spike.py`, which is then deleted)
- Create: `tools/test_gen_slz_backdrop.py`
- Modify: `tools/pack_media.py` (FILES), `tools/media_sizes.py` (HEADS), `data/media_sizes.txt`, `.gitignore`, `CREDITS.md`

**Interfaces:**
- Produces (Python, module `gen_slz_backdrop`): `kosinski(data: bytes) -> bytes`, `enigma(data: bytes) -> list[int]`, `nemesis(data: bytes) -> list` (tiles, each 8 rows of 8 colour indices), `render(src_dir) -> (width, height, pixels)` with pixels a list of RGB565 ints row by row, `pack(w, h, pixels) -> bytes`.
- Produces (file): `data/slzbg.bin` = `b"BKD1"` + u16 width LE + u16 height LE + width*height pixels, each RGB565 as 2 bytes **big-endian** (so a little-endian `uint16_t` read is already `rgb565::be()` for `pushImage`).

- [ ] **Step 1: Write the failing tests** in `tools/test_gen_slz_backdrop.py` (unittest, no network):
  - `test_kosinski_literals_and_end`: `kosinski(bytes([0x0B,0x00,0x41,0x42,0x00,0xF0,0x00])) == b"AB"`
  - `test_kosinski_inline_copy`: `kosinski(bytes([0x49,0x00,0x41,0xFF,0x00,0xF0,0x00])) == b"AAAAA"`
  - `test_enigma_incrementing_and_common`: `enigma(bytes([8,0,0x00,0x10,0x00,0x00,0x09,0x1F,0xE0])) == [0x10,0x11,0x12,0,0]`
  - `test_nemesis_one_code`: `nemesis(bytes([0x00,0x01,0x85,0x71,0x00,0xFF,0x00])) == [[[5]*8]*8]`
  - `test_nemesis_xor_mode`: same with header `0x80,0x01`: rows alternate `[5]*8` and `[0]*8`
  - `test_pack_header_and_byte_order`: `pack(2, 1, [0x1234, 0xABCD]) == b"BKD1" + bytes([2,0,1,0,0x12,0x34,0xAB,0xCD])`
  - `test_md_colour`: `md_to_rgb565(0x0EEE) == 0xFFFF` (shared with `gen_idle_sprite`: import it, don't copy it)
- [ ] **Step 2: Run** `python tools/test_gen_slz_backdrop.py` → fails (`ModuleNotFoundError` / missing names).
- [ ] **Step 3: Implement** `tools/gen_slz_backdrop.py` from the spike: stdlib only (drop PIL; optional `--png out.png` preview only when Pillow imports); fetch the six pinned files into `sonic-sprites/slz/` like `gen_idle_sprite.fetch`; render the layout, crop the window, write `data/slzbg.bin`; docstring in the style of `gen_idle_sprite.py` (what, source, © SEGA, format). Delete `tools/slz_spike.py`.
- [ ] **Step 4: Run** `python tools/test_gen_slz_backdrop.py` → OK; `python -m unittest discover -s tools -p "test_*.py"` → OK.
- [ ] **Step 5: Wire the media:** `pack_media.FILES += ["slzbg.bin"]`; `media_sizes.HEADS["slzbg.bin"] = b"BKD1"` and `slzbg.bin 8` in `data/media_sizes.txt` (an 8-byte stand-in: pack leaves it out, parse rejects it); `.gitignore`: `data/slzbg.bin`; CREDITS.md: the backdrop line beside Sonic's.
- [ ] **Step 6: Generate for real** `python tools/gen_slz_backdrop.py` → `data/slzbg.bin` 153 608 bytes; `python tools/media_sizes.py record` only if that is how sizes are kept (check the file's header comment), else leave `8`.
- [ ] **Step 7: Commit** `feat(tools): Star Light Zone backdrop for the update screen`.

### Task 2: Reading the backdrop

**Files:**
- Create: `src/util/backdrop.h`, `src/util/backdrop.cpp`
- Test: `test/test_backdrop/test_backdrop.cpp`

**Interfaces:**
- Produces: `namespace backdrop { bool parse(const uint8_t* blob, size_t len); int width(); int height(); const uint8_t* row(int y); }` — `row()` points at width*2 bytes (big-endian RGB565), nullptr when out of range or no file; a failed parse leaves width/height 0.

- [ ] **Step 1: Write the failing tests:**
  - `test_valid_file`: a 4 x 2 blob → `parse` true, `width()==4`, `height()==2`, `row(1)==blob+8+8`, `row(2)==nullptr`
  - `test_stand_in_reads_as_none` (Review Focus 1): `b"BKD1"+4 zeros` → false, `width()==0`, `row(0)==nullptr`
  - `test_bad_magic_or_length`: wrong magic → false; length one byte short → false; `nullptr` → false
  - `test_too_big`: 321 x 1 → false (nothing bigger than the 320 x 240 screen)
- [ ] **Step 2: Run** `tools\ntest.ps1 test\test_backdrop\test_backdrop.cpp src\util\backdrop.cpp` → compile failure.
- [ ] **Step 3: Implement** `backdrop` (header like `util/idlesprite.h`: what, where the file comes from, PURE, host-tested).
- [ ] **Step 4: Run** the same command → `0 Failures`.
- [ ] **Step 5: Commit** `feat(util): the update screen's backdrop file`.

### Task 3: The update's state and the rail's geometry

**Files:**
- Create: `src/util/otaview.h` (+ `.cpp` if needed)
- Test: `test/test_otaview/test_otaview.cpp`

**Interfaces:**
- Produces:
  - `namespace otaview { enum class Phase : uint8_t { Idle, Receiving, Done, Failed }; constexpr uint32_t DONE_MS = 600, FAILED_HOLD_MS = 3000; }`
  - `class otaview::Tracker { public: void onStart(); void onProgress(int pct); void onEnd(uint32_t nowMs); void onError(uint32_t nowMs); void onHandleReturned(uint32_t nowMs); Phase phase(uint32_t nowMs) const; int pct() const; bool rebootDue(uint32_t nowMs) const; };` — `onHandleReturned` turns Receiving into Failed (ArduinoOTA's cut-off paths skip `onError`) and leaves Done alone; Failed reads as Idle once `FAILED_HOLD_MS` has passed; `rebootDue` is true from `DONE_MS` after `onEnd`.
  - Geometry: `constexpr int RAIL_X = 16, RAIL_Y = 200, RAIL_W = 288, RAIL_H = 10, BORDER = 2;` `int fillWidth(int pct);` (inner width 284 px * pct / 100, clamped 0..284) `int riderX(int pct, int riderW);` (rider centred on the fill's tip, clamped to `RAIL_X .. RAIL_X + RAIL_W - riderW`).
- [ ] **Step 1: Write the failing tests:**
  - `test_receiving_then_done_then_reboot`: start, progress 40 → `phase==Receiving`, `pct()==40`; `onEnd(1000)` → `Done`; `rebootDue(1599)` false, `rebootDue(1600)` true
  - `test_end_then_return_is_done` (Review Focus 3): `onEnd(1000)`, `onHandleReturned(1001)` → still `Done`
  - `test_cut_off_is_failed_then_idle`: start, `onHandleReturned(5000)` → `Failed` at 5000 and 7999, `Idle` at 8000
  - `test_error_is_failed`: start, `onError(10)` → `Failed`
  - `test_new_start_during_failed_hold` (Review Focus 4): failed at 5000, `onStart()`, progress 3 → `Receiving` at 5100
  - `test_hold_survives_millis_wrap` (Review Focus 5): failed at `0xFFFFF000` → `Failed` at `0xFFFFF000 + 2999`, `Idle` at `+ 3000`
  - `test_fill_and_rider_clamp` (Review Focus 2): `fillWidth(0)==0`, `fillWidth(50)==142`, `fillWidth(100)==284`, `fillWidth(101)==284`, `fillWidth(-1)==0`; `riderX(0, 20)==16`, `riderX(100, 20)==284`, `riderX(50, 20)==18+142-10`
- [ ] **Step 2: Run** `tools\ntest.ps1 test\test_otaview\test_otaview.cpp` (add `src\util\otaview.cpp` if created) → compile failure.
- [ ] **Step 3: Implement** `otaview` (PURE, host-tested; unsigned time arithmetic as in `util/idlewait`).
- [ ] **Step 4: Run** → `0 Failures`.
- [ ] **Step 5: Commit** `feat(util): the update screen's state and rail`.

### Task 4: `net/ota` keeps the state and reboots itself

**Files:**
- Modify: `src/net/ota.h`, `src/net/ota.cpp`

**Interfaces:**
- Consumes: `otaview::Tracker`, `otaview::Phase` (Task 3).
- Produces: `struct ota::View { otaview::Phase phase; int pct; };` and `ota::View ota::view();` (thread-safe copy); `ota::active()` stays and means `Receiving || Done` (the network task stays paused until the reboot); `ota::progress()` stays (pct while active, else -1).

- [ ] **Step 1: Implement:** a `Tracker` under a `portMUX` in `ota.cpp`, fed by `onStart` / `onProgress` / `onEnd` / `onError` and by `onHandleReturned(millis())` after each `ArduinoOTA.handle()`; `ArduinoOTA.setRebootOnSuccess(false)`; in the task loop, when `rebootDue(millis())`: the existing log line, `delay(100)`, `esp_restart()`. Keep the existing `[ota]` log lines.
- [ ] **Step 2: Build** `pio run -e s3` → SUCCESS. (No host test: ArduinoOTA glue; the decisions are Task 3's, tested.)
- [ ] **Step 3: Commit** `feat(ota): done and failed states, the deck reboots itself after the screen's flash`.

### Task 5: The screen

**Files:**
- Modify: `src/ui/otascreen.h`, `src/ui/otascreen.cpp`, `src/main.cpp` (the `ota::active()` block, ~line 370; the media load in `setup()` beside `idle.bin`)

**Interfaces:**
- Consumes: `backdrop::*` (Task 2), `otaview::*` (Task 3), `ota::view()` (Task 4).
- Produces: `bool ui::drawShadowUpdate(TFT_eSPI& t, const ota::View& v, uint32_t nowMs, bool full);` — false (nothing drawn) when `backdrop::width()==0`.

- [ ] **Step 1: Implement `drawShadowUpdate`:** `full`: push the backdrop row by row through a small RAM row buffer (the file need not be 2-byte aligned, as `avatarview::show`), then the strip and `Keep the deck powered`; every call: repaint the rail band (y 186..211) from the backdrop, the rail per the spec's colours (fill as a left-to-right gradient, grey when Failed), the percent (`FIRMWARE UPDATE` / `Update failed`), and two cyan jet glows (#77FFFF, ~6 x 8 px, bobbing 1 px every 120 ms) at `riderX(pct, 20)` just above the rail (stage 2 puts Shadow there). `Done`: fill the screen #C01818 once, `FIRMWARE UPDATE` centred white, then nothing until the reboot. Redraw only on a change of pct, phase or the 120 ms bob step.
- [ ] **Step 2: Wire `main.cpp`:** at boot `MEDIA_DECL(data_slzbg_bin)` and `backdrop::parse(...)` with a `[ota] update screen: Star Light Zone` / `: battle box` log; replace the `ota::active()` block: take `ota::view()`; when its phase isn't Idle, draw `drawShadowUpdate(..., lastMode != -2)` and fall back to `drawUpdate(s_tft, v.pct, ...)` when it returns false **and** the phase is Receiving or Done (Failed without the art behaves as today: back to the deck); set `lastMode = -2`, `delay(20)`, return.
- [ ] **Step 3: Build** `pio run -e s3 -e s3_ota -e hwcheck_s3` → all SUCCESS (with the real `data/slzbg.bin` and with the 8-byte stand-in).
- [ ] **Step 4: Commit** `feat(ui): the Star Light Zone update screen`.

### Task 6: Docs and the device check

**Files:**
- Modify: `docs/ARCHITECTURE.md` (the OTA entry), `docs/BACKLOG.md` (Verification), `README.md` (media list if it names them)

- [ ] **Step 1:** ARCHITECTURE: the screen, the files, the states. BACKLOG Verification: first USB `pio run -e s3 -t upload` and `-t uploadmedia` (the new media file), then an OTA `pio run -e s3_ota -t upload` shows the screen: rail fills, jets ride, red flash, reboot; cut the PC's upload mid-way → `Update failed` 3 s, then the deck. Stage 2 (Shadow's Sonic Battle frames) as an Ideas item waiting on the sheet.
- [ ] **Step 2:** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → `fail=0`; `python -m unittest discover -s tools -p "test_*.py"` → OK.
- [ ] **Step 3: Commit** `docs: the Star Light Zone update screen`.
