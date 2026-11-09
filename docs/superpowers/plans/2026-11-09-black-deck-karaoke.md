# Karaoke on the Black Deck Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The black deck shows a full-screen karaoke view (the line sung swept dark, the walking Pokémon on it, the next line waiting) while the yellow deck's KARAOKE switch is on; the yellow deck stops fetching lyrics and song state.

**Architecture:** Two pure, host-tested modules on the black deck decide everything that can be decided off-device: which lines show and where the sweep is (`util/black/karaokepick`), and whether the karaoke view, the "no lyrics" message or the normal screen shows (`util/black/karaokemode`). A signed `GET /karaoke?on=1|0` keeps the mode alive like `/music`. The yellow deck's KARAOKE item becomes a switch whose keep-alive rides in its existing deck task; its KARAOKE page, `/state` polling, now-playing band and Japanese font are removed.

**Tech Stack:** C++17, Arduino-ESP32 (PlatformIO `espressif32@6.9.0`), TFT_eSPI 2.5.43, Unity host tests via `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-11-09-black-deck-karaoke-design.md`

## Global Constraints

- Public repo: no secrets, addresses, SSIDs or personal data in code, docs or logs.
- Conventional commits `type(scope): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; no merge without João.
- Builds clean with `-Werror` (`pio run -e s3`, `-e ledpanel`, `-e obddeck`); `tools/run_tests.ps1` and `python -m pytest tools` pass.
- The yellow deck flashes over USB only (COM port moves: check `Get-PnpDevice`, the CH340). Ask João before touching the black deck's port (COM12) or flashing it.
- Keep-alive: the yellow deck sends `on=1` at once and every 10 s (`MUSIC_KEEP_MS`); the black deck drops karaoke after 30 s without one.
- No-lyrics timer: 30 s, counted from the first moment the song's status is Plain, Instrumental or None; Searching, Retrying and the intro before the first line never count.
- The yellow deck's `[mem]` heap must not go down: record it before Task 6 and after.
- Task 6 edits `src/yellow/ledpanel.cpp`, which PR #54 (`fix/yellow-restarts`) also changes: start Task 6 after #54 is merged, or rebase this branch on it first.
- Match the files' style: comment density, naming, the `namespace` + `// PURE, host-tested.` header for util modules.

## Review Focus

- A song change in the middle of a line: the view must reset to the new song's lines (or its search message), never show the old song's line. Test in Task 2 (`test_a_new_song_restarts_the_timer`) and Task 1 (picks come from the lyrics passed in, no state).
- Seeking backwards or the position briefly behind the line's start (interpolation jitter): the sweep is 0, not negative or wrapped. Test in Task 1 (`test_sweep_clamps_before_start`).
- Two lines with the same timestamp, or a line whose end is not after its start: no division by zero, the sweep shows the line full. Test in Task 1 (`test_zero_length_line_is_full`).
- The yellow deck going silent while the karaoke view shows: back to the normal screen within 30 s. Test in Task 2 (`test_keepalive_lapse_turns_it_off`).
- Status going Searching → Retrying → None: the 30 s start only at None. Test in Task 2 (`test_retrying_does_not_count`).

---

### Task 1: Line picking, sweep and hop (pure)

**Files:**
- Create: `src/util/black/karaokepick.h`, `src/util/black/karaokepick.cpp`
- Test: `test/test_karaokepick/test_karaokepick.cpp`

**Interfaces:**
- Consumes: `lyricbuf::Lyrics`, `lyricbuf::lineText(l, i)`, `lyricbuf::Line{tMs, off}` (`src/util/black/lyricbuf.h`).
- Produces:
  - `namespace karaokepick`, `constexpr uint32_t LAST_LINE_MS = 5000; constexpr uint32_t HOP_MS = 300; constexpr int HOP_PX = 14;`
  - `struct Pick { int prev, cur, next; uint32_t startMs, endMs; };` (indices into `l.lines`, -1 = none)
  - `Pick pick(const lyricbuf::Lyrics& l, uint32_t posMs, uint32_t durationMs);`
  - `int sweepPx(uint32_t posMs, uint32_t startMs, uint32_t endMs, int widthPx);`
  - `struct Hop { int x; int lift; bool done; };`
  - `Hop hopAt(uint32_t sinceMs, int fromX, int toX);`

