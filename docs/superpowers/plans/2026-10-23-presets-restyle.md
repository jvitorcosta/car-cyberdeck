# Presets and a more faithful terminal look: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the four saved favourites with four fixed, named presets. Make every yellow-deck screen
look more like a Fallout RobCo terminal (scanlines, caps, list rows, Pip-Boy tabs, block bars, a
typed boot), with amber phosphor at night, terminal-style status messages, and the lights' real colours on the
menu strip. The watch's Favourites page becomes Presets.

**Architecture:** This builds on branch `feat/yellow-menu` (plan `2026-10-23-yellow-deck-menu.md`, done and reviewed).
The same split as before: pure host-tested modules in `src/util/`, with drawing and behaviour in `src/ledpanel.cpp`.
`ledfavs` (saved slots) is replaced by `ledpresets` (a fixed table). Behaviour changes (Task 3) and
the visual restyle (Task 4) are separate device tasks.

**Tech Stack:** as before (ESP32 CYD, PlatformIO `ledpanel`, TFT_eSPI, NimBLE 2, Unity host tests, Connect IQ 9.3).

**Spec:** `docs/superpowers/specs/2026-10-23-yellow-deck-menu-design.md`. Task 6 updates its favourites
sections with the owner's 2026-10-23 decisions below, which override it where they differ.

## Owner's decisions (2026-10-23, binding)

- Four **fixed presets**, no saving on the deck:
  1 `ROSE`: inside pink 40 %, outside pink 100 %
  2 `FIRE`: inside red 50 %, outside red 100 %
  3 `CALM`: inside white 40 %, outside white 40 %
  4 `PURPURA`: inside purple 40 %, outside purple 100 %
  All four have both lights on.
- Voice: "rose", "fire", "calm", "purpura" apply a preset. "preset/favourite + one..four or 1..4" still works.
- The look: faint scanlines, ALL CAPS bracket labels, the inverted bar for selection, list rows on the menu, presets as the menu's main element,
  Pip-Boy tabs for the zones, bordered colour boxes with a **big colour chip**, a block level bar, a typed RobCo boot under 1 s,
  and a blinking block cursor on the menu strip.
- **Amber at night** instead of dimmed green. **Terminal status messages** (`>ROSE APPLIED`, `>INSIDE: NO LINK`).
  The **menu strip shows the actual colours** (`IN PINK 40%  OUT PINK 100%`).

## Global Constraints

