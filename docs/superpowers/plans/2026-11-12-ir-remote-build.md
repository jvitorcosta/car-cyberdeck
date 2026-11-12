# IR Remote Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The light kit's 24-button IR remote, read by a KY-022 on the yellow deck, drives both lights (level, on/off, colours, the controllers' effects) and starts four of the owner's Spotify playlists, shuffled, through the black deck.

**Architecture:** Decoding (IRremote) lives in a private library, `lib/irrx`, so `src/` keeps `-Werror`. Which code is which key, what each key does, and the hold-to-repeat rule are a PURE header (`src/util/yellow/irkeys.h`) with a host test. `src/yellow/ledpanel.cpp` polls `irrx` from `loop()` and acts through the same calls the PS3 pad uses. The playlist ask runs on the existing `deck` task. The black deck gains `shuffle=1` on `/playlists/play`.

**Tech Stack:** C++17, Arduino-ESP32 (espressif32@6.9.0), IRremote `z3t0/IRremote@4.4.1`, Unity host tests via `tools/run_tests.ps1`, PlatformIO envs `ledpanel`, `irtest`, `s3`.

**Spec:** `docs/superpowers/specs/2026-11-12-ir-remote-design.md`. Hardware, wiring and the remote's layout: `docs/superpowers/plans/2026-11-12-ir-remote.md` (the earlier handoff; its "Proposed mapping" and its Tasks are superseded by this plan).

## Global Constraints

- KY-022 on CN1: signal GPIO 27, powered from **3.3 V** only.
- Yellow deck heap: ~30 KB low water at worst (music mode streaming). No new task, no buffer over a few hundred bytes. **Gate:** if `[mem]` low water drops more than ~1 KB with IRremote in, stop and look.
- The black-deck ask runs on the `deck` task, never in `loop()`.
- Public repo: playlist IDs and names only in git-ignored `src/config.h`; `include/config.example.h` gets empty placeholders. No MACs, SSIDs, plate, vault paths.
- Flash the yellow deck over USB only (COM16, CH340). The black deck (COM12) only with the owner's OK, asked each time.
- Pure logic in `src/util/` headers with a host test `test/test_<name>/test_<name>.cpp`; run `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 <suites>`. Builds: `pio run -e ledpanel`, `pio run -e s3`.
- Commits: `type(scope): subject`, ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. A remote that sends a full frame again every ~110 ms while held (not NEC repeat frames) must not act twice on ON, OFF, a colour or ♪. Test: `test_a_resent_frame_is_a_repeat` (Task 2).
2. A ♪ press while the previous one is still waiting on the black deck must not queue a second play or block the keep-alives forever. Test: `test_playlist_presses_coalesce` (Task 2) plus the Task 6 desk check.
3. A code with this remote's address but an unknown command (a sibling remote's key) does nothing, logged once. Test: `test_unknown_command_is_ignored` (Task 2).
4. A colour press while both lights are off turns them on at their old levels, not 100 %. Test: `test_paint_turns_lights_on_at_their_levels` (Task 3).
5. A playlist with no tracks, or a track count Spotify doesn't send, still plays (from the start) and doesn't divide by zero. Test: `test_play_position_without_a_total` (Task 5).

---

### Task 1: Capture the codes (with the owner)

**Files:**
- Restore: `platformio.ini` `[env:irtest]`, `src/diag/irtest.cpp` (from `git stash` "feat/irtest: IR test work in progress")
- Modify: `docs/superpowers/plans/2026-11-12-ir-remote-build.md` (fill the code table below)

- [ ] **Step 1:** `git stash apply stash@{0}` on `feat/ir-remote` (keep the stash until the branch merges). `pio run -e irtest` → `[SUCCESS]`.
- [ ] **Step 2:** With the owner: confirm red = 3.3 V and black = GND on the CN1 cable (multimeter, module unplugged), then `pio run -e irtest -t upload --upload-port COM16` → `Hash of data verified`.
- [ ] **Step 3:** Record serial (pyserial on COM16, 115200) while the owner presses the 20 buttons once each in row order, then holds ☀▲ for 2 s. Expected: one `[ir] NEC addr 0x.... cmd 0x..` line per press, all one address, 24 distinct commands, and `(held)` lines while ☀▲ is held. Note whether a hold gives `(held)` repeat frames or full frames again.
- [ ] **Step 4:** Fill this table here and commit `docs(plan): the remote's IR codes`.