Rules for `pick` (from the spec):
- A line is blank when `lineText` is empty (after trimming spaces).
- `cur` = the last non-blank line with `tMs <= posMs`; before the first non-blank line (the intro) `cur = -1` and `next` = the first non-blank line.
- `startMs` = `cur`'s time; `endMs` = the time of the line right after `cur` in the file, blank or not (so a blank line ends the sweep and the line stays full through the break). For `cur` the last line: `min(startMs + LAST_LINE_MS, durationMs)` (or `startMs + LAST_LINE_MS` when `durationMs` is 0).
- `prev` = the last non-blank line before `cur`; `next` = the first non-blank line after `cur`.

- [ ] **Step 1: Write the failing tests**

Build lyrics the way `test/test_lyricbuf` does, feeding LRC lines to `lyricbuf::addLine` (`"[00:04.00]B"` is B at 4000 ms, `"[00:04.00]"` a blank line). Tests:
- `test_picks_prev_cur_next` — lines A@1000, B@4000, C@7000; `pick(l, 5000, 0)` → prev=A, cur=B, next=C, startMs 4000, endMs 7000.
- `test_intro_has_no_current` — `pick(l, 500, 0)` → cur -1, prev -1, next=A.
- `test_blank_line_keeps_the_last_sung` — A@1000, ""@4000, B@9000; `pick(l, 6000, 0)` → cur=A, endMs 4000, next=B, prev -1.
- `test_blank_is_never_prev` — A@1000, ""@4000, B@9000; `pick(l, 9500, 0)` → prev=A, cur=B.
- `test_last_line_ends_at_song_or_cap` — A@1000 only; `pick(l, 2000, 3000).endMs == 3000`; `pick(l, 2000, 0).endMs == 6000`.
- `test_sweep_is_proportional` — `sweepPx(5500, 4000, 7000, 300) == 150`.
- `test_sweep_clamps_before_start` — `sweepPx(3000, 4000, 7000, 300) == 0`.
- `test_sweep_clamps_after_end` — `sweepPx(9000, 4000, 7000, 300) == 300`.
- `test_zero_length_line_is_full` — `sweepPx(4000, 4000, 4000, 300) == 300`.
- `test_hop_goes_up_and_lands` — `hopAt(0, 250, 10)` → x 250, lift 0, not done; `hopAt(HOP_MS / 2, 250, 10)` → x 130, lift `HOP_PX`; `hopAt(HOP_MS, 250, 10)` → x 10, lift 0, done.

- [ ] **Step 2: Run them to see them fail**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (or `tools\ntest.ps1 test\test_karaokepick\test_karaokepick.cpp src\util\black\karaokepick.cpp src\util\black\lyricbuf.cpp`)
Expected: `FAIL test_karaokepick` (does not compile: `karaokepick.h` missing).

- [ ] **Step 3: Implement `karaokepick` per the Interfaces block**

`hopAt`: x moves linearly from `fromX` to `toX` over `HOP_MS`; `lift` is a parabola `4 * HOP_PX * t * (1 - t)` with `t = sinceMs / HOP_MS` clamped to 1; `done` when `sinceMs >= HOP_MS`.

- [ ] **Step 4: Run the tests to see them pass**

Expected: `PASS test_karaokepick: 10 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/util/black/karaokepick.h src/util/black/karaokepick.cpp test/test_karaokepick
git commit -m "feat(black): karaoke line picking, sweep and hop"
```

### Task 2: The karaoke switch and screen choice (pure)

**Files:**
- Create: `src/util/black/karaokemode.h` (header-only, like `util/yellow/padtrigger.h`)
- Test: `test/test_karaokemode/test_karaokemode.cpp`

**Interfaces:**
- Consumes: `lyricstatus::Status` (`src/util/wire/lyricstatus.h`: `Searching, Retrying, Synced, Plain, Instrumental, None`).
- Produces:
  - `namespace karaokemode`, `constexpr uint32_t KEEP_MS = 30000; constexpr uint32_t NO_LYRICS_MS = 30000;`
  - `enum class Screen : uint8_t { Normal, Karaoke, NoLyrics };`
  - `class Mode` with: `void keepAlive(uint32_t nowMs);` (turns on, refreshes), `void stop();`, `bool on(uint32_t nowMs) const;` (false once `KEEP_MS` passed since the last keep-alive), `Screen screen(lyricstatus::Status s, uint32_t gen, uint32_t nowMs);`

Rules for `screen`: off → `Normal`. On: Searching, Retrying, Synced → `Karaoke`. Plain, Instrumental, None → `NoLyrics` until `NO_LYRICS_MS` after the first call that saw one of them for this `gen`, then `Normal` for the rest of that `gen`. A new `gen` restarts the timer.

- [ ] **Step 1: Write the failing tests**

