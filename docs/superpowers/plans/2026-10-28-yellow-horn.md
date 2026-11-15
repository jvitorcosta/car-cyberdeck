# Yellow Deck Horn (no DFPlayer) Implementation Plan

> **Parked idea (2026-11-15), not scheduled.** Kept as a plan only: there are no resources for it
> now, and #52 scoped the yellow deck to lights, karaoke, music, pad and watch (its heap is ~37 KB).
> Nothing here is built. The horn's mic cable pinout is still a guess until it is measured. If it
> returns, it may fit a future deck better than the yellow one.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The yellow deck (ESP32 CYD, env `ledpanel`) plays short clips through the MFL 20 W megaphone horn: it keys the horn's PTT, plays a clip from flash, and releases.

**Architecture:** Clips live in one embedded blob (`data/horn.bin`, built by a tool from your own WAV/MP3s, with a committed default of synthetic beeps). The CYD plays them on GPIO26 through I2S0's built-in DAC; its SPEAK output feeds the horn's mic line through a 1:1 audio isolator and an attenuator. GPIO22 drives a PC817 that closes the mic's PTT. A pure, host-tested sequencer decides when PTT goes on and off; a thin hardware module does the I/O. Triggers: a new HORN item on the CYBERDECK menu and `GET /horn`.

**Tech Stack:** Arduino-ESP32 (PlatformIO `ledpanel`), legacy `driver/i2s.h` in built-in DAC mode, Unity host tests via `tools/run_tests.ps1`, Python + ffmpeg (`D:\dev\tools\ffmpeg\bin`) for the clip tool.