| Key | cmd | Key | cmd | Key | cmd | Key | cmd |
|---|---|---|---|---|---|---|---|
| ☀▲ | 0x00 | ☀▼ | 0x01 | ON | 0x02 | OFF | 0x03 |
| RED | 0x04 | GREEN | 0x05 | BLUE | 0x06 | WHITE | 0x07 |
| ORANGE | 0x08 | YELLOW | 0x09 | CYAN | 0x0A | PURPLE | 0x0B |
| JUMP3 | 0x0C | JUMP7 | 0x0D | FADE3 | 0x0E | FADE7 | 0x0F |
| ♪1 | 0x10 | ♪2 | 0x11 | ♪3 | 0x12 | ♪4 | 0x13 |

Address: `0xEF00` (NEC; raw `~cmd<<24 | cmd<<16 | 0xEF00`). Hold sends NEC repeat frames (`(held)`,
command repeated by IRremote). Captured 2026-11-12; the remote has 20 buttons, not 24. Stray
frames happen (UNKNOWN, PulseDistance, LG decodes of noise): only NEC at `0xEF00` counts.

### Task 2: `irkeys`: codes, actions and the repeat rule (pure)

**Files:**
- Create: `src/util/yellow/irkeys.h`, `test/test_irkeys/test_irkeys.cpp`

**Interfaces:**
- Produces:
  - `namespace irkeys`: `enum class Key : uint8_t { BrightUp, BrightDown, On, Off, Red, Green, Blue, White, Orange, Yellow, Cyan, Purple, Jump3, Jump7, Fade3, Fade7, Music1, Music2, Music3, Music4, N };`
  - `constexpr uint16_t ADDRESS` = Task 1's address; a `constexpr` table `{uint8_t command; Key key;}` of Task 1's 20 codes.
  - `bool lookup(uint16_t address, uint8_t command, Key* out);`: false for another address or an unknown command.
  - `enum class Kind : uint8_t { Level, Power, Colour, Effect, Playlist };` `struct Action { Kind kind; int value; };` and `Action action(Key k);`. Level ±1, Power 1/0, Colour = the `ledlook::Colour` swatch, Effect 0..3 (JUMP3, JUMP7, FADE3, FADE7), Playlist 0..3.
  - `constexpr uint32_t STEP_MS = 250, SAME_FRAME_MS = 200;` and `class Repeat { public: bool accept(Key k, bool repeat, uint32_t now); };`. Brightness keys act on the first frame, then on repeats at most once per `STEP_MS`. Every other key acts on its first frame only. A non-repeat frame of the same key within `SAME_FRAME_MS` of that key's last frame counts as a repeat. `now` compares wrap-safe (`int32_t` difference).

- [ ] **Step 1: Failing tests:**
  - `test_every_key_maps`: each of the 20 table codes looks up to its `Key`, and `action` gives the table in the spec (e.g. `Cyan` → `{Colour, ledlook::CYAN}`, `Fade7` → `{Effect, 3}`, `Music2` → `{Playlist, 1}`, `BrightDown` → `{Level, -1}`, `Off` → `{Power, 0}`).
  - `test_foreign_address_is_ignored`: a known command with `ADDRESS ^ 1` → false.
  - `test_unknown_command_is_ignored`: `ADDRESS` with a command not in the table → false.
  - `test_brightness_repeats_four_a_second`: BrightUp at t=0 (first) → true; repeats at 110, 220 → false, false; 330 → true; 440 → false; 580 → true.
  - `test_other_keys_act_once_while_held`: Off at 0 → true; repeats at 110…1100 → all false.
  - `test_a_resent_frame_is_a_repeat`: On (non-repeat) at 0 → true, On (non-repeat) at 110 → false, On (non-repeat) at 400 → true.
  - `test_playlist_presses_coalesce`: Music1 at 0 → true; Music1 non-repeat at 150 → false.
  - `test_repeat_survives_millis_wrap`: BrightUp first at 0xFFFFFF00, repeat at +260 → true.