- `test_off_is_normal` — a fresh `Mode`: `screen(Synced, 1, 0) == Normal`.
- `test_synced_shows_karaoke` — `keepAlive(0)`; `screen(Synced, 1, 100) == Karaoke`; `screen(Searching, 1, 100) == Karaoke`.
- `test_no_lyrics_for_30s_then_normal` — `keepAlive(0)`; `screen(None, 1, 1000) == NoLyrics`; at 30999 still `NoLyrics` (keep-alive refreshed at 20000); at 31000 `Normal`.
- `test_plain_counts_as_none` — Plain behaves like None.
- `test_retrying_does_not_count` — `screen(Retrying, 1, 0..25000)` stays `Karaoke`; `screen(None, 1, 25000) == NoLyrics`; at 54999 still `NoLyrics` (keep-alives every 10 s).
- `test_a_new_song_restarts_the_timer` — after gen 1 went `Normal` by the timer, `screen(Synced, 2, t) == Karaoke`; `screen(None, 3, t) == NoLyrics`.
- `test_keepalive_lapse_turns_it_off` — `keepAlive(0)`; `on(29999)` true; `on(30000)` false; `screen(Synced, 1, 30000) == Normal`.
- `test_stop_is_at_once` — `keepAlive(0)`; `stop()`; `screen(Synced, 1, 1) == Normal`.

- [ ] **Step 2: Run them to see them fail** — Expected: `FAIL test_karaokemode` (header missing).
- [ ] **Step 3: Implement `karaokemode::Mode`** per the Interfaces block; use wrap-safe `nowMs - at` arithmetic.
- [ ] **Step 4: Run them to see them pass** — Expected: `PASS test_karaokemode: 8 Tests 0 Failures`.
- [ ] **Step 5: Commit** — `git commit -m "feat(black): karaoke switch and screen choice"`

### Task 3: The black deck's route and lyric lines

**Files:**
- Create: `src/black/core/karaoke.h`, `src/black/core/karaoke.cpp`
- Modify: `src/black/core/shared.h` (after `lyricPair`, ~line 46), `src/black/core/shared.cpp` (after `lyricPair`, ~line 91)
- Modify: `src/black/net/remote.cpp` (a handler next to `handleMusic` ~line 153, and `s_server->on("/karaoke", ...)` next to `/music` ~line 312)

**Interfaces:**
- Consumes: `karaokemode::Mode` (Task 2), `karaokepick::pick` (Task 1), `reqauth::pass` as `handleMusic` uses it.
- Produces:
  - `namespace karaoke { void keepAlive(); void stop(); karaokemode::Screen screen(lyricstatus::Status s, uint32_t gen); }` — a `karaokemode::Mode` under a `portMUX`, `millis()` inside; any task.
  - `shared::KaraokeLines { lyricstatus::Status status; char prev[160], cur[160], next[160]; uint32_t startMs, endMs; }` and `void karaokeLines(uint32_t gen, uint32_t posMs, uint32_t durationMs, KaraokeLines& out);` — under the shared lock like `lyricPair`, filled from `karaokepick::pick`; empty strings for -1 indices; zeros when the lyrics are not for `gen`.
  - `GET /karaoke?on=1` → `karaoke::keepAlive()`, 200 `{"on":1}`; `on=0` → `karaoke::stop()`, 200 `{"on":0}`; anything else 400 `on=1 or on=0`. Signed or the key, through `reqauth::pass(*s_server, s_gate, "/karaoke", "wrong key")`.

- [ ] **Step 1: Implement `core/karaoke` and `shared::karaokeLines`** per the Interfaces block (no host test: thin wrappers over Tasks 1-2).
- [ ] **Step 2: Add the route** in `remote.cpp` and log `[karaoke] on` / `[karaoke] off` on a change of state only (not every keep-alive).
- [ ] **Step 3: Build** — Run: `pio run -e s3`. Expected: `[SUCCESS]`, no warnings in `src/`.
- [ ] **Step 4: Commit** — `git commit -m "feat(black): GET /karaoke and the karaoke lines"`

### Task 4: The black deck's karaoke view

**Files:**
- Create: `src/black/ui/karaokeview.h`, `src/black/ui/karaokeview.cpp`
- Modify: `src/black/ui/screen_now.cpp:114-156` (`drawWalker`): move the sprite blit into a shared function
- Modify: `src/black/ui/screen_now.h` (declare it)
- Modify: `src/black/main.cpp` (mode 0 branch, ~lines 517-591)