**Spec:** Decided in chat on 2026-10-28; wiring in `docs/hardware/siren-wiring.drawio` (Task 5 redraws it for this version). Base branch: `main` (after #27). Work on a new branch `feat/yellow-horn`.

## Global Constraints

- Pins: PTT = **GPIO22** (CN1), audio = **GPIO26** (CYD speaker amp, SPEAK connector). GPIO27 and GPIO35 stay free (no DFPlayer).
- SPEAK is a bridged (BTL) output: **neither SPEAK pin is ground**. It only ever connects to the isolator's primary.
- Clip format in flash: **8000 Hz, unsigned 8-bit mono, 128 = silence**.
- Flash budget: the `ledpanel` app is ~1.39 MB of a 1,966,080-byte slot. `data/horn.bin` must stay **≤ 400,000 bytes** (~50 s); the tool refuses bigger.
- PTT timing: key **150 ms** before audio, hold **100 ms** after it ends, never hold longer than **15 s** in one go.
- PTT is OFF at boot: GPIO22 is set OUTPUT LOW before anything else in `setup()`.
- At most **6 clips** (one screen of buttons, the Presets grid).
- Your own clips stay out of git (`horn-clips/`, `data/horn.bin`); only `data/horn_default.bin` is committed.
- Code style: `PURE, host-tested` modules under `src/util/yellow/`, hardware under `src/yellow/`, conventional commits `type(scope): subject`.

## Review Focus

1. **A second trigger while a clip plays** (menu tap during a `/horn` call): expect it to be refused, not to cut or restart the clip. Test in Task 2 (`test_busy_rejects_second_start`).
2. **Playback that never reports done** (I2S stall): PTT must still drop by the 15 s cap. Test in Task 2 (`test_cap_releases_ptt`).
3. **A clip index out of range or an empty bank** (`/horn?clip=9`, no clips built): a clear refusal, no crash. Tests in Task 1 (`test_clip_out_of_range`) and Task 4 (`test_horn_arg_parse`).
4. **A truncated or foreign `horn.bin`**: parse fails, the HORN item shows "no clips" instead of reading past the blob. Test in Task 1 (`test_truncated_blob`).
5. **Stop mid-clip**: PTT drops within the tail, audio is silenced. Test in Task 2 (`test_stop_releases_after_tail`).

---

### Task 1: Clip bank format and its tool

**Files:**
- Create: `src/util/yellow/hornbin.h`, `src/util/yellow/hornbin.cpp`
- Create: `tools/gen_horn.py`, `tools/horn_clip.py`
- Create: `data/horn_default.bin` (generated)
- Modify: `platformio.ini` (`[env:ledpanel]`: `embed_files`, `extra_scripts`), `.gitignore`
- Test: `test/test_hornbin/test_hornbin.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace hornbin {
  constexpr int MAX_CLIPS = 6;
  struct Clip { const uint8_t* pcm; uint32_t len; char name[13]; };   // name NUL-terminated
  struct Bank { int count; uint16_t rate; Clip clips[MAX_CLIPS]; };
  bool parse(const uint8_t* blob, size_t len, Bank* out);   // false: not a whole, valid file
  uint32_t durationMs(const Bank& b, int i);                  // 0 when i is out of range
  }
  ```
- Format "HRN1" (little-endian): magic `HRN1`, u16 count (1..6), u16 rate (8000); then count × {u32 offset, u32 len, char name[12]} (offset from blob start); then the PCM. PURE, host-tested (header comment in the style of `carbin.h`).

- [ ] **Step 1: Write the failing tests** — `test_parse_two_clips` (a hand-built 2-clip blob: count 2, rate 8000, names "beep", "hi", lengths 8000 and 4000 → `durationMs` 1000 and 500), `test_truncated_blob` (same blob minus its last byte → `parse` false), `test_bad_magic` (→ false), `test_clip_out_of_range` (`durationMs(b, 2)` and `(b, -1)` → 0), `test_too_many_clips` (count 7 → false).
- [ ] **Step 2: Run** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` — expected: `FAIL test_hornbin` (missing header).
- [ ] **Step 3: Implement `hornbin::parse` / `durationMs`** — every offset+len checked against `len` before use; names copied and NUL-terminated.
- [ ] **Step 4: Run the tests** — expected: `PASS test_hornbin`.
- [ ] **Step 5: Write `tools/gen_horn.py`** — `python tools/gen_horn.py` (no args) writes `data/horn_default.bin` with three synthetic clips: "beep" (1 kHz, 0.4 s), "beepbeep" (two 0.25 s beeps), "sweep" (600→1400 Hz, 1 s). `python tools/gen_horn.py horn-clips/` converts up to 6 WAV/MP3 (sorted by filename; name = filename stem cut to 12 chars) through ffmpeg to 8 kHz u8 mono, writes `data/horn.bin`, prints each clip's seconds and the total, and exits 1 above 400,000 bytes.
- [ ] **Step 6: Write `tools/horn_clip.py`** — a pre-build script like `tools/greeting_clip.py`: when `data/horn.bin` is missing, copy `data/horn_default.bin` to it. Add it to `[env:ledpanel]` `extra_scripts`, and `data/horn.bin` to its `board_build.embed_files` (after `data/jp16.bin`). `.gitignore`: `data/horn.bin`, `horn-clips/`.
- [ ] **Step 7: Verify** — `python tools/gen_horn.py` then `pio run -e ledpanel`: builds; the size line stays under 1,966,080.
- [ ] **Step 8: Commit** — `feat(horn): clip bank format, tool and default beeps`

### Task 2: PTT sequencer

**Files:**
- Create: `src/util/yellow/hornseq.h`, `src/util/yellow/hornseq.cpp`
- Test: `test/test_hornseq/test_hornseq.cpp`

**Interfaces:**
- Consumes: nothing (times and the clip length come in as numbers).
- Produces:
  ```cpp
  namespace hornseq {
  constexpr uint32_t KEY_MS = 150, TAIL_MS = 100, CAP_MS = 15000;
  struct Out { bool ptt; int play; bool silence; };  // play: clip to start now, else -1; silence: stop audio now
  class Seq {
   public:
    bool start(int clip, uint32_t now);   // false when not idle
    void stop(uint32_t now);              // silence now, release after TAIL_MS
    Out tick(uint32_t now, bool audioDone);
    bool busy() const;
  };
  }
  ```
  States: Idle → Keying (ptt on) → Playing → Tail → Idle. `play` is reported exactly once, at `KEY_MS`. Wraparound-safe time compares (`int32_t(now - t)`), as elsewhere in ledpanel.

- [ ] **Step 1: Write the failing tests** — `test_keys_before_play` (start at 1000: tick(1000) ptt on, play -1; tick(1150) play == clip), `test_release_after_done_and_tail` (done at 2000 → ptt still on at 2099, off at 2100, `busy()` false), `test_busy_rejects_second_start`, `test_cap_releases_ptt` (never done: ptt off and silence at start + 15000), `test_stop_releases_after_tail` (stop at 1500 → silence true that tick, ptt off at 1600), `test_wraparound` (start at 0xFFFFFF00).
- [ ] **Step 2: Run tests** — expected `FAIL test_hornseq`.
- [ ] **Step 3: Implement `hornseq::Seq`.**
- [ ] **Step 4: Run tests** — expected `PASS test_hornseq`; all other suites still pass.
- [ ] **Step 5: Commit** — `feat(horn): PTT sequencer`

### Task 3: Hardware module (I2S DAC + PTT pin)

**Files:**
- Create: `src/yellow/horn.h`, `src/yellow/horn.cpp`
- Modify: `src/yellow/ledpanel.cpp` (`setup()`: PTT low first; `hornBegin` after the jp font; `hornLoop()` in `loop()`)

**Interfaces:**
- Consumes: `hornbin::parse/Bank/durationMs` (Task 1), `hornseq::Seq/Out` (Task 2), `EMBED(data_horn_bin)` from `common/core/embed.h`.
- Produces:
  ```cpp
  void hornPinsSafe();            // GPIO22 OUTPUT LOW; first line of setup()
  bool hornBegin();               // parse the embedded bank, install I2S0 in DAC mode; false: no clips
  int hornCount();                // 0 when the bank is invalid
  const char* hornName(int i);    // "" when out of range
  bool hornPlay(int i);           // false: busy or bad index
  void hornStop();
  bool hornBusy();
  void hornLoop();                // feeds I2S, runs Seq::tick, drives GPIO22
  ```
- I2S0: `I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN`, 8000 Hz, 16-bit, `i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)` (GPIO26). Each u8 sample goes out as `(uint16_t)s << 8`. `hornLoop` writes with timeout 0 so it never blocks the panel; audio is "done" once the clip's bytes are written and the DMA buffers drained (`i2s_zero_dma_buffer` on silence).

- [ ] **Step 1: Implement `horn.cpp`** per the interface; log `[horn] N clips` at begin and `[horn] play i "name" Xms` / `[horn] ptt on|off`.
- [ ] **Step 2: Wire into `ledpanel.cpp`** — `hornPinsSafe()` as the first line of `setup()`; `hornBegin()` after the jp font load; `hornLoop()` each `loop()`.
- [ ] **Step 3: Build** — `pio run -e ledpanel`: no warnings (`-Werror`), size under the slot.
- [ ] **Step 4: Boot check** — flash over USB: the boot log shows `[horn] 3 clips`, and GPIO22 measures 0 V. (Playing needs a trigger, which comes in Task 4.)
- [ ] **Step 5: Commit** — `feat(horn): I2S DAC playback and PTT pin on the yellow deck`

### Task 4: Triggers — `/horn` and the HORN menu screen

**Files:**
- Modify: `src/util/yellow/deckmenu.h`, `src/util/yellow/deckmenu.cpp` (item `HORN`, `Screen::Horn` with BACK + 6 clip slots in the Presets grid)
- Create: `src/util/yellow/hornarg.h` (parse the `clip` query arg)
- Modify: `src/yellow/ledpanel.cpp` (menu label, Horn screen draw/press, `server->on("/horn", ...)`)
- Test: `test/test_deckmenu/test_deckmenu.cpp` (extend), `test/test_hornarg/test_hornarg.cpp`

**Interfaces:**
- Consumes: `hornCount/hornName/hornPlay/hornStop/hornBusy` (Task 3).
- Produces:
  ```cpp
  namespace deckmenu {
  enum Item : uint8_t { LIGHTS, KARAOKE, MUSIC, HORN };  // N_ITEMS = 4
  enum class Screen : uint8_t { Menu, Lights, Karaoke, Presets, Horn };
  enum HornId : int { H_BACK = 0, H_CLIP0 = 1, H_N = H_CLIP0 + 6 };
  }
  namespace hornarg { int parse(const char* s, int count); }  // clip index 0..count-1, or -1
  ```
- `GET /horn?clip=<1..N>&k=<REMOTE_TOKEN>` (1-based for people), `GET /horn?stop=1&k=...`, auth through `reqauth::pass` like `/say`. Replies: `playing <name>`, `busy`, `no such clip`, `stopped`.
- Horn screen: one button per clip, label = clip name; an empty bank shows a single line "no clips" and only BACK.

- [ ] **Step 1: Write the failing tests** — `test_hornarg_parse`: `("1",3)→0`, `("3",3)→2`, `("4",3)→-1`, `("0",3)→-1`, `("",3)→-1`, `("2x",3)→-1`, `("1",0)→-1`. Extend `test_deckmenu`: Menu has 4 items inside the screen and apart (existing `check_screen`), Horn screen buttons inside and apart, `neighbour` moves between clip slots.
- [ ] **Step 2: Run tests** — expected FAIL for both suites.
- [ ] **Step 3: Implement `hornarg::parse` and the deckmenu changes** (Horn slots reuse the Presets grid rects).
- [ ] **Step 4: Run tests** — expected PASS, all suites.
- [ ] **Step 5: Wire `ledpanel.cpp`** — menu label "HORN"; Horn screen draws clip names, a press calls `hornPlay(i)`, a second press while busy calls `hornStop()`; `/horn` handler as above.
- [ ] **Step 6: Build** — `pio run -e ledpanel`, no warnings.
- [ ] **Step 7: Commit** — `feat(horn): HORN menu screen and /horn endpoint`

### Task 5: Diagram and docs for this version

**Files:**
- Modify: `docs/hardware/siren-wiring.drawio` (remove DFPlayer, GPIO27/35; add the 1:1 isolator between SPEAK and the attenuator; GPIO26/SPEAK label; note "SPEAK is BTL: no pin is ground")
- Create: `docs/hardware/siren-wiring.drawio.png` (export with `D:\Apps\draw.io\draw.io.exe -x -f png -e -b 10 -s 2`)
- Modify: `docs/leds-voice.md` (a "Horn" section: `/horn` usage, clip tool, limits)

- [ ] **Step 1: Redraw and export**; check the PNG has no wire across a label or box.
- [ ] **Step 2: Docs section.**
- [ ] **Step 3: Commit** — `docs(horn): wiring without the DFPlayer, /horn usage`

### Task 6: Bench test on the real horn (João + agent)

**Files:** none (results go in the PR description).

- [ ] **Step 1: Pinout known** — the mic cable's SINAL / PTT / MALHA are measured and drawn (precondition; see `docs/hardware/siren-wiring.drawio`).
- [ ] **Step 2: Wire per the diagram** on the bench: 12 V through the 5 A fuse; isolator primary on SPEAK+/SPEAK−; secondary → 10 kΩ → node → 1 µF → SINAL, node → 100 Ω (or 1 kΩ trimpot) → MALHA; GPIO22 → 1 kΩ → PC817 LED → GND; PC817 collector → PTT, emitter → MALHA.
- [ ] **Step 3: Check** — `/horn?clip=1` on the hotspot: the horn switches to voice and plays "beep"; PTT drops right after; the hand mic still works; no hum with the engine running. Tune the 100 Ω for clean audio. If PTT doesn't close, drop the PC817 resistor to 470 Ω.
- [ ] **Step 4: Your clips** — put WAV/MP3s in `horn-clips/`, `python tools/gen_horn.py horn-clips/`, flash (`pio run -e ledpanel_ota -t upload`), play each from the menu.