- Branch `feat/yellow-menu`, worktree `D:\dev\projects\spotify-pokemon-deck-menu`. Commit per task (conventional commits, scopes `leds` / `garmin`). Don't push.
- Public repo: never commit `src/config.h`, `garmin/carlights/source/Secrets.mc`, `garmin/carlights/bin/`.
- `/say` and `/state` keep their formats for colour/power/level commands. `/status` is unchanged.
- Voice stays English. "purpura" is the owner's preset name, the only exception.
- Every touch target is at least 36 px tall and 70 px wide. The colour buttons keep their current rects (calibration and muscle memory).
- Night: 18:00-06:00 (`daynight`), backlight day 255 / night 90 (`ui/backlight`). Night = amber palette. The screen only.
- The BOTH-while-mixed rules (`util/bothlook.h`) stay as they are. Applying a preset is like applying a favourite before.
- `src/` builds with `-Werror` (incl. `-Wshadow`, `-Wdouble-promotion`, `-Wformat-truncation`).
- Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`. Device build: `pio run -e ledpanel`.
- Watch build: `monkeyc -d fr255 -f garmin/carlights/monkey.jungle -o garmin/carlights/bin/CarLights.prg -y D:\dev\tools\garmin\developer_key.der -w -l 2`.

## Review Focus

1. **Siri's spelling of the names**: "Rose.", "Fire!", "CALM", "Purpura" (any case, punctuation) must apply the preset. "purple" must still mean the colour. *Pinned in Task 1.*
2. **A preset name mixed with other words**: "rose 50" or "set fire" still applies the preset only, with no level and no colour. *Pinned in Task 1.*
3. **The night palette**: every role has blue 0 (amber), it is dimmer than day, and nothing on screen stays green at night. *Pinned in Task 2, and checked in Task 4's review.*
4. **A status message that outlives its screen**: a message shows ~2.5 s, then the normal line comes back. A screen change or a newer message replaces it, with no stale text left behind. *Task 3.*
5. **A redraw during the boot typing**: voice or the watch acting while the boot animation runs must not draw over it or be lost. *Task 4.*

---

### Task 1: Presets, voice names, strip and message text (pure)

**Files:**
- Create: `src/util/ledpresets.h`, `src/util/ledpresets.cpp`, `test/test_ledpresets/test_ledpresets.cpp`
- Delete: `src/util/ledfavs.h`, `src/util/ledfavs.cpp`, `test/test_ledfavs/` (only after Task 3 stops using them. In this task, leave them and let Task 3 delete them.)
- Modify: `src/util/ledvoice.h` (rename `Command::fav` to `Command::preset`, add the name words), `test/test_ledvoice/test_ledvoice.cpp`
- Modify: `src/util/lightstatus.h/.cpp` (add `strip` and the messages), `test/test_lightstatus/test_lightstatus.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace ledpresets {
  constexpr int N = 4;
  struct Preset { const char* name; const char* word; ledlook::Look in, out; };  // name "ROSE", word "rose"
  extern const Preset PRESETS[N];   // values: Owner's decisions; swatch indices from ledlook (pink 2, red 0, white 6, purple 3)
  // slot 1..4: "Rose: inside Pink 40 percent, outside Pink 100 percent" (name in title case)
  void describe(int slot, char* out, size_t n);
  // one /presets line: "1 ROSE pink 40 pink 100"
  void wireLine(int slot, char* out, size_t n);
  }
  namespace ledvoice { /* Command::preset: 1..4 or -1 (was fav) */ }
  namespace lightstatus {
  // menu strip: "IN PINK 40%  OUT PINK 100%"; off "IN OFF", no link "IN --"
  void strip(const ledlook::Look& in, bool inUp, const ledlook::Look& out, bool outUp, char* buf, size_t n);
  // messages: ">ROSE APPLIED", ">INSIDE: NO LINK", ">OUTSIDE: LINKED"
  void applied(const char* presetName, char* buf, size_t n);
  void linkChange(bool inside, bool up, char* buf, size_t n);
  }
  ```
- Voice: the words `rose`, `fire`, `calm`, `purpura` set `preset` to 1..4 directly. They count as recognised command words, so they disarm an armed favourite word.
- [ ] **Step 1: Write the failing tests:**
  - `test_ledpresets`: `PRESETS[0]` is ROSE, in pink 40 lit, out pink 100 lit. Check all four against the table. `describe(1)` gives `"Rose: inside Pink 40 percent, outside Pink 100 percent"`. `describe(4)` starts `"Purpura: "`. `wireLine(3)` gives `"3 CALM white 40 white 40"`. Every describe line fits in 112.
  - `test_ledvoice` (rename existing fav tests to preset): `"Rose."`→1, `"FIRE!"`→2, `"calm"`→3, `"Purpura"`→4, each with `level == -1` and `color == -1`. `"rose 50"` gives preset 1 and `level == 50`: the plain rule. *Ruling:* Task 3 applies the preset and **ignores** level/colour when `preset > 0`. `"purple"` gives `color == 3` and `preset == -1`. `"preset two"` gives 2. `"favourite number 2"` gives 2.
  - `test_lightstatus`: `strip` (pink 40 lit up, pink 100 lit up) gives `"IN PINK 40%  OUT PINK 100%"`. Off gives `"IN OFF"`. No link gives `"IN --"`. `applied("ROSE")` gives `">ROSE APPLIED"`. `linkChange(true,false)` gives `">INSIDE: NO LINK"`. `linkChange(false,true)` gives `">OUTSIDE: LINKED"`.
- [ ] **Step 2: Run** the host tests. They fail.
- [ ] **Step 3: Implement.** `ledpanel.cpp` uses `Command::fav` today. Rename the use there in the same commit so the build stays green (`c.fav` → `c.preset`, still mapped to the favourite apply until Task 3).
- [ ] **Step 4: Run** the host tests (all pass) and `pio run -e ledpanel` (it builds).
- [ ] **Step 5: Commit** `feat(leds): four named presets, their voice words, strip and message text`.

### Task 2: Menu layout and amber night palette (pure)

**Files:**
- Modify: `src/util/deckmenu.h/.cpp`, `test/test_deckmenu/test_deckmenu.cpp`, `src/util/termpal.h/.cpp`, `test/test_termpal/test_termpal.cpp`

**Interfaces:**
- deckmenu: rename `M_FAV0` → `M_PRE0`, `L_FAV0` → `L_PRE0`, `isFav` → `isPreset`, `favSlot` → `presetSlot`. Remove `HOLD_MS`, since there is no saving. Menu layout values:
  - `HEADER{0,0,320,44}` (three text lines);
  - item slot i as a full-width row `{4, 48 + i*40, 312, 36}`, with at most 2 item slots;
  - preset k as a big block `{4 + (k%2)*158, 92 + (k/2)*46, 154, 42}`;
  - `MENU_STRIP{0,184,320,56}`.
  - Enum ids: `M_ITEM0 = 0, M_PRE0 = 2, M_N = 6`.
  - Lights rects are unchanged (`L_PRE0 = 12`).
- termpal: night becomes **amber**. Day is unchanged. Night `TEXT 0xB37B00`, `MID 0x6E4C00`, `DIM 0x372600`, `BG 0x000000`. `tile()` is unchanged (x70 % at night). Add `uint32_t scan(bool night)`, the scanline colour: day `0x031008`, night `0x0A0700`.
- [ ] **Step 1: Write the failing tests:**
  - The deckmenu tests keep checking that rects are on screen, don't overlap, and are ≥70x36, against the new menu values.
  - Down from `M_ITEM0` is `M_PRE0`. Right from `M_PRE0` is `M_PRE0+1`. Down from `M_PRE0` is `M_PRE0+2`.
  - `presetSlot(Menu, M_PRE0+3) == 3`.
  - termpal: night TEXT is `0xB37B00`. Every night role has blue 0 and each channel ≤ day's red channel. `scan(false) == 0x031008`.
- [ ] **Step 2: Run.** It fails.
- [ ] **Step 3: Implement.** Update `ledpanel.cpp`'s uses of the renamed ids so the build stays green. No other behaviour change.
- [ ] **Step 4: Run** the host tests and `pio run -e ledpanel`. Both pass.
- [ ] **Step 5: Commit** `feat(leds): menu rows and preset blocks, amber night palette`.

### Task 3: Deck behaviour: presets, messages, strip, /presets

**Files:**
- Modify: `src/ledpanel.cpp`. Delete: `src/util/ledfavs.*`, `test/test_ledfavs/`.

Behaviour:
- **Presets replace favourites.**
  - Tap or A on a preset applies it, on the Menu and on Lights, acting on settle like other buttons. The hold-to-save logic, the 1 s Xbox hold, `saveFav`, the NVS keys and the save flash are removed.
  - Apply = the old `applyFav` with the preset's looks (seed BOTH as `bothlook::seed`, push both lights, save the looks, redraw).
  - Old NVS `fav0..3` keys are left alone; nothing reads them.
- **Voice**: `c.preset > 0` applies that preset and ignores colour, level and zone in the same phrase. The reply is `ledpresets::describe`.
- **HTTP**: `GET /presets?k=` → the 4 `wireLine`s joined by `\n`, with 403 "Wrong key". `/favs` is removed.
- **Status messages**:
  - On the Lights page the `STATUS` line, and on the Menu the strip's text line, show a message for 2500 ms. Then the normal line comes back.
  - Messages: `applied(name)` on a preset apply (from any source), and `linkChange` when a light connects or drops.
  - A screen change or a newer message replaces the old one. The message redraw goes through the same night palette.
- **Menu strip**: `lightstatus::strip` replaces the IN/OUT ON boxes. It is text, with a small colour chip before each light's colour name (drawn in `termpal::tile`).
- Spec Review Focus 4: a test-free device task. The reviewer checks the timer and screen-change paths.
- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** the host tests (the `test_ledfavs` suite is gone) and `pio run -e ledpanel`.
- [ ] **Step 3: Commit** `feat(leds): presets replace favourites; status messages; real colours on the strip`. Note the on-device checks in the body: a tap on ROSE applies it and shows `>ROSE APPLIED`; `curl /presets`; "Honda, rose"; a link drop shows `>INSIDE: NO LINK`.

### Task 4: Deck restyle: scanlines, caps, list rows, tabs, chips, block bar, boot, cursor

**Files:**
- Modify: `src/ledpanel.cpp`

Visual rules (every screen except touch calibration):
- **Background** is black with a scanline every 3rd pixel row in `termpal::scan(night)`. Every place that clears a rect (`fillRect`/`fillRoundRect` with BG) goes through one helper that repaints the bg with its scanlines, so partial redraws keep the lines aligned to absolute screen rows (`y % 3 == 0`).
- **Text** labels are ALL CAPS. Buttons read `[LIGHTS]`, `[-]`, `[+]`, `< BACK`. **Selection** (focus, the current tab, the lit colour) is the inverted bar: TEXT fill with BG text. Pressed stays yellow, tinted.
- **Menu**:
  - The header has three lines: `WELCOME TO ROBCO INDUSTRIES (TM) TERMLINK` (TEXT), `>SET TERMINAL/INQUIRE` (TEXT), `HONDA CITY // CARDECK OS` (MID). Then the `[LIGHTS]` row starting `> `.
  - Each preset block has its name in caps and two big chips (≈18x18, inside and outside colour via `termpal::tile`, DIM outline when off) with their levels.
  - The strip has the clock, the `strip` line or a message, and a **block cursor** blinking at ~1 Hz at the end of the text line. The blink redraws only the cursor cell.