**Interfaces:**
- Consumes: `karaoke::screen` and `shared::karaokeLines` (Task 3), `karaokepick::sweepPx/hopAt/HOP_MS` (Task 1), `lyricmsg::compose` for the searching and no-lyrics messages, `ui::drawRunScaled` / `ui::drawRun` / `glyphrun::wrapTwo`, `ui::dialogueBox`, `theme::pal()`.
- Produces:
  - `void ui::blitWalker(TFT_eSPI& t, int cx, int feetY, uint32_t animMs, int step, walkrect::Rect& prev);` — the existing body of `drawWalker` from the frame pick to `pushImage`, with the caller's `cx`, feet line and dirty-rect state; `drawWalker` calls it with its own `prev` (no visible change on the normal screen).
  - `void ui::drawKaraoke(TFT_eSPI& t, const AppState& st, const shared::KaraokeLines& k, const char* message);` — whole view: top strip (song, artist, progress bar as on the normal screen), the dialogue box, prev line small at the box's top, current line at scale 2 dim (wrapped to two rows with `wrapTwo`, "..." past that), next line small underneath. `message` non-null (searching or no lyrics, from `lyricmsg`) is drawn in the current line's place instead.
  - `void ui::drawKaraokeSweep(TFT_eSPI& t, int px);` — draws the current line's lit run clipped (`setViewport`) to the band between the last drawn px and `px` (both rows: row 1's width first, then row 2); forgets on `drawKaraoke`.
  - `void ui::drawKaraokeWalker(TFT_eSPI& t, uint32_t animMs, int step, int sweepPx, int lift);` — the Pokémon via `blitWalker`, its feet on row 1's top edge, x = row 1's left + `sweepPx` mapped over row 1's width (both rows' progress, the spec's wrapped-line rule), raised by `lift`.

Behaviour in `main.cpp` mode 0:
- Each `PROGRESS_MS` tick: `const auto sc = karaoke::screen(status, gen)`. A change of `sc`, of `gen`, or of the picked `cur` line forces a whole draw (`drawNow` for `Normal`, `drawKaraoke` otherwise). On a `cur` change start a hop: remember `hopFrom = last walker x`, `hopAt0 = millis()`.
- While `Karaoke` with a current line: every loop pass at the walker's pace (`WALK_FRAME_MS`), `drawKaraokeSweep(sweepPx(pos, start, end, width))` and `drawKaraokeWalker(..., lift)` with x/lift from `hopAt(millis() - hopAt0, hopFrom, sweepX)` until `done`, then from the sweep.
- Paused (`!st.playing`): position and animation clocks stop, as the normal walker's do.
- The OTA screen, status scenes and camera band keep their order over mode 0 unchanged.