- [ ] **Step 2:** `tools\run_tests.ps1 test_irkeys` → FAIL (no header).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Same run → PASS, 8 tests.
- [ ] **Step 5:** Commit `feat(yellow): the IR remote's keys and what each one does`.

### Task 3: Receive and drive the lights (level, power, colour)

**Files:**
- Create: `lib/irrx/irrx.h`, `lib/irrx/irrx.cpp` (the only file that includes `IRremote.hpp`)
- Modify: `platformio.ini` (`z3t0/IRremote@4.4.1` in `[env:ledpanel]` `lib_deps`; `[env:irtest]` uses `lib/irrx` and drops its empty `build_src_flags`)
- Modify: `src/diag/irtest.cpp` (on `irrx`)
- Modify: `include/pins.h` (`PIN_IR = 27` under `BOARD_CYD`, comment "KY-022 on CN1")
- Modify: `src/util/yellow/bothlook.h`, `test/test_bothlook/test_bothlook.cpp`
- Modify: `src/yellow/ledpanel.cpp` (setup, loop, a `// ---- IR remote ----` section beside the PS3 one)

**Interfaces:**
- Consumes: `irkeys::lookup`, `irkeys::action`, `irkeys::Repeat` (Task 2); `bothlook::recolour`, `change(BOTH, …)`, `musicYield`, `push`, `save`, `drawLooks`, `setMessage` (existing).
- Produces:
  - `namespace irrx { void begin(int pin); bool poll(uint16_t* address, uint8_t* command, bool* repeat); }`: `poll` is true once per decoded frame, resuming the receiver itself; UNKNOWN-protocol frames are dropped.
  - `bothlook::paint(int swatch, ledlook::Look* both, ledlook::Look* in, ledlook::Look* out)`: `recolour` plus every look lit, each keeping its `pct`.
  - `static void irTick()` in `ledpanel.cpp`, called every `loop()` pass.

- [ ] **Step 1: Failing test** in `test_bothlook`: `test_paint_turns_lights_on_at_their_levels` (in PINK 60 off, out PINK 40 off → paint RED → both RED, lit, 60 and 40; `both` RED lit).
- [ ] **Step 2:** `tools\run_tests.ps1 test_bothlook` → FAIL. **Step 3:** implement `paint`. **Step 4:** PASS.
- [ ] **Step 5:** `lib/irrx` + `platformio.ini` + `irtest.cpp` on it. `pio run -e irtest` → `[SUCCESS]` with `src/`'s flags back.
- [ ] **Step 6:** Record `[mem]` on the deck for 2 min with the current `main` build first (baseline low water), as the gate's reference.
- [ ] **Step 7:** `irTick`: `irrx::poll` → `irkeys::lookup` (log `[ir] unknown 0x....:0x..` once per code) → `Repeat::accept` → act. Level and Power go through the same paths as `padLevel(step)` / `padLightsPower` without the pad's buzz (factor the shared part out of them, or call them with the pad feel skipped). Colour: `musicYield()`, `bothlook::paint`, push both, `save()`, the message `>RED` (the colour's name, like `padColour`), `drawLooks()`. Log `[ir] <key>` per acted press. `irrx::begin(PIN_IR)` in `setup()` after the display is up.
- [ ] **Step 8:** All tests + `pio run -e ledpanel` → PASS / `[SUCCESS]`. Flash COM16; record 3 min with the owner pressing ☀▲ (held), ☀▼, OFF, ON and four colours. Expected: one `[ir]` line per press, brightness stepping ~4/s while held, `[mem]` low water within ~1 KB of Step 6's.
- [ ] **Step 9:** Commit `feat(yellow): the IR remote sets both lights' level, power and colour`.