- **Lights**:
  - The zones are **Pip-Boy tabs**: a TEXT rule under the tab row. The current tab has a TEXT outline open at the bottom, the others are MID text. `< BACK` sits at the left.
  - Each colour button is a black box with a MID border, a **big chip** (≈30x30, left-aligned and vertically centred) and the name in caps to its right. The lit colour is the inverted bar with the chip kept.
  - The level box is a **block bar**: 10 cells, `█` for each 10 % and `░` for the rest, then `70%`, or `MIX`, or `OFF`.
  - The preset row shows each full name in caps (`PURPURA` included; pick a font that fits 75 px), plus two small chips.
- **Boot**: after the touch calibration check and before the first `drawAll()`, the three header lines type out at ~6 ms per character, then the menu appears. The whole boot stays under 1 s. Voice jobs and link changes that arrive meanwhile wait in the queue or flags and are handled after it (Review Focus 5). No drawing from other paths happens during the typing.
- The night switch keeps `backlight::set` before the repaint. Amber comes from termpal.
- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** the host tests and `pio run -e ledpanel`, both pass. Also build with `-D NIGHT_TEST` (amber).
- [ ] **Step 3: Commit** `feat(leds): a more faithful RobCo terminal on every screen`. Put the on-device checks in the body.

### Task 5: Watch: Presets page