- [ ] **Step 1: Extract `blitWalker`**; build `pio run -e s3`; Expected `[SUCCESS]`.
- [ ] **Step 2: Implement `karaokeview`** per the Interfaces block; layout values (box rect, row y's, colours) are the builder's within the mockup's arrangement (`.superpowers/brainstorm/*/content/karaoke-look.html`, option B).
- [ ] **Step 3: Wire it into `main.cpp`** as above.
- [ ] **Step 4: Build** — `pio run -e s3`; Expected `[SUCCESS]`.
- [ ] **Step 5: Ask João to flash the black deck** (USB on COM12 or `pio run -e s3_ota -t upload`), then check by hand with the key: `curl "http://<black deck>/karaoke?on=1&k=<key>"` every 10 s while a synced song plays. Expected: the view, the sweep, the Pokémon walking and hopping; a song without lyrics returns to the normal screen 30 s after its "no lyrics" message; no keep-alive for 30 s returns to normal.
- [ ] **Step 6: Commit** — `git commit -m "feat(black): the karaoke view with the walking Pokémon"`

### Task 5: The voice words (pure)

**Files:**
- Modify: `src/util/yellow/ledvoice.h` (the `music` field ~line 21, the `MUSIC` words ~line 102, the parse ~lines 114-191)
- Test: `test/test_ledvoice/test_ledvoice.cpp`

**Interfaces:**
- Produces: `int karaoke = -1;` in the parse result next to `music` (1 on, 0 off, -1 not asked), the same rules as `music`: "karaoke" on, with an off word off, and then nothing else in the phrase.

- [ ] **Step 1: Write the failing tests** — `test_karaoke_on` ("karaoke" → karaoke 1, music -1, no colour), `test_karaoke_off` ("karaoke off" → 0), `test_karaoke_only` ("karaoke red" → karaoke 1 and no colour change, as "music red" does).
- [ ] **Step 2: Run them to see them fail** — Expected `FAIL test_ledvoice`.
- [ ] **Step 3: Implement** alongside the `MUSIC` words (`KARAOKE[] = {"karaoke", "karaokê", "caraoquê"}`).
- [ ] **Step 4: Run them to see them pass** — Expected `PASS test_ledvoice`.
- [ ] **Step 5: Commit** — `git commit -m "feat(yellow): the karaoke voice word"`

### Task 6: The yellow deck's KARAOKE switch, and the lyrics code out

Start after PR #54 is merged (see Global Constraints). Record the yellow deck's `[mem]` line at the desk first.

**Files:**
- Modify: `src/yellow/ledpanel.cpp` (the Menu item ~line 763, `karaokeReady` ~line 230, the KARAOKE screen ~lines 1130-1440, the lyrics task ~lines 2148-2433, `applyVoice`, `publish`)
- Modify: `src/util/yellow/deckmenu.h` (drop `Screen::Karaoke`, `K_BACK`, `KARAOKE_BODY`; keep the `KARAOKE` item)
- Modify: `platformio.ini` (`env:ledpanel`: drop `board_build.embed_files = data/jp16.bin`)
- Delete: `src/yellow/ui/jpfont.*`, `src/yellow/ui/lyricrun.*`, `src/util/yellow/karaoke.*`, `src/util/yellow/pollpace.h`, `src/util/wire/stateparse.*` and their tests (`test_jpfont`, `test_lyricrun`, `test_karaoke`, `test_pollpace`, `test_stateparse`) — each only if `grep` finds no other user; `tools/gen_jp_font.py` and `data/jp16.bin` go only if nothing else references them
- Modify: `test/test_deckmenu/test_deckmenu.cpp` for the removed screen

**Interfaces:**
- Consumes: `deckGet`, `deckAnswered`, `musicKeepAlive`'s pattern, `MUSIC_KEEP_MS`/`MUSIC_RETRY_MS`; the black deck's `GET /karaoke` (Task 3); `ledvoice` `karaoke` (Task 5).
- Produces: `static bool karaokeOn` (loop-owned) and `static volatile bool karaokeWant` (for the deck task), `setKaraoke(bool on)` mirroring `setMusic` (message `>KARAOKE ON` / `>KARAOKE OFF`, item redraw with `ON_TAG`/`OFF_TAG`, `padFelt()` from the controller), `karaokeKeepAlive()` mirroring `musicKeepAlive` on `/karaoke`.

- [ ] **Step 1: Make KARAOKE a switch** — tap, cross on focus and the voice word call `setKaraoke(!karaokeOn)`; the item shows `ON`/`OFF` like MUSIC MODE; `karaokeKeepAlive()` runs in the deck task's loop next to `musicKeepAlive()`.
- [ ] **Step 2: Remove the KARAOKE page, the `/state` poll and the Menu's now-playing band** — the deck task keeps only the music and karaoke keep-alives and the black deck's address keeping; it wakes every 100 ms as it does now when nothing is wanted. Rename the task "deck" and its handle in the `[mem]` line.
- [ ] **Step 3: Remove the Japanese font and the helpers** listed above, and `platformio.ini`'s `jp16.bin` embed.
- [ ] **Step 4: Build and test** — `pio run -e ledpanel` → `[SUCCESS]`; `tools\run_tests.ps1` → `fail=0`.
- [ ] **Step 5: Flash the yellow deck over USB and check at the desk** — KARAOKE toggles `ON`/`OFF`; the black deck's log shows `[karaoke] on` and keep-alives; off stops it at once; `[mem]` heap at least the recorded value.
- [ ] **Step 6: Commit** — `git commit -m "feat(yellow): KARAOKE switches the black deck's karaoke; the lyrics page goes"`

### Task 7: Docs and the PR

**Files:**
- Modify: `docs/yellow-deck.md` (intro, Set up, On the deck: the Menu, `[KARAOKE]`, the KARAOKE screen and its messages, Theme, Memory), `README.md:34`, `docs/ARCHITECTURE.md` (diagram, routes, yellow task table, wire table, util list), the black deck's docs for `GET /karaoke` and the view, `docs/BACKLOG.md` (Japanese lyrics #21 moves to the black deck), `CHANGELOG.md` (Unreleased: Black deck Added, Yellow deck Changed/Removed).

- [ ] **Step 1: Update the docs** listed above to describe what was built.
- [ ] **Step 2: Full checks** — `pio run -e s3`, `pio run -e ledpanel`, `pio run -e obddeck` → `[SUCCESS]`; `tools\run_tests.ps1` → `fail=0`; `python -m pytest tools -q` → all pass.
- [ ] **Step 3: Commit, push and open the PR** against `main` (not merged), body ending with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`, with a test plan that leaves the car test unchecked.