### Task 4: The JUMP / FADE effects

**Files:**
- Modify: `src/util/yellow/irkeys.h` (+ test), `src/yellow/ledpanel.cpp`

**Interfaces:**
- Consumes: `elkcmd::effect(uint8_t mode)`, `elkcmd::speed(uint8_t pct)`; the welcome curtain's per-light writes (`ledpanel.cpp` ~320-340) as the pattern.
- Produces: `irkeys::EFFECT_MODE[4]` (JUMP3, JUMP7, FADE3, FADE7) and `irkeys::EFFECT_SPEED`; `static void irEffect(int i)`.

- [ ] **Step 1:** `irEffect(i)`: `musicYield()`, then to each linked light `speed(EFFECT_SPEED)` and `effect(EFFECT_MODE[i])`. On the outside light the second frame goes ≥1 s after the first (its controller drops a write within ~1 s of the last, as the preset fade notes). Show `>JUMP3` etc. The looks are unchanged. A later colour, level or power press ends the effect with its colour frame. Starting values: `EFFECT_SPEED = 50`, `EFFECT_MODE = {0, 1, 2, 3}`, marked TUNE in a comment.
- [ ] **Step 2:** A probe build flag `-D IR_EFFECT_PROBE` (in a local `[env:ledpanel_probe]` extending `ledpanel`): JUMP3 / JUMP7 step a probe mode −1 / +1 (0..212), FADE3 sends it to both lights, logging `[ir] effect mode N`.
- [ ] **Step 3: In the car with the owner** (lights in range): flash the probe, find the modes that look like "jump 3 colours", "jump 7", "fade 3", "fade 7" on each light, and note whether the outside light runs effects at all. Put the four numbers in `EFFECT_MODE`. A test `test_effect_modes_are_distinct` pins them as four distinct values ≤ 212.
- [ ] **Step 4:** Tests + build; flash the normal `ledpanel`; commit `feat(yellow): JUMP and FADE run the lights' own effects`.

If the car visit can't happen yet, ship Steps 1-2 with the TUNE values and leave Step 3 in `docs/BACKLOG.md`.

### Task 5: Black deck: shuffled playlist start

**Files:**
- Modify: `src/util/black/playlists.h/.cpp`, `test/test_playlists/test_playlists.cpp`
- Modify: `src/black/spotify/client.cpp` (`Job::Play` carries `shuffle`), `client.h` (`watchPlay(const char* id, bool shuffle, char* out, size_t n)`), `src/black/net/remote.cpp` (`handlePlaylistPlay` reads `shuffle=1`)

**Interfaces:**
- Produces: `GET /playlists/play?id=<22>&shuffle=1` (signed as today). It answers `200 playing` or the same `why()` texts with 502, 503 or 504. Without `shuffle` it behaves exactly as today (the watch).
  - `bool playlists::readTotal(const char* body, int* total)`: reads `tracks.total` from `GET /v1/playlists/<id>?fields=tracks.total`.
  - `bool playlists::playBodyAt(const char* id, int position, char* out, size_t n)`: `{"context_uri":"spotify:playlist:<id>","offset":{"position":N}}`; `position < 0` gives `playBody`'s body.
  - `int playlists::startPosition(int total, uint32_t random)`: `random % total`, or -1 when `total <= 0`.