**Files:**
- Modify: `garmin/carlights/source/Deck.mc`, `ZonesView.mc`, `ZonesDelegate.mc`, `CarLightsApp.mc` (comment), `FavDots.mc` (rename optional)

- `/favs` becomes `/presets`. Each line is `"1 ROSE pink 40 pink 100"`. Parse the number, the **name**, then two lights as before. Store `presets` as `[name, inColor, inPct, inLit, outColor, outPct, outLit]`.
- The 4th page is **PRESETS**. Each row shows the name and two dots. START opens a Menu2 titled "Presets" with items labelled by name, e.g. "ROSE", sublabel "Pink 40 / Pink 100". Selecting one sends `/say?text=preset N`, using the number, which is robust; the reply isn't absorbed and the looks re-sync, as before.
- "None saved" goes away. If `/presets` failed, the page shows "No presets yet" and START does nothing.
- [ ] **Step 1: Implement.** **Step 2:** Build (no new warnings). **Step 3: Commit** `feat(garmin): presets page with names`.

### Task 6: Docs and spec

- `docs/leds-voice.md`:
  - Favourites become presets: the table of four, the voice words, `/presets` and its format.
  - The restyle and amber night, briefly.
  - Remove hold-to-save.
- `docs/BACKLOG.md` Verification: update the pending checklist. No saving; presets, voice names, amber, the boot typing, the cursor, messages, the strip.
- The spec: in the favourites sections, add a dated note "2026-10-23 (owner): fixed named presets replace saved favourites" with the table, plus the amber, messages and strip decisions.
- [ ] **Step 1: Write.** **Step 2: Commit** `docs(leds): presets and the terminal restyle`.