- [ ] **Step 1: Failing tests:** `test_read_total` (`{"tracks":{"total":57}}` → 57; no `tracks` → false); `test_play_body_at_a_position` (exact JSON for position 12; position -1 = `playBody`'s); `test_play_position_without_a_total` (`startPosition(0, 7)` → -1, `startPosition(5, 7)` → 2).
- [ ] **Step 2:** `tools\run_tests.ps1 test_playlists` → FAIL. **Step 3:** implement. **Step 4:** PASS.
- [ ] **Step 5:** `runPlay` with shuffle: GET the total (a failure there gives position -1). Then `PUT /v1/me/player/play` with `playBodyAt(id, startPosition(total, esp_random()))`. If that succeeds, `PUT /v1/me/player/shuffle?state=true` (its failure is logged, not reported). Log `[spotify] play playlist <id> shuffled at <pos>: HTTP n`.
- [ ] **Step 6:** `pio run -e s3` → `[SUCCESS]`; all tests pass. **Ask the owner before flashing COM12.** Then call `/playlists/play?id=…&shuffle=1` from the yellow deck (Task 6) or a signed curl, and check Spotify starts mid-playlist with shuffle on.
- [ ] **Step 7:** Commit `feat(black): /playlists/play can start a playlist shuffled`.

### Task 6: ♪1-♪4 on the yellow deck

**Files:**
- Modify: `include/config.example.h` (`IR_PLAYLIST_1..4` and `IR_PLAYLIST_1..4_NAME`, all `""`), `src/config.h` (the owner's, git-ignored)
- Modify: `src/yellow/ledpanel.cpp` (`deckAsk` / `deckGet` take `uint32_t timeoutMs = 1500`; the deck task's playlist ask; `irTick`'s Playlist case)

**Interfaces:**
- Consumes: Task 5's endpoint; `irkeys` `Kind::Playlist` 0..3.
- Produces: `static std::atomic<int8_t> irPlaylistWant{-1}` (loop → deck task, the last press wins); `static std::atomic<int16_t> irPlaylistRc{0}` and `static char irPlaylistWhy[40]` (deck task → loop, the reply text written before the code is stored).

- [ ] **Step 1:** Find the four playlists' IDs: from the Spotify web player (the owner's Chrome, read-only), or the owner pastes the links. The names to match are "Blinded by the Lights", "Divas Pop", the "Hamilton" driving playlist and "Cuddles and Moans" (maybe "Amassos e Gemidos"). **Confirm the four with the owner.** Fill `src/config.h` (never committed) and the placeholders in `config.example.h`.
- [ ] **Step 2:** Playlist key: an empty ID → `>NO PLAYLIST SET`; else `irPlaylistWant.store(i)` and `>STARTING <NAME>`. The deck task, each pass, takes `irPlaylistWant.exchange(-1)`. When it's ≥ 0, it sends `deckGet("/playlists/play", {{"id", ID}, {"shuffle", "1"}}, reply, sizeof reply, nullptr, nullptr, 16000)` (the black deck waits up to 15 s on Spotify). The reply goes into `irPlaylistWhy` and the code into `irPlaylistRc`. `deckAnswered(rc > 0)`. `loop()` shows `>PLAYING <NAME>` on 200, `>BLACK DECK NOT ANSWERING` (ALERT) on rc ≤ 0, else `>` + the reply in capitals (e.g. `>OPEN SPOTIFY ON PHONE`). Log `[ir] playlist N: rc=…`.
- [ ] **Step 3:** All tests + `pio run -e ledpanel`; flash COM16; the owner presses ♪1-♪4 with Spotify open on the phone. Expected: each starts its playlist mid-way with shuffle on. A second ♪ while one is waiting plays only the last. The music / karaoke keep-alives resume after.
- [ ] **Step 4:** Commit `feat(yellow): the remote's music keys start four playlists shuffled`.

### Task 7: Docs and the car check

**Files:** `docs/yellow-deck.md` (a "Remote" section: wiring, the key table, `irtest`), `docs/ARCHITECTURE.md` (the fourth input; `lib/irrx`), `CHANGELOG.md` (Unreleased: yellow deck + black deck lines), `docs/BACKLOG.md` (the car check).

- [ ] **Step 1:** Write the docs; the key table follows the spec's mapping.
- [ ] **Step 2:** BACKLOG, the car check: range and angle from the driver's seat to the mounted remote, sunlight on the receiver, presses during a BLE reconnect / with the PS3 pad connected / during music mode, the effect modes (if Task 4 Step 3 is still open).
- [ ] **Step 3:** All tests, `pio run -e ledpanel -e s3`; commit `docs(yellow): the IR remote`.
